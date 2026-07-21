#pragma once

// The application controller: owns the core services (terrain store, refractivity
// maps, compute pipeline) and exposes everything the QML layer binds to.
//
// Threading model: the UI thread renders only. Every recompute runs on the global
// QThreadPool; results come back as immutable snapshots delivered by queued
// invocation, tagged with a revision — stale results are dropped. Dragging a pin
// streams debounced recomputes at interactive rate.

#include "core/budget/auto_design.h"
#include "core/budget/availability.h"
#include "core/budget/link_budget.h"
#include "core/budget/solver.h"
#include "core/geo/atmosphere.h"
#include "core/project/project.h"
#include "core/terrain/profile.h"
#include "core/terrain/terrain_store.h"
#include "core/tropo/model_suite.h"

#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>
#include <optional>
#include <stop_token>

// Immutable result snapshot handed from the worker to the UI thread.
struct ComputeOutcome {
    quint64 revision = 0;
    tl::terrain::Profile profile;
    tl::tropo::SuiteResult suite;
    tl::budget::LinkBudget budget;
    std::shared_ptr<tl::budget::AvailabilityEngine> engine;
    tl::Percent availabilityAnnual{0.0};
    tl::Percent availabilityWorstMonth{0.0};
    tl::budget::DiversitySeparation separation;
    double computeMs = 0.0;
};

