#include "core/report/report_content.h"

#include "core/common/sha256.h"
#include "core/geo/coords.h"

#include <cstdio>
#include <map>
#include <sstream>

namespace tl::report {

namespace {

struct Entry {
    const char* en;
    const char* pl;
};

// Report vocabulary. UI copy is terse and technical; the report follows suit.
const std::map<std::string, Entry>& vocabulary() {
    static const std::map<std::string, Entry> table = {
        {"report_title", {"Troposcatter Link Design Report", "Raport projektu łącza troposferycznego"}},
        {"feasible_green",
         {"FEASIBLE — margin meets the target availability",
          "WYKONALNE — margines spełnia docelową dostępność"}},
        {"feasible_yellow",
         {"MARGINAL — margin within 3 dB of the requirement",
          "GRANICZNE — margines w granicach 3 dB od wymagania"}},
        {"feasible_red",
         {"NOT FEASIBLE — margin below the required availability",
          "NIEWYKONALNE — margines poniżej wymaganej dostępności"}},
        {"sec_sites", {"Sites", "Stanowiska"}},
        {"sec_geometry", {"Path geometry", "Geometria trasy"}},
        {"sec_models", {"Propagation model comparison", "Porównanie modeli propagacyjnych"}},
        {"sec_budget", {"Link budget", "Bilans łącza"}},
        {"sec_availability", {"Availability and diversity", "Dostępność i dywersyfikacja"}},
        {"sec_equipment", {"Equipment", "Wyposażenie"}},
        {"sec_assumptions", {"Assumptions and data provenance", "Założenia i pochodzenie danych"}},
        {"sec_conclusion", {"Conclusion", "Wnioski"}},
        {"sec_ai", {"Automated commentary (local model)", "Komentarz automatyczny (model lokalny)"}},
        {"site", {"Site", "Stanowisko"}},
        {"latitude", {"Latitude", "Szerokość geogr."}},
        {"longitude", {"Longitude", "Długość geogr."}},
        {"dd", {"Decimal degrees", "Stopnie dziesiętne"}},
        {"dms", {"DMS", "DMS"}},
        {"mgrs", {"MGRS", "MGRS"}},
        {"utm", {"UTM", "UTM"}},
        {"antenna_agl", {"Antenna height AGL", "Wysokość anteny AGL"}},
        {"distance", {"Geodesic distance", "Odległość geodezyjna"}},
        {"azimuth_ab", {"Azimuth A>B", "Azymut A>B"}},
        {"azimuth_ba", {"Azimuth B>A", "Azymut B>A"}},
        {"takeoff_a", {"Takeoff angle A", "Kąt startu A"}},
        {"takeoff_b", {"Takeoff angle B", "Kąt startu B"}},
        {"scatter_angle", {"Scatter angle", "Kąt rozproszenia"}},
        {"cv_height", {"Common volume base (AMSL)", "Podstawa objętości wspólnej (n.p.m.)"}},
        {"cv_top", {"Common volume top (AMSL)", "Wierzchołek objętości wspólnej (n.p.m.)"}},
        {"cv_above_terrain", {"Common volume above terrain", "Objętość wspólna nad terenem"}},
        {"cv_slant_a", {"Slant range from A", "Odległość skośna od A"}},
        {"cv_slant_b", {"Slant range from B", "Odległość skośna od B"}},
        {"direct_blocked",
         {"Direct path obstructed by terrain/horizon", "Trasa bezpośrednia zasłonięta przez teren/horyzont"}},
        {"direct_clear", {"Direct path not obstructed", "Trasa bezpośrednia nie jest zasłonięta"}},
        {"model", {"Model", "Model"}},
        {"median_loss", {"Median loss (dB)", "Strata medianowa (dB)"}},
        {"coupling_loss", {"Aperture-medium coupling (dB)", "Sprzężenie apertura-ośrodek (dB)"}},
        {"validity", {"Validity", "Ważność"}},
        {"valid", {"in range", "w zakresie"}},
        {"invalid", {"OUT OF VALIDITY RANGE", "POZA ZAKRESEM WAŻNOŚCI"}},
        {"spread", {"Model spread (uncertainty band)", "Rozrzut modeli (pasmo niepewności)"}},
        {"spread_note",
         {"The spread between valid models is stated as the uncertainty of this design. A designer "
          "who sees the spread builds in margin; hiding it builds false confidence.",
          "Rozrzut między ważnymi modelami stanowi niepewność tego projektu. Projektant, który widzi "
          "rozrzut, dobiera zapas; ukrywanie go buduje fałszywą pewność."}},
        {"item", {"Item", "Pozycja"}},
        {"value", {"Value", "Wartość"}},
        {"tx_power", {"TX power", "Moc nadajnika"}},
        {"line_loss_tx", {"TX line loss", "Straty toru TX"}},
        {"antenna_gain_tx", {"TX antenna gain", "Zysk anteny TX"}},
        {"eirp", {"EIRP", "EIRP"}},
        {"path_loss", {"Median path loss (primary model)", "Medianowa strata trasy (model główny)"}},
        {"antenna_gain_rx", {"RX antenna gain", "Zysk anteny RX"}},
        {"line_loss_rx", {"RX line loss", "Straty toru RX"}},
        {"rsl", {"Median received signal level", "Medianowy poziom sygnału odbieranego"}},
        {"noise_floor", {"Noise floor (kTB + NF)", "Poziom szumów (kTB + NF)"}},
        {"median_snr", {"Median SNR", "Medianowy SNR"}},
        {"required_snr", {"Required SNR", "Wymagany SNR"}},
        {"fade_margin", {"Fade margin", "Margines zaników"}},
        {"availability_annual", {"Availability (average year)", "Dostępność (rok średni)"}},
        {"availability_wm", {"Availability (worst month)", "Dostępność (najgorszy miesiąc)"}},
        {"diversity", {"Diversity", "Dywersyfikacja"}},
        {"diversity_gain", {"Diversity gain at target", "Zysk dywersyfikacji przy celu"}},
        {"div_none", {"none", "brak"}},
        {"div_space", {"space", "przestrzenna"}},
        {"div_frequency", {"frequency", "częstotliwościowa"}},
        {"div_angle", {"angle", "kątowa"}},
        {"div_quad", {"quad (space+frequency)", "poczwórna (przestrzenna+częstotliwościowa)"}},
        {"sep_h", {"Space diversity spacing, horizontal", "Odstęp dywersyfikacji przestrzennej, poziomy"}},
        {"sep_v", {"Space diversity spacing, vertical", "Odstęp dywersyfikacji przestrzennej, pionowy"}},
        {"sep_f", {"Frequency diversity separation", "Odstęp dywersyfikacji częstotliwościowej"}},
        {"target_availability", {"Target availability", "Dostępność docelowa"}},
        {"k_factor", {"Effective earth radius factor k", "Współczynnik efektywnego promienia Ziemi k"}},
        {"n0", {"Sea-level surface refractivity N0", "Refrakcyjność przy poziomie morza N0"}},
        {"dn", {"Refractivity lapse rate dN", "Gradient refrakcyjności dN"}},
        {"climate", {"Radio climate", "Klimat radiowy"}},
        {"primary_model", {"Primary model", "Model główny"}},
        {"terrain_sources", {"Terrain data sources", "Źródła danych terenowych"}},
        {"provenance_imported", {"imported", "zaimportowane"}},
        {"provenance_downloaded", {"downloaded", "pobrane"}},
        {"no_terrain",
         {"NO TERRAIN DATA — smooth-earth estimates",
          "BRAK DANYCH TERENOWYCH — oszacowania dla gładkiej Ziemi"}},
        {"voids_note",
         {"Profile contains interpolated DEM voids (flagged in the profile view).",
          "Profil zawiera interpolowane braki DEM (oznaczone w widoku profilu)."}},
        {"app_version", {"TropoLink version", "Wersja TropoLink"}},
        {"gdal_version", {"GDAL version", "Wersja GDAL"}},
        {"geographiclib_version", {"GeographicLib version", "Wersja GeographicLib"}},
        {"report_hash", {"Report content SHA-256", "SHA-256 treści raportu"}},
        {"frequency", {"Frequency", "Częstotliwość"}},
        {"bandwidth", {"Bandwidth", "Szerokość pasma"}},
        {"modulation", {"Modulation", "Modulacja"}},
        {"data_rate", {"Data rate", "Przepływność"}},
        {"noise_figure", {"Receiver noise figure", "Współczynnik szumów odbiornika"}},
        {"antenna_diameter", {"Antenna diameter", "Średnica anteny"}},
        {"model_versions", {"Model versions", "Wersje modeli"}},
        {"fig_map",
         {"Map with sites, path and common volume", "Mapa ze stanowiskami, trasą i objętością wspólną"}},
        {"fig_profile",
         {"Terrain profile with common-volume lens", "Profil terenu z soczewką objętości wspólnej"}},
        {"fig_curve", {"Availability vs. fade margin", "Dostępność w funkcji marginesu zaników"}},
    };
    return table;
}

std::string fmt(const char* format, double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), format, value);
    return buf;
}

