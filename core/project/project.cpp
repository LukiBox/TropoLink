#include "core/project/project.h"

#include <nlohmann/json.hpp>
#include <zip.h>

#include <cstring>

namespace tl::project {

using nlohmann::json;

const Site* Project::findSite(const std::string& siteId) const {
    for (const auto& s : sites) {
        if (s.id == siteId) {
            return &s;
        }
    }
    return nullptr;
}

namespace {

json siteToJson(const Site& s) {
    return json{{"id", s.id},
                {"name", s.name},
                {"latDeg", s.position.latitude.value()},
                {"lonDeg", s.position.longitude.value()},
                {"antennaAglM", s.antennaHeightAgl.value()}};
}

Site siteFromJson(const json& j) {
    Site s;
    s.id = j.value("id", "");
    s.name = j.value("name", "");
    s.position = geo::GeoPoint{Degrees(j.value("latDeg", 0.0)), Degrees(j.value("lonDeg", 0.0))};
    s.antennaHeightAgl = Meters(j.value("antennaAglM", 4.0));
    return s;
}

const char* diversityKey(budget::DiversityMode m) {
    return budget::diversityName(m);
}

budget::DiversityMode diversityFromKey(const std::string& k) {
    if (k == "space") {
        return budget::DiversityMode::Space;
    }
    if (k == "frequency") {
        return budget::DiversityMode::Frequency;
    }
    if (k == "angle") {
        return budget::DiversityMode::Angle;
    }
    if (k == "quad") {
        return budget::DiversityMode::Quad;
    }
    return budget::DiversityMode::None;
}

const char* modelKey(tropo::ModelId id) {
    switch (id) {
    case tropo::ModelId::Fspl:
        return "fspl";
    case tropo::ModelId::P617:
        return "p617";
    case tropo::ModelId::Tn101:
        return "tn101";
    case tropo::ModelId::Itm:
        return "itm";
    }
    return "p617";
}

tropo::ModelId modelFromKey(const std::string& k) {
    if (k == "tn101") {
        return tropo::ModelId::Tn101;
    }
    if (k == "itm") {
        return tropo::ModelId::Itm;
    }
    if (k == "fspl") {
        return tropo::ModelId::Fspl;
    }
    return tropo::ModelId::P617;
}

json linkToJson(const LinkSpec& l) {
    json radio{{"txPowerDbm", l.radio.txPower.value()},
               {"gainADbi", l.radio.antennaGainA.value()},
               {"gainBDbi", l.radio.antennaGainB.value()},
               {"lineLossADb", l.radio.lineLossA.value()},
               {"lineLossBDb", l.radio.lineLossB.value()},
               {"noiseFigureDb", l.radio.noiseFigure.value()},
               {"bandwidthHz", l.radio.bandwidth.value()},
               {"bandwidthFromModulation", l.radio.bandwidthFromModulation},
               {"modulationName", l.radio.modulation.name},
               {"modulationBitsPerSymbol", l.radio.modulation.bitsPerSymbol},
               {"modulationRequiredEbN0Db", l.radio.modulation.requiredEbN0.value()},
               {"modulationRolloff", l.radio.modulation.rolloff},
               {"dataRateBps", l.radio.dataRate.value()}};
    if (l.radio.requiredSnrOverride) {
        radio["requiredSnrOverrideDb"] = l.radio.requiredSnrOverride->value();
    }
    json atm{{"kFactor", l.atmosphere.kFactor},
             {"seaLevelN0", l.atmosphere.seaLevelN0},
             {"lapseRateDn", l.atmosphere.lapseRateDn},
             {"climate", static_cast<int>(l.atmosphere.climate)},
             {"kFactorOverridden", l.atmosphere.kFactorOverridden}};
    return json{{"id", l.id},
                {"name", l.name},
                {"siteA", l.siteAId},
                {"siteB", l.siteBId},
                {"frequencyHz", l.frequency.value()},
                {"radio", std::move(radio)},
                {"atmosphere", std::move(atm)},
                {"diversity", diversityKey(l.diversity)},
                {"primaryModel", modelKey(l.primaryModel)},
                {"targetAvailabilityPct", l.targetAvailability.value()},
                {"targetIsWorstMonth", l.targetIsWorstMonth},
                {"antennaDiameterM", l.antennaDiameter.value()}};
}

} // namespace

std::string projectToJson(const Project& project) {
    json doc;
    doc["schemaVersion"] = project.schemaVersion;
    doc["name"] = project.name;
    doc["id"] = project.id;
    json sites = json::array();
    for (const auto& s : project.sites) {
        sites.push_back(siteToJson(s));
    }
    doc["sites"] = std::move(sites);
    json links = json::array();
    for (const auto& l : project.links) {
        links.push_back(linkToJson(l));
    }
    doc["links"] = std::move(links);
    json snaps = json::array();
    for (const auto& s : project.snapshots) {
        json js{{"linkId", s.linkId},
                {"created", s.createdIso8601},
                {"appVersion", s.appVersion},
                {"gdalVersion", s.gdalVersion},
                {"geographicLibVersion", s.geographicLibVersion},
                {"reportContentSha256", s.reportContentSha256}};
        js["modelVersions"] = s.modelVersions;
        js["values"] = s.values;
        snaps.push_back(std::move(js));
    }
    doc["snapshots"] = std::move(snaps);
    return doc.dump(2);
}

Expected<Project> projectFromJson(const std::string& jsonText) {
    json doc = json::parse(jsonText, nullptr, false);
    if (doc.is_discarded()) {
        return Error{"project JSON is not valid"};
    }
    Project p;
    p.schemaVersion = doc.value("schemaVersion", 1);
    if (p.schemaVersion > kSchemaVersion) {
        return Error{"project schema version " + std::to_string(p.schemaVersion) +
                     " is newer than this TropoLink understands"};
    }
    // Schema migrations land here as version bumps accumulate (none needed at v1).
    p.name = doc.value("name", "Untitled");
    p.id = doc.value("id", "");
    for (const auto& js : doc.value("sites", json::array())) {
        p.sites.push_back(siteFromJson(js));
    }
    for (const auto& jl : doc.value("links", json::array())) {
        LinkSpec l;
        l.id = jl.value("id", "");
        l.name = jl.value("name", "");
        l.siteAId = jl.value("siteA", "");
        l.siteBId = jl.value("siteB", "");
        const auto& radio = jl.value("radio", json::object());
        l.radio.txPower = Dbm(radio.value("txPowerDbm", 57.0));
        l.radio.antennaGainA = Dbi(radio.value("gainADbi", 39.1));
        l.radio.antennaGainB = Dbi(radio.value("gainBDbi", 39.1));
        l.radio.lineLossA = Decibels(radio.value("lineLossADb", 0.5));
        l.radio.lineLossB = Decibels(radio.value("lineLossBDb", 0.5));
        l.radio.noiseFigure = Decibels(radio.value("noiseFigureDb", 4.0));
        l.radio.bandwidth = Hertz(radio.value("bandwidthHz", 2.0e6));
        l.radio.bandwidthFromModulation = radio.value("bandwidthFromModulation", true);
        l.radio.modulation.name = radio.value("modulationName", "QPSK");
        l.radio.modulation.bitsPerSymbol = radio.value("modulationBitsPerSymbol", 2.0);
        l.radio.modulation.requiredEbN0 = Decibels(radio.value("modulationRequiredEbN0Db", 10.5));
        l.radio.modulation.rolloff = radio.value("modulationRolloff", 0.35);
        l.radio.dataRate = BitsPerSecond(radio.value("dataRateBps", 2.0e6));
        if (radio.contains("requiredSnrOverrideDb")) {
            l.radio.requiredSnrOverride = Decibels(radio["requiredSnrOverrideDb"].get<double>());
        }
        l.frequency = Hertz(jl.value("frequencyHz", 4.4e9));
        const auto& atm = jl.value("atmosphere", json::object());
        l.atmosphere.kFactor = atm.value("kFactor", 4.0 / 3.0);
        l.atmosphere.seaLevelN0 = atm.value("seaLevelN0", 315.0);
        l.atmosphere.lapseRateDn = atm.value("lapseRateDn", 40.0);
        l.atmosphere.climate = static_cast<geo::Climate>(atm.value("climate", 5));
        l.atmosphere.kFactorOverridden = atm.value("kFactorOverridden", false);
        l.diversity = diversityFromKey(jl.value("diversity", "quad"));
        l.primaryModel = modelFromKey(jl.value("primaryModel", "p617"));
        l.targetAvailability = Percent(jl.value("targetAvailabilityPct", 99.9));
        l.targetIsWorstMonth = jl.value("targetIsWorstMonth", false);
        l.antennaDiameter = Meters(jl.value("antennaDiameterM", 3.0));
        p.links.push_back(std::move(l));
    }
    for (const auto& js : doc.value("snapshots", json::array())) {
        ResultSnapshot s;
        s.linkId = js.value("linkId", "");
        s.createdIso8601 = js.value("created", "");
        s.appVersion = js.value("appVersion", "");
        s.gdalVersion = js.value("gdalVersion", "");
        s.geographicLibVersion = js.value("geographicLibVersion", "");
        s.reportContentSha256 = js.value("reportContentSha256", "");
        // Hoisted: iterating `temporary.items()` dangles in C++20 (P2644 is C++23).
        const json modelVersions = js.value("modelVersions", json::object());
        for (const auto& [k, v] : modelVersions.items()) {
            s.modelVersions[k] = v.get<std::string>();
        }
        const json values = js.value("values", json::object());
        for (const auto& [k, v] : values.items()) {
            s.values[k] = v.get<double>();
        }
        p.snapshots.push_back(std::move(s));
    }
    return p;
}

Status saveProject(const Project& project, const std::string& utf8Path) {
    const std::string payload = projectToJson(project);
    int errorCode = 0;
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);
    if (archive == nullptr) {
        return Error{"cannot create project file (libzip error " + std::to_string(errorCode) + ")"};
    }
    zip_source_t* source = zip_source_buffer(archive, payload.data(), payload.size(), 0);
    if (source == nullptr || zip_file_add(archive, "project.json", source, ZIP_FL_OVERWRITE) < 0) {
        if (source != nullptr) {
            zip_source_free(source);
        }
        zip_discard(archive);
        return Error{"cannot write project.json into the archive"};
    }
    if (zip_close(archive) != 0) {
        return Error{"cannot finalize project file"};
    }
    return Status::ok();
}