class AppController : public QObject {
    Q_OBJECT
    // Sites
    Q_PROPERTY(double siteALat READ siteALat WRITE setSiteALat NOTIFY sitesChanged)
    Q_PROPERTY(double siteALon READ siteALon WRITE setSiteALon NOTIFY sitesChanged)
    Q_PROPERTY(double siteBLat READ siteBLat WRITE setSiteBLat NOTIFY sitesChanged)
    Q_PROPERTY(double siteBLon READ siteBLon WRITE setSiteBLon NOTIFY sitesChanged)
    Q_PROPERTY(double siteAAgl READ siteAAgl WRITE setSiteAAgl NOTIFY sitesChanged)
    Q_PROPERTY(double siteBAgl READ siteBAgl WRITE setSiteBAgl NOTIFY sitesChanged)
    // Radio
    Q_PROPERTY(double frequencyGHz READ frequencyGHz WRITE setFrequencyGHz NOTIFY radioChanged)
    Q_PROPERTY(QString txPowerText READ txPowerText WRITE setTxPowerText NOTIFY radioChanged)
    Q_PROPERTY(double txPowerDbm READ txPowerDbm NOTIFY radioChanged)
    Q_PROPERTY(double gainA READ gainA WRITE setGainA NOTIFY radioChanged)
    Q_PROPERTY(double gainB READ gainB WRITE setGainB NOTIFY radioChanged)
    Q_PROPERTY(double lineLossA READ lineLossA WRITE setLineLossA NOTIFY radioChanged)
    Q_PROPERTY(double lineLossB READ lineLossB WRITE setLineLossB NOTIFY radioChanged)
    Q_PROPERTY(double noiseFigure READ noiseFigure WRITE setNoiseFigure NOTIFY radioChanged)
    Q_PROPERTY(int modulationIndex READ modulationIndex WRITE setModulationIndex NOTIFY radioChanged)
    Q_PROPERTY(QStringList modulationNames READ modulationNames NOTIFY radioChanged)
    Q_PROPERTY(double dataRateMbps READ dataRateMbps WRITE setDataRateMbps NOTIFY radioChanged)
    Q_PROPERTY(double antennaDiameter READ antennaDiameter WRITE setAntennaDiameter NOTIFY radioChanged)
    // Atmosphere
    Q_PROPERTY(double kFactor READ kFactor WRITE setKFactor NOTIFY atmosphereChanged)
    Q_PROPERTY(bool kFactorAuto READ kFactorAuto WRITE setKFactorAuto NOTIFY atmosphereChanged)
    Q_PROPERTY(double seaLevelN0 READ seaLevelN0 NOTIFY atmosphereChanged)
    Q_PROPERTY(double lapseRateDn READ lapseRateDn NOTIFY atmosphereChanged)
    Q_PROPERTY(int climateIndex READ climateIndex WRITE setClimateIndex NOTIFY atmosphereChanged)
    Q_PROPERTY(QStringList climateNames READ climateNames CONSTANT)
    // Availability / diversity / model
    Q_PROPERTY(int diversityIndex READ diversityIndex WRITE setDiversityIndex NOTIFY targetChanged)
    Q_PROPERTY(int primaryModelIndex READ primaryModelIndex WRITE setPrimaryModelIndex NOTIFY targetChanged)
    Q_PROPERTY(
        double targetAvailability READ targetAvailability WRITE setTargetAvailability NOTIFY targetChanged)
    Q_PROPERTY(bool targetWorstMonth READ targetWorstMonth WRITE setTargetWorstMonth NOTIFY targetChanged)
    // Results
    Q_PROPERTY(QVariantMap geometry READ geometry NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList modelRows READ modelRows NOTIFY resultsChanged)
    Q_PROPERTY(double spreadDb READ spreadDb NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList waterfall READ waterfall NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap budget READ budgetMap NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap availability READ availabilityMap NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList pathPolyline READ pathPolyline NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap commonVolume READ commonVolumeMap NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList horizonFanA READ horizonFanA NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList horizonFanB READ horizonFanB NOTIFY resultsChanged)
    // Profile view draw data (display space: metres along path, metres AMSL + bulge)
    Q_PROPERTY(QPolygonF profileTerrain READ profileTerrain NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList profileVoidSpans READ profileVoidSpans NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileRayA READ profileRayA NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileRayB READ profileRayB NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileLens READ profileLens NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileDirectRay READ profileDirectRay NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileFresnelLower READ profileFresnelLower NOTIFY resultsChanged)
    Q_PROPERTY(QPolygonF profileFresnelUpper READ profileFresnelUpper NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap profileMeta READ profileMeta NOTIFY resultsChanged)
    // Status
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double computeTimeMs READ computeTimeMs NOTIFY resultsChanged)
    Q_PROPERTY(bool terrainCovered READ terrainCovered NOTIFY resultsChanged)
    Q_PROPERTY(bool profileHasVoids READ profileHasVoids NOTIFY resultsChanged)
    Q_PROPERTY(bool airgap READ airgap CONSTANT)
    Q_PROPERTY(QString languageCode READ languageCode WRITE setLanguageCode NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY themeChanged)
    Q_PROPERTY(QVariantList terrainEntries READ terrainEntries NOTIFY terrainChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(
        bool aiCommentaryEnabled READ aiCommentaryEnabled WRITE setAiCommentaryEnabled NOTIFY targetChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(QStringList antennaPresets READ antennaPresets CONSTANT)
    Q_PROPERTY(QStringList radioPresets READ radioPresets CONSTANT)
    // Persisted map-source choice ("": offline terrain rendering).
    Q_PROPERTY(
        QString mapOnlineSource READ mapOnlineSource WRITE setMapOnlineSource NOTIFY mapSettingsChanged)
    Q_PROPERTY(QString mapBasemapPath READ mapBasemapPath WRITE setMapBasemapPath NOTIFY mapSettingsChanged)

  public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    // --- trivial accessors -------------------------------------------------
    double siteALat() const { return link_.siteA.latitude.value(); }
    double siteALon() const { return link_.siteA.longitude.value(); }
    double siteBLat() const { return link_.siteB.latitude.value(); }
    double siteBLon() const { return link_.siteB.longitude.value(); }
    double siteAAgl() const { return link_.aglA.value(); }
    double siteBAgl() const { return link_.aglB.value(); }
    void setSiteALat(double v);
    void setSiteALon(double v);
    void setSiteBLat(double v);
    void setSiteBLon(double v);
    void setSiteAAgl(double v);
    void setSiteBAgl(double v);

    double frequencyGHz() const { return link_.frequency.gigahertz(); }
    void setFrequencyGHz(double v);
    QString txPowerText() const { return txPowerText_; }
    void setTxPowerText(const QString& text);
    double txPowerDbm() const { return link_.radio.txPower.value(); }
    double gainA() const { return link_.radio.antennaGainA.value(); }
    double gainB() const { return link_.radio.antennaGainB.value(); }
    double lineLossA() const { return link_.radio.lineLossA.value(); }
    double lineLossB() const { return link_.radio.lineLossB.value(); }
    double noiseFigure() const { return link_.radio.noiseFigure.value(); }
    void setGainA(double v);
    void setGainB(double v);
    void setLineLossA(double v);
    void setLineLossB(double v);
    void setNoiseFigure(double v);
    int modulationIndex() const { return modulationIndex_; }
    void setModulationIndex(int v);
    QStringList modulationNames() const;
    double dataRateMbps() const { return link_.radio.dataRate.megabits(); }
    void setDataRateMbps(double v);
    double antennaDiameter() const { return link_.antennaDiameter.value(); }
    void setAntennaDiameter(double v);

    double kFactor() const { return link_.atmosphere.kFactor; }
    void setKFactor(double v);
    bool kFactorAuto() const { return !link_.atmosphere.kFactorOverridden; }
    void setKFactorAuto(bool v);
    double seaLevelN0() const { return link_.atmosphere.seaLevelN0; }
    double lapseRateDn() const { return link_.atmosphere.lapseRateDn; }
    int climateIndex() const { return static_cast<int>(link_.atmosphere.climate) - 1; }
    void setClimateIndex(int v);
    QStringList climateNames() const;

    int diversityIndex() const { return static_cast<int>(link_.diversity); }
    void setDiversityIndex(int v);
    int primaryModelIndex() const;
    void setPrimaryModelIndex(int v);
    double targetAvailability() const { return link_.targetAvailability.value(); }
    void setTargetAvailability(double v);
    bool targetWorstMonth() const { return link_.targetIsWorstMonth; }
    void setTargetWorstMonth(bool v);

    QVariantMap geometry() const { return geometry_; }
    QVariantList modelRows() const { return modelRows_; }
    double spreadDb() const { return spreadDb_; }
    QVariantList waterfall() const { return waterfall_; }
    QVariantMap budgetMap() const { return budget_; }
    QVariantMap availabilityMap() const { return availability_; }
    QVariantList pathPolyline() const { return pathPolyline_; }
    QVariantMap commonVolumeMap() const { return commonVolume_; }
    QVariantList horizonFanA() const { return horizonFanA_; }
    QVariantList horizonFanB() const { return horizonFanB_; }
    QPolygonF profileTerrain() const { return profileTerrain_; }
    QVariantList profileVoidSpans() const { return profileVoidSpans_; }
    QPolygonF profileRayA() const { return profileRayA_; }
    QPolygonF profileRayB() const { return profileRayB_; }
    QPolygonF profileLens() const { return profileLens_; }
    QPolygonF profileDirectRay() const { return profileDirectRay_; }
    QPolygonF profileFresnelLower() const { return profileFresnelLower_; }
    QPolygonF profileFresnelUpper() const { return profileFresnelUpper_; }
    QVariantMap profileMeta() const { return profileMeta_; }

    bool busy() const { return busy_; }
    double computeTimeMs() const { return computeTimeMs_; }
    bool terrainCovered() const { return terrainCovered_; }
    bool profileHasVoids() const { return profileHasVoids_; }
    static bool airgap() {
#ifdef TROPOLINK_AIRGAP
        return true;
#else
        return false;
#endif
    }
    QString languageCode() const { return languageCode_; }
    void setLanguageCode(const QString& code);
    // Headless report mode: pick the report language without rewriting the
    // operator's stored UI preference.
    void setLanguageCodeTransient(const QString& code);
    bool darkTheme() const { return darkTheme_; }
    void setDarkTheme(bool v);
    QVariantList terrainEntries() const;
    QString statusMessage() const { return statusMessage_; }

    tl::terrain::TerrainStore* terrainStore() const { return terrainStore_.get(); }
    const ComputeOutcome* lastOutcome() const { return outcome_ ? outcome_.get() : nullptr; }

    // --- invokables ---------------------------------------------------------
    Q_INVOKABLE void setSiteFromMap(int site, double lat, double lon, bool preview);
    Q_INVOKABLE bool parseCoordinateToSite(int site, const QString& text);
    Q_INVOKABLE QVariantMap parseCoordinate(const QString& text) const;
    Q_INVOKABLE QVariantMap coordinateFormats(double lat, double lon) const;
    Q_INVOKABLE QString measure(double lat1, double lon1, double lat2, double lon2) const;
    Q_INVOKABLE void importTerrainFiles(const QList<QUrl>& urls);
    Q_INVOKABLE void removeTerrainEntry(const QString& fileName);
    Q_INVOKABLE void recompute(bool immediate = false);
    Q_INVOKABLE QVariantMap solveFor(int what); // 0 power, 1 gain, 2 rate
    // Auto-design: derive the whole radio configuration from the path geometry and
    // apply it. Returns a summary {ok, note, changes:[{field,from,to,reason}], ...}.
    Q_INVOKABLE QVariantMap autoDesignRadio();
    Q_INVOKABLE QVariantList availabilityCurve(bool worstMonth) const;
    Q_INVOKABLE bool exportKml(const QUrl& url);
    Q_INVOKABLE bool exportProfileCsv(const QUrl& url);
    Q_INVOKABLE bool exportBudgetCsv(const QUrl& url);
    Q_INVOKABLE bool saveProjectFile(const QUrl& url);
    Q_INVOKABLE bool loadProjectFile(const QUrl& url);
    Q_INVOKABLE void loadReferenceProject();
    Q_INVOKABLE bool generateReport(const QUrl& url, const QImage& mapSnapshot);
    Q_INVOKABLE QString lastReportHash() const { return lastReportHash_; }
    Q_INVOKABLE QString provenance(const QString& key) const; // formula/source on hover
    // Detailed in-app help (PL/EN follows languageCode).
    Q_INVOKABLE QVariantList helpTopics() const;
    Q_INVOKABLE QString helpHtml(const QString& topic) const;
    QString mapOnlineSource() const { return mapOnlineSource_; }
    void setMapOnlineSource(const QString& id);
    QString mapBasemapPath() const { return mapBasemapPath_; }
    void setMapBasemapPath(const QString& path);
    Q_INVOKABLE void downloadSrtmForRegion(double minLat, double maxLat, double minLon, double maxLon);
    QStringList antennaPresets() const;
    QStringList radioPresets() const;
    Q_INVOKABLE void applyAntennaPreset(int index);
    Q_INVOKABLE void applyRadioPreset(int index);
    bool aiCommentaryEnabled() const { return aiCommentaryEnabled_; }
    void setAiCommentaryEnabled(bool v) {
        aiCommentaryEnabled_ = v;
        emit targetChanged();
    }
    bool downloading() const { return downloading_; }

  signals:
    void sitesChanged();
    void radioChanged();
    void atmosphereChanged();
    void targetChanged();
    void resultsChanged();
    void busyChanged();
    void languageChanged();
    void themeChanged();
    void terrainChanged();
    void statusMessageChanged();
    void reportGenerated(const QString& path);
    void downloadingChanged();
    void mapSettingsChanged();

  private:
    struct LinkState {
        tl::geo::GeoPoint siteA{tl::Degrees(51.50609699), tl::Degrees(15.33150851)};
        tl::geo::GeoPoint siteB{tl::Degrees(52.43470597), tl::Degrees(15.21931198)};
        tl::Meters aglA{4.0};
        tl::Meters aglB{4.0};
        tl::Hertz frequency = tl::Hertz::fromGigahertz(4.4);
        tl::budget::RadioParams radio;
        tl::geo::Atmosphere atmosphere;
        tl::budget::DiversityMode diversity = tl::budget::DiversityMode::Quad;
        tl::tropo::ModelId primaryModel = tl::tropo::ModelId::P617;
        tl::Percent targetAvailability{99.9};
        bool targetIsWorstMonth = false;
        tl::Meters antennaDiameter{3.0};
    };

    void scheduleRecompute(bool immediate);
    void runComputation(quint64 revision, LinkState state, std::stop_token stopToken);
    void deliverOutcome(std::shared_ptr<ComputeOutcome> outcome);
    void refreshAutoAtmosphere();
    void applyProject(const tl::project::Project& project);
    tl::project::Project currentProject() const;
    void setStatus(const QString& message);

    LinkState link_;
    QString txPowerText_ = QStringLiteral("57.0 dBm");
    int modulationIndex_ = 1; // QPSK
    tl::budget::ModulationLibrary modulations_;

    std::unique_ptr<tl::terrain::TerrainStore> terrainStore_;
    std::optional<tl::geo::RefractivityMaps> refractivityMaps_;

    std::atomic<quint64> revision_{0};
    QTimer debounce_;
    bool busy_ = false;
    std::shared_ptr<ComputeOutcome> outcome_;
    // Computations run serialized on their own single-thread pool (extraction is
    // internally parallel already); a new revision stop-requests the running one so
    // dragging a pin over a long path cannot pile up unbounded work.
    QThreadPool computePool_;
    std::stop_source computeStop_;

    // published results
    QVariantMap geometry_;
    QVariantList modelRows_;
    double spreadDb_ = 0.0;
    QVariantList waterfall_;
    QVariantMap budget_;
    QVariantMap availability_;
    QVariantList pathPolyline_;
    QVariantMap commonVolume_;
    QVariantList horizonFanA_;
    QVariantList horizonFanB_;
    QPolygonF profileTerrain_;
    QVariantList profileVoidSpans_;
    QPolygonF profileRayA_;
    QPolygonF profileRayB_;
    QPolygonF profileLens_;
    QPolygonF profileDirectRay_;
    QPolygonF profileFresnelLower_;
    QPolygonF profileFresnelUpper_;
    QVariantMap profileMeta_;
    double computeTimeMs_ = 0.0;
    bool terrainCovered_ = false;
    bool profileHasVoids_ = false;
    QString languageCode_ = QStringLiteral("pl");
    bool darkTheme_ = true;
    QString mapOnlineSource_;
    QString mapBasemapPath_;
    QString statusMessage_;
    QString lastReportHash_;
    QString projectName_ = QStringLiteral("Untitled");
    bool aiCommentaryEnabled_ = true;
    bool downloading_ = false;

    struct AntennaPreset {
        QString name;
        double gainDbi;
        double diameterM;
    };
    struct RadioPreset {
        QString name;
        double txPowerDbm;
        double noiseFigureDb;
    };
    QList<AntennaPreset> antennaPresets_;
    QList<RadioPreset> radioPresets_;
    void loadEquipmentLibrary();
};