std::string tr(const std::string& key, Language lang) {
    const auto it = vocabulary().find(key);
    if (it == vocabulary().end()) {
        return key;
    }
    return lang == Language::Polish ? it->second.pl : it->second.en;
}

std::string coordRow(const geo::GeoPoint& p, geo::CoordFormat f) {
    auto r = geo::Coords::format(p, f);
    return r.hasValue() ? r.value() : std::string("-");
}

} // namespace

std::string reportString(const std::string& key, Language language) {
    return tr(key, language);
}

std::string ReportContent::canonicalText() const {
    std::ostringstream out;
    out << "TropoLink report v1\n" << title << "\n" << projectName << "\n" << linkName << "\n";
    out << "feasibility:" << static_cast<int>(feasibility) << "\n" << feasibilityStatement << "\n";
    for (const auto& s : sections) {
        out << "## " << s.key << " | " << s.title << "\n";
        for (const auto& p : s.paragraphs) {
            out << p << "\n";
        }
        for (const auto& t : s.tables) {
            out << "[table] " << t.title << "\n";
            for (const auto& h : t.header) {
                out << h << ";";
            }
            out << "\n";
            for (const auto& row : t.rows) {
                for (const auto& cell : row) {
                    out << cell << ";";
                }
                out << "\n";
            }
        }
        for (const auto& f : s.figureKeys) {
            out << "[figure] " << f << "\n";
        }
    }
    return out.str();
}