Expected<Project> loadProject(const std::string& utf8Path) {
    int errorCode = 0;
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_RDONLY, &errorCode);
    if (archive == nullptr) {
        return Error{"cannot open project file (libzip error " + std::to_string(errorCode) + ")"};
    }
    zip_stat_t st;
    if (zip_stat(archive, "project.json", 0, &st) != 0) {
        zip_discard(archive);
        return Error{"project archive holds no project.json"};
    }
    zip_file_t* file = zip_fopen(archive, "project.json", 0);
    if (file == nullptr) {
        zip_discard(archive);
        return Error{"cannot read project.json"};
    }
    std::string payload(st.size, '\0');
    const auto read = zip_fread(file, payload.data(), payload.size());
    zip_fclose(file);
    zip_discard(archive);
    if (read < 0 || static_cast<zip_uint64_t>(read) != st.size) {
        return Error{"short read on project.json"};
    }
    return projectFromJson(payload);
}

Project referenceProject() {
    Project p;
    p.name = "Reference link - western Poland";
    p.id = "tlk-reference-0001";
    Site a;
    a.id = "site-a";
    a.name = "Site A";
    a.position = geo::GeoPoint{Degrees(51.50609699), Degrees(15.33150851)};
    a.antennaHeightAgl = Meters(4.0);
    Site b;
    b.id = "site-b";
    b.name = "Site B";
    b.position = geo::GeoPoint{Degrees(52.43470597), Degrees(15.21931198)};
    b.antennaHeightAgl = Meters(4.0);
    p.sites = {a, b};

    LinkSpec l;
    l.id = "link-1";
    l.name = "A-B reference";
    l.siteAId = "site-a";
    l.siteBId = "site-b";
    l.frequency = Hertz::fromGigahertz(4.4);
    l.radio.txPower = Dbm(57.0); // 500 W
    l.radio.antennaGainA = Dbi(39.1);
    l.radio.antennaGainB = Dbi(39.1);
    l.radio.lineLossA = Decibels(0.5);
    l.radio.lineLossB = Decibels(0.5);
    l.radio.noiseFigure = Decibels(4.0);
    l.radio.modulation = budget::Modulation{"QPSK", 2.0, Decibels(10.5), 0.35};
    l.radio.dataRate = BitsPerSecond::fromMegabits(2.0);
    l.diversity = budget::DiversityMode::Quad;
    p.links = {l};
    return p;
}

} // namespace tl::project
