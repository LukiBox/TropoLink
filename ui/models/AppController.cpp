#include "ui/models/AppController.h"

#include "core/geo/coords.h"
#include "core/project/csv_export.h"
#include "core/project/kml_export.h"
#include "core/terrain/horizon.h"
#include "core/tropo/fspl.h"
#include "ui/models/HelpContent.h"
#include "ui/report/PdfReport.h"

#include <gdal.h>
#include <GeographicLib/Constants.hpp>

#ifndef TROPOLINK_AIRGAP
#include "core/ai/ollama_client.h"

#include <zlib.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QThreadPool>

#include <cmath>

using namespace tl;

namespace {

QString dataDirectory() {
    // Deployed: <exe>/data. Development fallback: source tree resources/data.
    const QDir exeData(QCoreApplication::applicationDirPath() + QStringLiteral("/data"));
    if (exeData.exists(QStringLiteral("N050.TXT"))) {
        return exeData.absolutePath();
    }
    return QStringLiteral(TROPOLINK_SOURCE_DATA_DIR);
}

std::string toUtf8(const QString& s) { return s.toStdString(); }

QString localPath(const QUrl& url) {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QVariantMap pointVariant(const geo::GeoPoint& p) {
    return QVariantMap{{QStringLiteral("lat"), p.latitude.value()},
                       {QStringLiteral("lon"), p.longitude.value()}};
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    const QString terrainDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/terrain");
    if (auto store = terrain::TerrainStore::open(toUtf8(terrainDir))) {
        terrainStore_ = std::move(store).value();
    }
    // First run: seed the store with the bundled reference terrain (SRTM-derived
    // DTED-0 over western Poland) so the example project computes over real ground.
    if (terrainStore_ && terrainStore_->entries().empty()) {
        const QDir bundled(dataDirectory() + QStringLiteral("/terrain"));
        for (const auto& info : bundled.entryInfoList({QStringLiteral("*.dt0"), QStringLiteral("*.dt1"),
                                                       QStringLiteral("*.dt2"), QStringLiteral("*.hgt")},
                                                      QDir::Files)) {
            (void)terrainStore_->importFile(toUtf8(info.absoluteFilePath()),
                                            terrain::Provenance::Imported);
        }
    }
    if (auto maps = geo::RefractivityMaps::load(toUtf8(dataDirectory()))) {
        refractivityMaps_ = std::move(maps).value();
    }
    const QString modulationsPath = dataDirectory() + QStringLiteral("/modulations.json");
    if (auto lib = budget::ModulationLibrary::load(toUtf8(modulationsPath))) {
        modulations_ = std::move(lib).value();
    } else {
        modulations_ = budget::ModulationLibrary::builtIn();
    }
    link_.radio.modulation = modulations_.entries()[static_cast<std::size_t>(
        std::min<int>(modulationIndex_, static_cast<int>(modulations_.entries().size()) - 1))];

    loadEquipmentLibrary();

    QSettings settings;
    darkTheme_ = settings.value("ui/darkTheme", true).toBool();
    mapOnlineSource_ = settings.value("ui/mapOnlineSource", QString()).toString();
    mapBasemapPath_ = settings.value("ui/mapBasemapPath", QString()).toString();
    if (!mapBasemapPath_.isEmpty() && !QFile::exists(mapBasemapPath_)) {
        mapBasemapPath_.clear(); // pack moved or deleted since last run
    }

    debounce_.setSingleShot(true);
    debounce_.setInterval(120);
    connect(&debounce_, &QTimer::timeout, this, [this] { recompute(true); });

    // One computation at a time; superseded ones are stop-requested and queued
    // stale jobs exit immediately on the revision check.
    computePool_.setMaxThreadCount(1);

    refreshAutoAtmosphere();
    recompute(true);
}

AppController::~AppController() {
    computeStop_.request_stop();
    computePool_.waitForDone(5000);
    QThreadPool::globalInstance()->waitForDone(2000);
}

// --- setters ----------------------------------------------------------------

void AppController::setSiteALat(double v) {
    link_.siteA.latitude = Degrees(std::clamp(v, -84.0, 84.0));
    emit sitesChanged();
    refreshAutoAtmosphere();
    scheduleRecompute(false);
}
void AppController::setSiteALon(double v) {
    link_.siteA.longitude = Degrees(std::clamp(v, -180.0, 180.0));
    emit sitesChanged();
    refreshAutoAtmosphere();
    scheduleRecompute(false);
}
void AppController::setSiteBLat(double v) {
    link_.siteB.latitude = Degrees(std::clamp(v, -84.0, 84.0));
    emit sitesChanged();
    refreshAutoAtmosphere();
    scheduleRecompute(false);
}
void AppController::setSiteBLon(double v) {
    link_.siteB.longitude = Degrees(std::clamp(v, -180.0, 180.0));
    emit sitesChanged();
    refreshAutoAtmosphere();
    scheduleRecompute(false);
}
void AppController::setSiteAAgl(double v) {
    link_.aglA = Meters(std::clamp(v, 0.5, 500.0));
    emit sitesChanged();
    scheduleRecompute(false);
}
void AppController::setSiteBAgl(double v) {
    link_.aglB = Meters(std::clamp(v, 0.5, 500.0));
    emit sitesChanged();
    scheduleRecompute(false);
}

void AppController::setFrequencyGHz(double v) {
    link_.frequency = Hertz::fromGigahertz(std::clamp(v, 0.1, 20.0));
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::setTxPowerText(const QString& text) {
    // Unit-aware: accepts "500 W", "500W", "57 dBm", "57dBm", "27 dBW", plain number = dBm.
    const QString t = text.trimmed().toLower();
    static const QRegularExpression re(
        QStringLiteral("^\\s*([0-9]+(?:[\\.,][0-9]+)?)\\s*(w|kw|dbm|dbw)?\\s*$"));
    const auto m = re.match(t);
    if (!m.hasMatch()) {
        setStatus(tr("Unrecognized power: %1").arg(text));
        return;
    }
    double value = m.captured(1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
    const QString unit = m.captured(2);
    Dbm dbm{57.0};
    if (unit == QLatin1String("w")) {
        dbm = Watts(value).dbm();
    } else if (unit == QLatin1String("kw")) {
        dbm = Watts(value * 1000.0).dbm();
    } else if (unit == QLatin1String("dbw")) {
        dbm = Dbm(value + 30.0);
    } else {
        dbm = Dbm(value);
    }
    link_.radio.txPower = dbm;
    txPowerText_ = text.trimmed();
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::setGainA(double v) {
    link_.radio.antennaGainA = Dbi(std::clamp(v, 0.0, 70.0));
    emit radioChanged();
    scheduleRecompute(false);
}
void AppController::setGainB(double v) {
    link_.radio.antennaGainB = Dbi(std::clamp(v, 0.0, 70.0));
    emit radioChanged();
    scheduleRecompute(false);
}
void AppController::setLineLossA(double v) {
    link_.radio.lineLossA = Decibels(std::clamp(v, 0.0, 20.0));
    emit radioChanged();
    scheduleRecompute(false);
}
void AppController::setLineLossB(double v) {
    link_.radio.lineLossB = Decibels(std::clamp(v, 0.0, 20.0));
    emit radioChanged();
    scheduleRecompute(false);
}
void AppController::setNoiseFigure(double v) {
    link_.radio.noiseFigure = Decibels(std::clamp(v, 0.0, 20.0));
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::setModulationIndex(int v) {
    if (v < 0 || v >= static_cast<int>(modulations_.entries().size())) {
        return;
    }
    modulationIndex_ = v;
    link_.radio.modulation = modulations_.entries()[static_cast<std::size_t>(v)];
    emit radioChanged();
    scheduleRecompute(false);
}

QStringList AppController::modulationNames() const {
    QStringList names;
    for (const auto& m : modulations_.entries()) {
        names << QString::fromStdString(m.name);
    }
    return names;
}

void AppController::setDataRateMbps(double v) {
    link_.radio.dataRate = BitsPerSecond::fromMegabits(std::clamp(v, 0.001, 1000.0));
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::setAntennaDiameter(double v) {
    link_.antennaDiameter = Meters(std::clamp(v, 0.3, 30.0));
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::setKFactor(double v) {
    link_.atmosphere.kFactor = std::clamp(v, 0.3, 10.0);
    link_.atmosphere.kFactorOverridden = true;
    emit atmosphereChanged();
    scheduleRecompute(false);
}

void AppController::setKFactorAuto(bool v) {
    link_.atmosphere.kFactorOverridden = !v;
    if (v) {
        refreshAutoAtmosphere();
    }
    emit atmosphereChanged();
    scheduleRecompute(false);
}

void AppController::setClimateIndex(int v) {
    link_.atmosphere.climate = static_cast<geo::Climate>(std::clamp(v, 0, 6) + 1);
    emit atmosphereChanged();
    scheduleRecompute(false);
}

QStringList AppController::climateNames() const {
    QStringList names;
    for (int c = 1; c <= 7; ++c) {
        names << QString::fromUtf8(geo::climateName(static_cast<geo::Climate>(c)));
    }
    return names;
}

void AppController::setDiversityIndex(int v) {
    link_.diversity = static_cast<budget::DiversityMode>(std::clamp(v, 0, 4));
    emit targetChanged();
    // Availability re-derives from the existing engine: instant, no full recompute.
    if (outcome_ && outcome_->engine) {
        outcome_->availabilityAnnual =
            outcome_->engine->availability(outcome_->budget.fadeMargin, link_.diversity, false);
        outcome_->availabilityWorstMonth =
            outcome_->engine->availability(outcome_->budget.fadeMargin, link_.diversity, true);
        deliverOutcome(outcome_);
    }
}

int AppController::primaryModelIndex() const {
    switch (link_.primaryModel) {
    case tropo::ModelId::P617:
        return 0;
    case tropo::ModelId::Tn101:
        return 1;
    case tropo::ModelId::Itm:
        return 2;
    default:
        return 0;
    }
}

void AppController::setPrimaryModelIndex(int v) {
    link_.primaryModel = v == 1   ? tropo::ModelId::Tn101
                         : v == 2 ? tropo::ModelId::Itm
                                  : tropo::ModelId::P617;
    emit targetChanged();
    scheduleRecompute(true);
}

void AppController::setTargetAvailability(double v) {
    link_.targetAvailability = Percent(std::clamp(v, 50.0, 99.9999));
    emit targetChanged();
    emit resultsChanged();
}

void AppController::setTargetWorstMonth(bool v) {
    link_.targetIsWorstMonth = v;
    emit targetChanged();
    emit resultsChanged();
}

void AppController::setLanguageCode(const QString& code) {
    if (languageCode_ == code) {
        return;
    }
    setLanguageCodeTransient(code);
    QSettings settings;
    settings.setValue("ui/language", code);
}

void AppController::setLanguageCodeTransient(const QString& code) {
    if (languageCode_ == code) {
        return;
    }
    languageCode_ = code;
    emit languageChanged();
}

void AppController::setDarkTheme(bool v) {
    if (darkTheme_ == v) {
        return;
    }
    darkTheme_ = v;
    QSettings settings;
    settings.setValue("ui/darkTheme", v);
    emit themeChanged();
}

QVariantList AppController::terrainEntries() const {
    QVariantList list;
    if (!terrainStore_) {
        return list;
    }
    for (const auto& e : terrainStore_->entries()) {
        list << QVariantMap{{"file", QString::fromStdString(e.fileName)},
                            {"format", QString::fromStdString(e.format)},
                            {"resolution", e.resolutionM},
                            {"downloaded", e.provenance == terrain::Provenance::Downloaded},
                            {"minLat", e.bounds.minLat},
                            {"maxLat", e.bounds.maxLat},
                            {"minLon", e.bounds.minLon},
                            {"maxLon", e.bounds.maxLon}};
    }
    return list;
}

// --- invokables --------------------------------------------------------------

void AppController::setSiteFromMap(int site, double lat, double lon, bool preview) {
    if (site == 0) {
        link_.siteA = geo::GeoPoint{Degrees(lat), Degrees(lon)};
    } else {
        link_.siteB = geo::GeoPoint{Degrees(lat), Degrees(lon)};
    }
    emit sitesChanged();
    if (!preview) {
        refreshAutoAtmosphere();
    }
    scheduleRecompute(!preview);
}

bool AppController::parseCoordinateToSite(int site, const QString& text) {
    const auto parsed = geo::Coords::parse(toUtf8(text));
    if (!parsed) {
        setStatus(tr("Unrecognized coordinate: %1").arg(text));
        return false;
    }
    setSiteFromMap(site, parsed.value().latitude.value(), parsed.value().longitude.value(), false);
    return true;
}

QVariantMap AppController::parseCoordinate(const QString& text) const {
    const auto parsed = geo::Coords::parse(toUtf8(text));
    if (!parsed) {
        return QVariantMap{{"ok", false}};
    }
    return QVariantMap{{"ok", true},
                       {"lat", parsed.value().latitude.value()},
                       {"lon", parsed.value().longitude.value()}};
}

QVariantMap AppController::coordinateFormats(double lat, double lon) const {
    const geo::GeoPoint p{Degrees(lat), Degrees(lon)};
    QVariantMap map;
    map["dd"] = QString::fromStdString(geo::Coords::formatDecimalDegrees(p));
    map["dms"] = QString::fromUtf8(geo::Coords::formatDms(p));
    const auto mgrs = geo::Coords::formatMgrs(p);
    map["mgrs"] = mgrs ? QString::fromStdString(mgrs.value()) : QStringLiteral("-");
    const auto utm = geo::Coords::formatUtm(p);
    map["utm"] = utm ? QString::fromStdString(utm.value()) : QStringLiteral("-");
    return map;
}

QString AppController::measure(double lat1, double lon1, double lat2, double lon2) const {
    const auto inv = geo::Geodesy::inverse(geo::GeoPoint{Degrees(lat1), Degrees(lon1)},
                                           geo::GeoPoint{Degrees(lat2), Degrees(lon2)});
    return tr("%1 km  az %2\xC2\xB0")
        .arg(inv.distance.kilometers(), 0, 'f', 2)
        .arg(inv.forwardAzimuth.value(), 0, 'f', 1);
}

void AppController::importTerrainFiles(const QList<QUrl>& urls) {
    if (!terrainStore_) {
        return;
    }
    int imported = 0;
    for (const auto& url : urls) {
        const auto result =
            terrainStore_->importFile(toUtf8(localPath(url)), terrain::Provenance::Imported);
        if (result) {
            ++imported;
        } else {
            setStatus(QString::fromStdString(result.error().message));
        }
    }
    if (imported > 0) {
        setStatus(tr("Imported %1 terrain file(s)").arg(imported));
        emit terrainChanged();
        scheduleRecompute(true);
    }
}

void AppController::removeTerrainEntry(const QString& fileName) {
    if (!terrainStore_) {
        return;
    }
    (void)terrainStore_->removeEntry(toUtf8(fileName));
    emit terrainChanged();
    scheduleRecompute(true);
}

void AppController::scheduleRecompute(bool immediate) {
    if (immediate) {
        debounce_.stop();
        recompute(true);
    } else if (!debounce_.isActive()) {
        debounce_.start();
    }
}

void AppController::recompute(bool immediate) {
    Q_UNUSED(immediate);
    const quint64 rev = ++revision_;
    if (!busy_) {
        busy_ = true;
        emit busyChanged();
    }
    // Cancel the running computation (if any) and hand the new state to the
    // serialized compute pool. Queued stale jobs exit on the revision check.
    computeStop_.request_stop();
    computeStop_ = std::stop_source{};
    LinkState state = link_;
    std::stop_token token = computeStop_.get_token();
    computePool_.start([this, rev, state, token] { runComputation(rev, state, token); });
}

void AppController::runComputation(quint64 rev, LinkState state, std::stop_token stopToken) {
    if (revision_.load() != rev || stopToken.stop_requested()) {
        return; // superseded while queued
    }
    QElapsedTimer timer;
    timer.start();
    auto outcome = std::make_shared<ComputeOutcome>();
    outcome->revision = rev;

    // 1) Terrain profile (multithreaded inside, cancellable).
    terrain::ProfileRequest preq;
    preq.siteA = state.siteA;
    preq.siteB = state.siteB;
    if (terrainStore_) {
        if (auto profile = terrain::extractProfile(*terrainStore_, preq, stopToken)) {
            outcome->profile = std::move(profile).value();
        }
    }
    if (stopToken.stop_requested()) {
        return;
    }
    if (outcome->profile.points.empty()) {
        // No store or co-located sites: synthesize a smooth sea-level profile so the
        // geometry stays defined; models will flag the missing terrain.
        const auto inv = geo::Geodesy::inverse(state.siteA, state.siteB);
        const int n = 512;
        outcome->profile.totalDistance = inv.distance;
        outcome->profile.step = Meters(inv.distance.value() / (n - 1));
        outcome->profile.hasCoverage = false;
        const auto line = geo::Geodesy::sampleLine(state.siteA, state.siteB, n);
        for (int i = 0; i < n; ++i) {
            terrain::ProfilePoint pt;
            pt.distance = line[static_cast<std::size_t>(i)].distanceFromStart;
            pt.position = line[static_cast<std::size_t>(i)].point;
            pt.elevation = Meters(0.0);
            outcome->profile.points.push_back(pt);
        }
    }
    if (revision_.load() != rev) {
        return; // superseded while extracting
    }

    // 2) Model suite.
    tropo::SuiteInput sin;
    sin.siteA = state.siteA;
    sin.siteB = state.siteB;
    sin.antennaAglA = state.aglA;
    sin.antennaAglB = state.aglB;
    sin.frequency = state.frequency;
    sin.gainA = state.radio.antennaGainA;
    sin.gainB = state.radio.antennaGainB;
    sin.atmosphere = state.atmosphere;
    outcome->suite = tropo::runSuite(sin, outcome->profile, state.primaryModel);

    // 3) Budget + availability from the primary model.
    const auto primary = outcome->suite.primaryModel();
    const Decibels medianLoss = primary != nullptr && primary->validity().valid
                                    ? primary->medianLoss()
                                    : outcome->suite.fspl;
    outcome->budget = budget::computeLinkBudget(state.radio, medianLoss);
    if (primary != nullptr && primary->validity().valid) {
        outcome->engine = std::make_shared<budget::AvailabilityEngine>(*primary);
        outcome->availabilityAnnual =
            outcome->engine->availability(outcome->budget.fadeMargin, state.diversity, false);
        outcome->availabilityWorstMonth =
            outcome->engine->availability(outcome->budget.fadeMargin, state.diversity, true);
    }
    outcome->separation = budget::diversitySeparation(state.antennaDiameter, state.frequency,
                                                      outcome->suite.geometry.scatterAngle,
                                                      outcome->suite.inverse.distance);
    outcome->computeMs = static_cast<double>(timer.elapsed());

    if (revision_.load() != rev) {
        return;
    }
    QMetaObject::invokeMethod(
        this, [this, outcome] { deliverOutcome(outcome); }, Qt::QueuedConnection);
}

void AppController::deliverOutcome(std::shared_ptr<ComputeOutcome> outcome) {
    if (outcome->revision != revision_.load() && outcome != outcome_) {
        return; // stale
    }
    outcome_ = outcome;
    const auto& suite = outcome->suite;

    geometry_ = QVariantMap{
        {"distanceKm", suite.inverse.distance.kilometers()},
        {"azimuthAB", suite.inverse.forwardAzimuth.value()},
        {"azimuthBA", suite.inverse.reverseAzimuth.value()},
        {"takeoffAMrad", suite.horizons.takeoffAngleA.milliradians()},
        {"takeoffBMrad", suite.horizons.takeoffAngleB.milliradians()},
        {"thetaMrad", suite.geometry.scatterAngle.milliradians()},
        {"thetaDeg", suite.geometry.scatterAngle.degrees()},
        {"cvBaseM", suite.geometry.volumeBaseAmsl.value()},
        {"cvTopM", suite.geometry.volumeTopAmsl.value()},
        {"cvAboveTerrainM", suite.commonVolumeAboveTerrain.value()},
        {"cvDistanceAKm", suite.geometry.distanceToVolumeA.kilometers()},
        {"slantAKm", suite.geometry.slantRangeA.kilometers()},
        {"slantBKm", suite.geometry.slantRangeB.kilometers()},
        {"directObstructed", suite.horizons.directPathObstructed},
        {"fresnelClear", suite.horizons.fresnelZoneClear},
        {"fsplDb", suite.fspl.value()},
    };

    modelRows_.clear();
    for (const auto& row : suite.rows) {
        QStringList issues;
        for (const auto& s : row.validityIssues) {
            issues << QString::fromStdString(s);
        }
        QVariantList annual;
        QVariantList worstMonth;
        for (std::size_t i = 0; i < row.annualDb.size(); ++i) {
            annual << row.annualDb[i];
            worstMonth << row.worstMonthDb[i];
        }
        modelRows_ << QVariantMap{{"name", QString::fromStdString(row.name)},
                                  {"citation", QString::fromStdString(row.citation)},
                                  {"valid", row.valid},
                                  {"issues", issues.join(QStringLiteral("; "))},
                                  {"medianDb", row.median.value()},
                                  {"couplingDb", row.couplingLoss.value()},
                                  {"note", QString::fromStdString(row.note)},
                                  {"annual", annual},
                                  {"worstMonth", worstMonth},
                                  {"isPrimary", row.id == link_.primaryModel},
                                  {"isFspl", row.id == tropo::ModelId::Fspl}};
    }
    spreadDb_ = suite.spread.value();

    waterfall_.clear();
    for (const auto& item : outcome->budget.waterfall) {
        waterfall_ << QVariantMap{{"key", QString::fromStdString(item.label)},
                                  {"value", item.valueDb},
                                  {"isLevel", item.isLevel}};
    }
    budget_ = QVariantMap{{"eirp", outcome->budget.eirp.value()},
                          {"pathLoss", outcome->budget.pathLoss.value()},
                          {"rsl", outcome->budget.medianRsl.value()},
                          {"noise", outcome->budget.noiseFloor.value()},
                          {"snr", outcome->budget.medianSnr.value()},
                          {"requiredSnr", outcome->budget.requiredSnr.value()},
                          {"margin", outcome->budget.fadeMargin.value()}};

    availability_ = QVariantMap{
        {"annual", outcome->availabilityAnnual.value()},
        {"worstMonth", outcome->availabilityWorstMonth.value()},
        {"sepH", outcome->separation.horizontal.value()},
        {"sepV", outcome->separation.vertical.value()},
        {"sepFMhz", outcome->separation.frequencySeparationMhz},
        {"angleSepMrad", outcome->separation.angleSeparation.milliradians()},
        {"hasEngine", outcome->engine != nullptr},
    };
    if (outcome->engine) {
        availability_["divGain"] =
            outcome->engine->diversityGain(link_.targetAvailability, link_.diversity,
                                           link_.targetIsWorstMonth)
                .value();
        availability_["requiredMargin"] =
            outcome->engine
                ->marginForAvailability(link_.targetAvailability, link_.diversity, link_.targetIsWorstMonth)
                .value();
    }

    // Map overlays.
    pathPolyline_.clear();
    const auto samples = geo::Geodesy::sampleLine(link_.siteA, link_.siteB, 129);
    for (const auto& s : samples) {
        pathPolyline_ << pointVariant(s.point);
    }
    commonVolume_ = pointVariant(suite.commonVolumePosition);
    commonVolume_["baseM"] = suite.geometry.volumeBaseAmsl.value();

    auto buildFan = [](const geo::GeoPoint& site, double centerAzDeg, Meters radius) {
        QVariantList fan;
        fan << pointVariant(site);
        for (int i = -5; i <= 5; ++i) {
            const double az = centerAzDeg + i * 5.0;
            fan << pointVariant(geo::Geodesy::direct(site, Degrees(az), radius));
        }
        return fan;
    };
    const Meters fanA = Meters(std::max(5000.0, suite.horizons.horizonDistanceA.value()));
    const Meters fanB = Meters(std::max(5000.0, suite.horizons.horizonDistanceB.value()));
    horizonFanA_ = buildFan(link_.siteA, suite.inverse.forwardAzimuth.value(), fanA);
    horizonFanB_ = buildFan(link_.siteB, suite.inverse.reverseAzimuth.value(), fanB);

    // Profile draw data in display space: adding the effective-earth bulge to every
    // height makes the horizon rays straight lines (see ProfileItem).
    const double d = suite.inverse.distance.value();
    const double ka = link_.atmosphere.kFactor * geo::Atmosphere::kEarthRadiusKm * 1000.0;
    auto bulge = [d, ka](double x) { return x * (d - x) / (2.0 * ka); };
    profileTerrain_.clear();
    profileVoidSpans_.clear();
    double minY = 1e18;
    double maxY = -1e18;
    double voidStart = -1.0;
    for (const auto& pt : outcome->profile.points) {
        const double x = pt.distance.value();
        const double y = pt.elevation.value() + bulge(x);
        profileTerrain_ << QPointF(x, y);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        if (pt.interpolatedVoid && voidStart < 0.0) {
            voidStart = x;
        } else if (!pt.interpolatedVoid && voidStart >= 0.0) {
            profileVoidSpans_ << QVariantMap{{"from", voidStart}, {"to", x}};
            voidStart = -1.0;
        }
    }
    if (voidStart >= 0.0) {
        profileVoidSpans_ << QVariantMap{{"from", voidStart}, {"to", d}};
    }

    const double hA =
        (outcome->profile.points.empty() ? 0.0 : outcome->profile.points.front().elevation.value()) +
        link_.aglA.value();
    const double hB =
        (outcome->profile.points.empty() ? 0.0 : outcome->profile.points.back().elevation.value()) +
        link_.aglB.value();
    const double tauA = suite.horizons.takeoffAngleA.value();
    const double tauB = suite.horizons.takeoffAngleB.value();
    // Display-space rays are straight: y(x) = h + tau x + x d/(2ka) from A,
    // y(x) = h + tau (d-x) + (d-x) d/(2ka) from B.
    auto rayFromA = [&](double x, double tau) { return hA + tau * x + x * d / (2.0 * ka); };
    auto rayFromB = [&](double x, double tau) {
        const double xr = d - x;
        return hB + tau * xr + xr * d / (2.0 * ka);
    };
    const double xTop = suite.geometry.distanceToVolumeA.value();
    const double lensTopY = suite.geometry.volumeTopAmsl.value() + bulge(xTop);
    profileRayA_ = {QPointF(0.0, hA), QPointF(d, rayFromA(d, tauA))};
    profileRayB_ = {QPointF(d, hB), QPointF(0.0, rayFromB(0.0, tauB))};
    maxY = std::max({maxY, lensTopY, hA, hB});

    // Lens polygon: region above both horizon rays, below both upper beam edges.
    profileLens_.clear();
    {
        const double bwA = suite.geometry.halfPowerBeamwidthA.value() * 0.5;
        const double bwB = suite.geometry.halfPowerBeamwidthB.value() * 0.5;
        const int n = 96;
        QPolygonF lower;
        QPolygonF upper;
        for (int i = 0; i <= n; ++i) {
            const double x = d * i / n;
            const double lo = std::max(rayFromA(x, tauA), rayFromB(x, tauB));
            const double hi = std::min(rayFromA(x, tauA + bwA), rayFromB(x, tauB + bwB));
            if (hi > lo) {
                lower << QPointF(x, lo);
                upper.prepend(QPointF(x, hi));
            }
        }
        profileLens_ = lower + upper;
    }

    profileDirectRay_ = {QPointF(0.0, hA + bulge(0.0)), QPointF(d, hB + bulge(d))};
    // Direct-ray Fresnel zone (first): ellipse envelope around the display-space chord.
    profileFresnelLower_.clear();
    profileFresnelUpper_.clear();
    {
        const int n = 96;
        for (int i = 0; i <= n; ++i) {
            const double x = d * i / n;
            const double chordY = hA + (hB - hA) * (d > 0 ? x / d : 0.0) + bulge(x);
            const double r = terrain::fresnelRadius1(Meters(x), Meters(d), link_.frequency).value();
            profileFresnelLower_ << QPointF(x, chordY - r);
            profileFresnelUpper_ << QPointF(x, chordY + r);
        }
    }

    profileMeta_ = QVariantMap{{"distanceM", d},
                               {"minY", minY},
                               {"maxY", maxY},
                               {"antennaA", hA},
                               {"antennaB", hB},
                               {"aglA", link_.aglA.value()},
                               {"aglB", link_.aglB.value()},
                               {"lensBaseY",
                                suite.geometry.volumeBaseAmsl.value() + bulge(xTop)},
                               {"lensTopY", lensTopY},
                               {"lensX", xTop},
                               {"thetaMrad", suite.geometry.scatterAngle.milliradians()}};

    computeTimeMs_ = outcome->computeMs;
    terrainCovered_ = outcome->profile.hasCoverage;
    profileHasVoids_ = outcome->profile.hasVoids;
    busy_ = false;
    emit busyChanged();
    emit resultsChanged();
}

QVariantMap AppController::solveFor(int what) {
    QVariantMap out{{"ok", false}};
    if (!outcome_ || !outcome_->engine) {
        out["note"] = tr("No valid primary model");
        return out;
    }
    budget::SolverRequest req;
    req.solveFor = what == 1   ? budget::SolveFor::AntennaGain
                   : what == 2 ? budget::SolveFor::DataRate
                               : budget::SolveFor::TxPower;
    req.targetAvailability = link_.targetAvailability;
    req.worstMonth = link_.targetIsWorstMonth;
    req.diversity = link_.diversity;
    const budget::DesignSolver solver(*outcome_->engine, link_.radio, outcome_->budget.pathLoss);
    const auto result = solver.solve(req);
    out["ok"] = result.feasible;
    out["note"] = QString::fromStdString(result.note);
    out["requiredMargin"] = result.requiredMedianMargin.value();
    out["currentMargin"] = result.currentMedianMargin.value();
    if (result.requiredTxPower) {
        out["txDbm"] = result.requiredTxPower->value();
        out["txWatts"] = result.requiredTxPower->watts();
    }
    if (result.requiredAntennaGain) {
        out["gainDbi"] = result.requiredAntennaGain->value();
    }
    if (result.maxDataRate) {
        out["rateMbps"] = result.maxDataRate->megabits();
    }
    return out;
}

QVariantList AppController::availabilityCurve(bool worstMonth) const {
    QVariantList list;
    if (!outcome_ || !outcome_->engine) {
        return list;
    }
    for (const auto& pt : outcome_->engine->curve(link_.diversity, worstMonth)) {
        list << QVariantMap{{"margin", pt.marginDb}, {"availability", pt.availabilityPercent}};
    }
    return list;
}

bool AppController::exportKml(const QUrl& url) {
    if (!outcome_) {
        return false;
    }
    const auto project = currentProject();
    const auto status =
        tl::project::exportKml(project, project.links.front(), outcome_->suite, toUtf8(localPath(url)));
    setStatus(status ? tr("KML exported") : QString::fromStdString(status.error().message));
    return static_cast<bool>(status);
}

bool AppController::exportProfileCsv(const QUrl& url) {
    if (!outcome_) {
        return false;
    }
    const auto status =
        tl::project::exportCsv(tl::project::profileCsv(outcome_->profile), toUtf8(localPath(url)));
    setStatus(status ? tr("Profile CSV exported") : QString::fromStdString(status.error().message));
    return static_cast<bool>(status);
}

bool AppController::exportBudgetCsv(const QUrl& url) {
    if (!outcome_) {
        return false;
    }
    const auto status = tl::project::exportCsv(
        tl::project::budgetCsv(outcome_->budget, outcome_->suite), toUtf8(localPath(url)));
    setStatus(status ? tr("Budget CSV exported") : QString::fromStdString(status.error().message));
    return static_cast<bool>(status);
}

tl::project::Project AppController::currentProject() const {
    tl::project::Project p;
    p.name = projectName_.toStdString();
    p.id = "tlk-current";
    tl::project::Site a;
    a.id = "site-a";
    a.name = "Site A";
    a.position = link_.siteA;
    a.antennaHeightAgl = link_.aglA;
    tl::project::Site b;
    b.id = "site-b";
    b.name = "Site B";
    b.position = link_.siteB;
    b.antennaHeightAgl = link_.aglB;
    p.sites = {a, b};
    tl::project::LinkSpec l;
    l.id = "link-1";
    l.name = "A-B";
    l.siteAId = "site-a";
    l.siteBId = "site-b";
    l.frequency = link_.frequency;
    l.radio = link_.radio;
    l.atmosphere = link_.atmosphere;
    l.diversity = link_.diversity;
    l.primaryModel = link_.primaryModel;
    l.targetAvailability = link_.targetAvailability;
    l.targetIsWorstMonth = link_.targetIsWorstMonth;
    l.antennaDiameter = link_.antennaDiameter;
    p.links = {l};
    if (outcome_) {
        tl::project::ResultSnapshot snap;
        snap.linkId = "link-1";
        snap.createdIso8601 =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
        snap.appVersion = TROPOLINK_VERSION;
        snap.gdalVersion = GDALVersionInfo("RELEASE_NAME");
        snap.geographicLibVersion = GEOGRAPHICLIB_VERSION_STRING;
        for (const auto& row : outcome_->suite.rows) {
            snap.modelVersions[row.name] = row.citation;
        }
        snap.values["distance_km"] = outcome_->suite.inverse.distance.kilometers();
        snap.values["theta_mrad"] = outcome_->suite.geometry.scatterAngle.milliradians();
        snap.values["fspl_db"] = outcome_->suite.fspl.value();
        snap.values["margin_db"] = outcome_->budget.fadeMargin.value();
        snap.values["availability_annual_pct"] = outcome_->availabilityAnnual.value();
        snap.reportContentSha256 = lastReportHash_.toStdString();
        p.snapshots.push_back(std::move(snap));
    }
    return p;
}

bool AppController::saveProjectFile(const QUrl& url) {
    const auto status = tl::project::saveProject(currentProject(), toUtf8(localPath(url)));
    setStatus(status ? tr("Project saved") : QString::fromStdString(status.error().message));
    return static_cast<bool>(status);
}

void AppController::applyProject(const tl::project::Project& project) {
    projectName_ = QString::fromStdString(project.name);
    if (project.sites.size() >= 2 && !project.links.empty()) {
        const auto& l = project.links.front();
        const auto* a = project.findSite(l.siteAId);
        const auto* b = project.findSite(l.siteBId);
        if (a != nullptr && b != nullptr) {
            link_.siteA = a->position;
            link_.siteB = b->position;
            link_.aglA = a->antennaHeightAgl;
            link_.aglB = b->antennaHeightAgl;
        }
        link_.frequency = l.frequency;
        link_.radio = l.radio;
        link_.atmosphere = l.atmosphere;
        link_.diversity = l.diversity;
        link_.primaryModel = l.primaryModel;
        link_.targetAvailability = l.targetAvailability;
        link_.targetIsWorstMonth = l.targetIsWorstMonth;
        link_.antennaDiameter = l.antennaDiameter;
        txPowerText_ = QStringLiteral("%1 dBm").arg(l.radio.txPower.value(), 0, 'f', 1);
    }
    emit sitesChanged();
    emit radioChanged();
    emit atmosphereChanged();
    emit targetChanged();
    refreshAutoAtmosphere();
    scheduleRecompute(true);
}

bool AppController::loadProjectFile(const QUrl& url) {
    auto loaded = tl::project::loadProject(toUtf8(localPath(url)));
    if (!loaded) {
        setStatus(QString::fromStdString(loaded.error().message));
        return false;
    }
    applyProject(loaded.value());
    setStatus(tr("Project loaded"));
    return true;
}

void AppController::loadReferenceProject() { applyProject(tl::project::referenceProject()); }

bool AppController::generateReport(const QUrl& url, const QImage& mapSnapshot) {
    if (!outcome_ || !outcome_->engine) {
        setStatus(tr("No results to report"));
        return false;
    }
    setStatus(tr("Generating report..."));
    // Everything the worker needs is copied: no shared mutable state.
    auto project = std::make_shared<tl::project::Project>(currentProject());
    auto outcome = outcome_;
    ReportRenderInputs inputs;
    inputs.diversity = link_.diversity;
    inputs.language = languageCode_ == QLatin1String("pl") ? tl::report::Language::Polish
                                                           : tl::report::Language::English;
    inputs.terrainSources =
        terrainStore_ ? terrainStore_->entries() : std::vector<tl::terrain::StoreEntry>{};
    inputs.mapSnapshot = mapSnapshot;
    inputs.terrain = profileTerrain_;
    inputs.rayA = profileRayA_;
    inputs.rayB = profileRayB_;
    inputs.lens = profileLens_;
    inputs.directRay = profileDirectRay_;
    inputs.profileMeta = profileMeta_;
    const QString path = localPath(url);
    const bool wantCommentary = aiCommentaryEnabled_;

    QThreadPool::globalInstance()->start([this, inputs, project, outcome, path, wantCommentary]() mutable {
#ifndef TROPOLINK_AIRGAP
        if (wantCommentary && tl::ai::ollamaAvailable()) {
            tl::ai::OllamaRequest req;
            req.prompt =
                QStringLiteral(
                    "You are assisting a troposcatter link designer. In one short paragraph (%1), "
                    "assess this design plainly: distance %2 km, frequency %3 GHz, median fade margin "
                    "%4 dB, annual availability %5%%, model spread %6 dB. No marketing language.")
                    .arg(inputs.language == tl::report::Language::Polish ? "in Polish" : "in English")
                    .arg(outcome->suite.inverse.distance.kilometers(), 0, 'f', 1)
                    .arg(project->links.front().frequency.gigahertz(), 0, 'f', 2)
                    .arg(outcome->budget.fadeMargin.value(), 0, 'f', 1)
                    .arg(outcome->availabilityAnnual.value(), 0, 'f', 3)
                    .arg(outcome->suite.spread.value(), 0, 'f', 1)
                    .toStdString();
            if (auto commentary = tl::ai::generateCommentary(req)) {
                inputs.aiCommentary = QString::fromStdString(commentary.value());
            }
        }
#else
        Q_UNUSED(wantCommentary);
#endif
        inputs.project = project.get();
        inputs.outcome = outcome.get();
        const auto result = renderPdfReport(inputs, path);
        QMetaObject::invokeMethod(
            this,
            [this, result, path] {
                if (result.ok) {
                    lastReportHash_ = result.contentSha256;
                    setStatus(tr("Report written: %1").arg(path));
                    emit reportGenerated(path);
                } else {
                    setStatus(result.error);
                }
            },
            Qt::QueuedConnection);
    });
    return true;
}

QString AppController::provenance(const QString& key) const {
    static const QHash<QString, QString> table = {
        {"distance", QStringLiteral("GeographicLib (Karney 2013), WGS-84 geodesic inverse")},
        {"azimuth", QStringLiteral("GeographicLib (Karney 2013), WGS-84 geodesic inverse")},
        {"takeoff", QStringLiteral("Horizon scan over DEM profile, TN101 §6 convention")},
        {"theta", QStringLiteral("ITU-R P.617-5 eq. (1)-(2): \xCE\xB8 = \xCE\xB8t + \xCE\xB8r + d/(ka)")},
        {"cv", QStringLiteral("ITU-R P.617-5 eq. (7a), flat-earth ray-crossover form")},
        {"fspl", QStringLiteral("ITU-R P.525-4: 32.45 + 20log f(MHz) + 20log d(km)")},
        {"p617", QStringLiteral("ITU-R P.617-5 §4.1 eq. (4)-(6), N0/dN from P.452 digital maps")},
        {"tn101", QStringLiteral("NBS TN101 Ch. 9-10, computerized per ITS Algorithm §4.63/5/6.9/6.13")},
        {"itm", QStringLiteral("NTIA ITM v1.x P2P, official public-domain implementation")},
        {"coupling", QStringLiteral("ITU-R P.617-5 eq. (3): Lc = 0.07 exp[0.055(Gt+Gr)]")},
        {"noise", QStringLiteral("kTB at 290 K: -174 dBm/Hz + 10log B + NF")},
        {"worstmonth", QStringLiteral("ITU-R P.841-5, trans-horizon Q1 = 5.8 - 0.03 exp(Ns/75)")},
        {"availability", QStringLiteral("Log-normal hourly medians (model) x Rayleigh within-hour, "
                                        "selection-combining diversity")},
        {"separation", QStringLiteral("ITU-R P.617-5 §7 eq. (44)-(47)")},
    };
    return table.value(key, QStringLiteral(""));
}

QVariantList AppController::helpTopics() const {
    return tropolinkHelpTopics(languageCode_ == QLatin1String("pl"));
}

QString AppController::helpHtml(const QString& topic) const {
    return tropolinkHelpHtml(topic, languageCode_ == QLatin1String("pl"));
}

void AppController::setMapOnlineSource(const QString& id) {
    if (mapOnlineSource_ == id) {
        return;
    }
    mapOnlineSource_ = id;
    QSettings settings;
    settings.setValue("ui/mapOnlineSource", id);
    emit mapSettingsChanged();
}

void AppController::setMapBasemapPath(const QString& path) {
    if (mapBasemapPath_ == path) {
        return;
    }
    mapBasemapPath_ = path;
    QSettings settings;
    settings.setValue("ui/mapBasemapPath", path);
    emit mapSettingsChanged();
}

void AppController::refreshAutoAtmosphere() {
    if (!refractivityMaps_) {
        return;
    }
    const geo::GeoPoint mid{Degrees((siteALat() + siteBLat()) / 2.0),
                            Degrees((siteALon() + siteBLon()) / 2.0)};
    const auto autoAtm = refractivityMaps_->atmosphereAt(mid);
    link_.atmosphere.seaLevelN0 = autoAtm.seaLevelN0;
    link_.atmosphere.lapseRateDn = autoAtm.lapseRateDn;
    if (!link_.atmosphere.kFactorOverridden) {
        link_.atmosphere.kFactor = autoAtm.kFactor;
    }
    emit atmosphereChanged();
}

void AppController::setStatus(const QString& message) {
    statusMessage_ = message;
    emit statusMessageChanged();
}

void AppController::loadEquipmentLibrary() {
    QFile file(dataDirectory() + QStringLiteral("/equipment.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    for (const auto& v : doc.object().value(QStringLiteral("antennas")).toArray()) {
        const auto o = v.toObject();
        antennaPresets_.append({o.value(QStringLiteral("name")).toString(),
                                o.value(QStringLiteral("gainDbi")).toDouble(39.1),
                                o.value(QStringLiteral("diameterM")).toDouble(3.0)});
    }
    for (const auto& v : doc.object().value(QStringLiteral("radios")).toArray()) {
        const auto o = v.toObject();
        radioPresets_.append({o.value(QStringLiteral("name")).toString(),
                              o.value(QStringLiteral("txPowerDbm")).toDouble(57.0),
                              o.value(QStringLiteral("noiseFigureDb")).toDouble(4.0)});
    }
}

QStringList AppController::antennaPresets() const {
    QStringList names;
    for (const auto& a : antennaPresets_) {
        names << QStringLiteral("%1 (%2 dBi)").arg(a.name).arg(a.gainDbi, 0, 'f', 1);
    }
    return names;
}

QStringList AppController::radioPresets() const {
    QStringList names;
    for (const auto& r : radioPresets_) {
        names << QStringLiteral("%1 (%2 dBm)").arg(r.name).arg(r.txPowerDbm, 0, 'f', 0);
    }
    return names;
}

void AppController::applyAntennaPreset(int index) {
    if (index < 0 || index >= antennaPresets_.size()) {
        return;
    }
    const auto& a = antennaPresets_.at(index);
    link_.radio.antennaGainA = Dbi(a.gainDbi);
    link_.radio.antennaGainB = Dbi(a.gainDbi);
    link_.antennaDiameter = Meters(a.diameterM);
    emit radioChanged();
    scheduleRecompute(false);
}

void AppController::applyRadioPreset(int index) {
    if (index < 0 || index >= radioPresets_.size()) {
        return;
    }
    const auto& r = radioPresets_.at(index);
    link_.radio.txPower = Dbm(r.txPowerDbm);
    link_.radio.noiseFigure = Decibels(r.noiseFigureDb);
    txPowerText_ = QStringLiteral("%1 dBm").arg(r.txPowerDbm, 0, 'f', 1);
    emit radioChanged();
    scheduleRecompute(false);
}

// --- optional SRTM downloader (compiled out of the Air-Gap flavor entirely) ---

#ifndef TROPOLINK_AIRGAP

namespace {

QByteArray gunzip(const QByteArray& compressed) {
    QByteArray out;
    z_stream stream{};
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        return out;
    }
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    char buffer[1 << 16];
    int rc = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        stream.avail_out = sizeof(buffer);
        rc = inflate(&stream, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            out.clear();
            break;
        }
        out.append(buffer, static_cast<int>(sizeof(buffer) - stream.avail_out));
    } while (rc != Z_STREAM_END);
    inflateEnd(&stream);
    return out;
}

QString srtmTileName(int lat, int lon) {
    return QStringLiteral("%1%2%3%4")
        .arg(lat >= 0 ? QStringLiteral("N") : QStringLiteral("S"))
        .arg(std::abs(lat), 2, 10, QLatin1Char('0'))
        .arg(lon >= 0 ? QStringLiteral("E") : QStringLiteral("W"))
        .arg(std::abs(lon), 3, 10, QLatin1Char('0'));
}

} // namespace

void AppController::downloadSrtmForRegion(double minLat, double maxLat, double minLon, double maxLon) {
    if (!terrainStore_ || downloading_) {
        return;
    }
    struct Job {
        QList<QPair<int, int>> tiles;
        int index = 0;
        int imported = 0;
    };
    auto job = std::make_shared<Job>();
    for (int lat = static_cast<int>(std::floor(minLat)); lat <= static_cast<int>(std::floor(maxLat));
         ++lat) {
        for (int lon = static_cast<int>(std::floor(minLon)); lon <= static_cast<int>(std::floor(maxLon));
             ++lon) {
            job->tiles.append({lat, lon});
        }
    }
    if (job->tiles.isEmpty() || job->tiles.size() > 24) {
        setStatus(tr("Region covers %1 tiles; limit is 24").arg(job->tiles.size()));
        return;
    }
    downloading_ = true;
    emit downloadingChanged();

    auto* nam = new QNetworkAccessManager(this);
    auto fetchNext = std::make_shared<std::function<void()>>();
    *fetchNext = [this, nam, job, fetchNext] {
        if (job->index >= job->tiles.size()) {
            downloading_ = false;
            emit downloadingChanged();
            emit terrainChanged();
            setStatus(tr("SRTM download finished: %1 tile(s) imported").arg(job->imported));
            nam->deleteLater();
            if (job->imported > 0) {
                scheduleRecompute(true);
            }
            return;
        }
        const auto [lat, lon] = job->tiles[job->index++];
        const QString name = srtmTileName(lat, lon);
        const QUrl url(QStringLiteral("https://elevation-tiles-prod.s3.amazonaws.com/skadi/%1/%2.hgt.gz")
                           .arg(name.left(3), name));
        setStatus(tr("Downloading %1 (%2/%3)...").arg(name).arg(job->index).arg(job->tiles.size()));
        QNetworkReply* reply = nam->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, name, job, fetchNext] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray raw = gunzip(reply->readAll());
                // SRTM 1" tile: 3601 x 3601 16-bit samples — verified by exact size.
                if (raw.size() == 3601 * 3601 * 2) {
                    const QString dir =
                        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
                    const QString path = dir + QStringLiteral("/") + name + QStringLiteral(".hgt");
                    QFile file(path);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(raw);
                        file.close();
                        if (terrainStore_->importFile(path.toStdString(),
                                                      tl::terrain::Provenance::Downloaded)) {
                            ++job->imported;
                        }
                        QFile::remove(path);
                    }
                } else {
                    setStatus(tr("Tile %1 failed verification").arg(name));
                }
            }
            (*fetchNext)();
        });
    };
    (*fetchNext)();
}

#else

void AppController::downloadSrtmForRegion(double, double, double, double) {
    setStatus(tr("Downloader is not present in the Air-Gap build"));
}

#endif