void ReportContent::finalizeHash() {
    contentSha256 = Sha256::hex(canonicalText());
}

ReportContent buildReportContent(const ReportInputs& in, Language lang) {
    ReportContent rc;
    rc.language = lang;
    rc.title = tr("report_title", lang);
    rc.projectName = in.project.name;
    rc.linkName = in.link.name;
    rc.generatedStamp = in.generatedStamp;

    const bool worstMonthTarget = in.link.targetIsWorstMonth;
    const Percent achieved =
        worstMonthTarget ? in.achievedAvailabilityWorstMonth : in.achievedAvailabilityAnnual;
    const Decibels requiredMargin = in.availability.marginForAvailability(
        in.link.targetAvailability, in.link.diversity, worstMonthTarget);
    const double marginGap = in.budget.fadeMargin.value() - requiredMargin.value();
    if (achieved.value() >= in.link.targetAvailability.value()) {
        rc.feasibility = Feasibility::Green;
        rc.feasibilityStatement = tr("feasible_green", lang);
    } else if (marginGap >= -3.0) {
        rc.feasibility = Feasibility::Yellow;
        rc.feasibilityStatement = tr("feasible_yellow", lang);
    } else {
        rc.feasibility = Feasibility::Red;
        rc.feasibilityStatement = tr("feasible_red", lang);
    }

    // --- Sites -----------------------------------------------------------------
    {
        Section s;
        s.key = "sites";
        s.title = tr("sec_sites", lang);
        Table t;
        t.header = {tr("site", lang), tr("dd", lang),  tr("dms", lang),
                    tr("mgrs", lang), tr("utm", lang), tr("antenna_agl", lang)};
        for (const auto* site :
             {in.project.findSite(in.link.siteAId), in.project.findSite(in.link.siteBId)}) {
            if (site == nullptr) {
                continue;
            }
            t.rows.push_back({site->name, coordRow(site->position, geo::CoordFormat::DecimalDegrees),
                              coordRow(site->position, geo::CoordFormat::Dms),
                              coordRow(site->position, geo::CoordFormat::Mgrs),
                              coordRow(site->position, geo::CoordFormat::Utm),
                              fmt("%.1f m", site->antennaHeightAgl.value())});
        }
        s.tables.push_back(std::move(t));
        s.figureKeys.push_back("map");
        rc.sections.push_back(std::move(s));
    }

    // --- Geometry --------------------------------------------------------------
    {
        Section s;
        s.key = "geometry";
        s.title = tr("sec_geometry", lang);
        Table t;
        t.header = {tr("item", lang), tr("value", lang)};
        const auto& g = in.suite;
        t.rows = {
            {tr("distance", lang), fmt("%.3f km", g.inverse.distance.kilometers())},
            {tr("azimuth_ab", lang), fmt("%.2f\xC2\xB0", g.inverse.forwardAzimuth.value())},
            {tr("azimuth_ba", lang), fmt("%.2f\xC2\xB0", g.inverse.reverseAzimuth.value())},
            {tr("takeoff_a", lang), fmt("%.2f mrad", g.horizons.takeoffAngleA.milliradians())},
            {tr("takeoff_b", lang), fmt("%.2f mrad", g.horizons.takeoffAngleB.milliradians())},
            {tr("scatter_angle", lang), fmt("%.2f mrad", g.geometry.scatterAngle.milliradians()) + " (" +
                                            fmt("%.3f\xC2\xB0", g.geometry.scatterAngle.degrees()) + ")"},
            {tr("cv_height", lang), fmt("%.0f m", g.geometry.volumeBaseAmsl.value())},
            {tr("cv_top", lang), fmt("%.0f m", g.geometry.volumeTopAmsl.value())},
            {tr("cv_above_terrain", lang), fmt("%.0f m", g.commonVolumeAboveTerrain.value())},
            {tr("cv_slant_a", lang), fmt("%.2f km", g.geometry.slantRangeA.kilometers())},
            {tr("cv_slant_b", lang), fmt("%.2f km", g.geometry.slantRangeB.kilometers())},
        };
        s.tables.push_back(std::move(t));
        s.paragraphs.push_back(tr(g.horizons.directPathObstructed ? "direct_blocked" : "direct_clear", lang));
        if (g.profileHasVoids) {
            s.paragraphs.push_back(tr("voids_note", lang));
        }
        s.figureKeys.push_back("profile");
        rc.sections.push_back(std::move(s));
    }

    // --- Model comparison ------------------------------------------------------
    {
        Section s;
        s.key = "models";
        s.title = tr("sec_models", lang);
        Table t;
        t.header = {tr("model", lang), tr("median_loss", lang), tr("coupling_loss", lang),
                    tr("validity", lang)};
        for (const auto& row : in.suite.rows) {
            std::string validity = row.valid ? tr("valid", lang) : tr("invalid", lang);
            for (const auto& issue : row.validityIssues) {
                validity += "; " + issue;
            }
            if (!row.note.empty()) {
                validity += " [" + row.note + "]";
            }
            t.rows.push_back({row.name, row.valid ? fmt("%.1f", row.median.value()) : "-",
                              fmt("%.1f", row.couplingLoss.value()), validity});
        }
        s.tables.push_back(std::move(t));
        s.paragraphs.push_back(tr("spread", lang) + ": " + fmt("%.1f dB", in.suite.spread.value()) +
                               "  (\xC2\xB1" + fmt("%.1f dB", in.suite.spread.value() / 2.0) + ")");
        s.paragraphs.push_back(tr("spread_note", lang));
        rc.sections.push_back(std::move(s));
    }

    // --- Budget ----------------------------------------------------------------
    {
        Section s;
        s.key = "budget";
        s.title = tr("sec_budget", lang);
        Table t;
        t.header = {tr("item", lang), tr("value", lang)};
        for (const auto& item : in.budget.waterfall) {
            const std::string unit = item.isLevel ? " dBm" : " dB";
            t.rows.push_back({tr(item.label, lang), fmt("%.1f", item.valueDb) + unit});
        }
        s.tables.push_back(std::move(t));
        rc.sections.push_back(std::move(s));
    }

    // --- Availability & diversity ---------------------------------------------
    {
        Section s;
        s.key = "availability";
        s.title = tr("sec_availability", lang);
        Table t;
        t.header = {tr("item", lang), tr("value", lang)};
        const char* divKey = "div_none";
        switch (in.link.diversity) {
        case budget::DiversityMode::Space:
            divKey = "div_space";
            break;
        case budget::DiversityMode::Frequency:
            divKey = "div_frequency";
            break;
        case budget::DiversityMode::Angle:
            divKey = "div_angle";
            break;
        case budget::DiversityMode::Quad:
            divKey = "div_quad";
            break;
        default:
            break;
        }
        t.rows = {
            {tr("diversity", lang), tr(divKey, lang)},
            {tr("availability_annual", lang), fmt("%.4f %%", in.achievedAvailabilityAnnual.value())},
            {tr("availability_wm", lang), fmt("%.4f %%", in.achievedAvailabilityWorstMonth.value())},
            {tr("target_availability", lang),
             fmt("%.3f %%", in.link.targetAvailability.value()) +
                 (worstMonthTarget ? (lang == Language::Polish ? " (najgorszy miesiąc)" : " (worst month)")
                                   : (lang == Language::Polish ? " (rok)" : " (annual)"))},
            {tr("diversity_gain", lang),
             fmt("%.1f dB",
                 in.availability
                     .diversityGain(in.link.targetAvailability, in.link.diversity, worstMonthTarget)
                     .value())},
            {tr("sep_h", lang), fmt("%.1f m", in.separation.horizontal.value())},
            {tr("sep_v", lang), fmt("%.1f m", in.separation.vertical.value())},
            {tr("sep_f", lang), fmt("%.1f MHz", in.separation.frequencySeparationMhz)},
        };
        s.tables.push_back(std::move(t));
        s.figureKeys.push_back("curve");
        rc.sections.push_back(std::move(s));
    }

    // --- Equipment -------------------------------------------------------------
    {
        Section s;
        s.key = "equipment";
        s.title = tr("sec_equipment", lang);
        Table t;
        t.header = {tr("item", lang), tr("value", lang)};
        t.rows = {
            {tr("frequency", lang), fmt("%.4f GHz", in.link.frequency.gigahertz())},
            {tr("tx_power", lang), fmt("%.1f dBm", in.link.radio.txPower.value()) + " (" +
                                       fmt("%.0f W", in.link.radio.txPower.watts()) + ")"},
            {tr("antenna_gain_tx", lang), fmt("%.1f dBi", in.link.radio.antennaGainA.value())},
            {tr("antenna_gain_rx", lang), fmt("%.1f dBi", in.link.radio.antennaGainB.value())},
            {tr("antenna_diameter", lang), fmt("%.1f m", in.link.antennaDiameter.value())},
            {tr("noise_figure", lang), fmt("%.1f dB", in.link.radio.noiseFigure.value())},
            {tr("bandwidth", lang), fmt("%.3f MHz", in.link.radio.effectiveBandwidth().megahertz())},
            {tr("modulation", lang), in.link.radio.modulation.name},
            {tr("data_rate", lang), fmt("%.3f Mbit/s", in.link.radio.dataRate.megabits())},
        };
        s.tables.push_back(std::move(t));
        rc.sections.push_back(std::move(s));
    }

    // --- Assumptions & provenance (the audit page) ------------------------------
    {
        Section s;
        s.key = "assumptions";
        s.title = tr("sec_assumptions", lang);
        Table t;
        t.header = {tr("item", lang), tr("value", lang)};
        t.rows = {
            {tr("k_factor", lang), fmt("%.4f", in.link.atmosphere.kFactor)},
            {tr("n0", lang), fmt("%.1f", in.link.atmosphere.seaLevelN0)},
            {tr("dn", lang), fmt("%.1f N/km", in.link.atmosphere.lapseRateDn)},
            {tr("climate", lang), geo::climateName(in.link.atmosphere.climate)},
            {tr("primary_model", lang),
             in.suite.primaryModel() ? in.suite.primaryModel()->name() : std::string("-")},
            {tr("app_version", lang), in.appVersion},
            {tr("gdal_version", lang), in.gdalVersion},
            {tr("geographiclib_version", lang), in.geographicLibVersion},
        };
        for (const auto& row : in.suite.rows) {
            if (row.id != tropo::ModelId::Fspl) {
                t.rows.push_back({tr("model_versions", lang) + ": " + row.name, row.citation});
            }
        }
        s.tables.push_back(std::move(t));

        Table sources;
        sources.title = tr("terrain_sources", lang);
        sources.header = {tr("item", lang), tr("value", lang)};
        if (in.terrainSources.empty()) {
            sources.rows.push_back({tr("no_terrain", lang), "-"});
        }
        for (const auto& e : in.terrainSources) {
            sources.rows.push_back(
                {e.fileName, e.format + ", " + fmt("%.0f m", e.resolutionM) + ", " +
                                 tr(e.provenance == terrain::Provenance::Downloaded ? "provenance_downloaded"
                                                                                    : "provenance_imported",
                                    lang)});
        }
        s.tables.push_back(std::move(sources));
        rc.sections.push_back(std::move(s));
    }

    // --- Plain-language conclusion (no AI) --------------------------------------
    {
        Section s;
        s.key = "conclusion";
        s.title = tr("sec_conclusion", lang);
        char buf[640];
        if (lang == Language::Polish) {
            std::snprintf(buf, sizeof(buf),
                          "Łącze o długości %.1f km przy %.2f GHz osiąga medianowy margines zaników %.1f dB "
                          "przy dostępności rocznej %.4f%%. Rozrzut modeli %.1f dB przyjęto jako niepewność "
                          "projektu. Ocena: %s.",
                          in.suite.inverse.distance.kilometers(), in.link.frequency.gigahertz(),
                          in.budget.fadeMargin.value(), in.achievedAvailabilityAnnual.value(),
                          in.suite.spread.value(), rc.feasibilityStatement.c_str());
        } else {
            std::snprintf(buf, sizeof(buf),
                          "The %.1f km link at %.2f GHz achieves a median fade margin of %.1f dB with an "
                          "annual availability of %.4f%%. The %.1f dB model spread is carried as the design "
                          "uncertainty. Rating: %s.",
                          in.suite.inverse.distance.kilometers(), in.link.frequency.gigahertz(),
                          in.budget.fadeMargin.value(), in.achievedAvailabilityAnnual.value(),
                          in.suite.spread.value(), rc.feasibilityStatement.c_str());
        }
        s.paragraphs.push_back(buf);
        rc.sections.push_back(std::move(s));
    }

    if (!in.aiCommentary.empty()) {
        Section s;
        s.key = "ai";
        s.title = tr("sec_ai", lang);
        s.paragraphs.push_back(in.aiCommentary);
        rc.sections.push_back(std::move(s));
    }

    rc.finalizeHash();
    return rc;
}

} // namespace tl::report
