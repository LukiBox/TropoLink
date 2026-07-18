#pragma once

// .tlk project files: versioned JSON inside a zip container. A project holds multiple
// sites, multiple links, radio parameters, equipment references and frozen result
// snapshots carrying the app and model versions that produced them plus library
// versions (GDAL, GeographicLib) — the audit trail that makes a design reproducible
// years later.

#include "core/budget/availability.h"
#include "core/budget/link_budget.h"
#include "core/common/expected.h"
#include "core/geo/atmosphere.h"
#include "core/geo/geodesy.h"
#include "core/tropo/loss_model.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tl::project {

inline constexpr int kSchemaVersion = 1;

struct Site {
    std::string id;
    std::string name;
    geo::GeoPoint position;
    Meters antennaHeightAgl{4.0};
};

struct LinkSpec {
    std::string id;
    std::string name;
    std::string siteAId;
    std::string siteBId;
    Hertz frequency = Hertz::fromGigahertz(4.4);
    budget::RadioParams radio;
    geo::Atmosphere atmosphere;
    budget::DiversityMode diversity = budget::DiversityMode::Quad;
    tropo::ModelId primaryModel = tropo::ModelId::P617;
    Percent targetAvailability{99.9};
    bool targetIsWorstMonth = false;
    Meters antennaDiameter{3.0}; // for the diversity separation calculator
};

// Frozen numbers from a completed computation — auditability, not live state.
struct ResultSnapshot {
    std::string linkId;
    std::string createdIso8601;
    std::string appVersion;
    std::string gdalVersion;
    std::string geographicLibVersion;
    std::map<std::string, std::string> modelVersions; // model name -> citation string
    std::map<std::string, double> values;             // canonical key -> value
    std::string reportContentSha256;
};

struct Project {
    int schemaVersion = kSchemaVersion;
    std::string name = "Untitled";
    std::string id;
    std::vector<Site> sites;
    std::vector<LinkSpec> links;
    std::vector<ResultSnapshot> snapshots;

    [[nodiscard]] const Site* findSite(const std::string& siteId) const;
};

// Zip container I/O. Lossless round-trip is a test requirement.
[[nodiscard]] Status saveProject(const Project& project, const std::string& utf8Path);
[[nodiscard]] Expected<Project> loadProject(const std::string& utf8Path);

// JSON (de)serialization, exposed for tests and for the report audit page.
[[nodiscard]] std::string projectToJson(const Project& project);
[[nodiscard]] Expected<Project> projectFromJson(const std::string& jsonText);

// The reference scenario (§9 of the design brief), preloaded on first run.
[[nodiscard]] Project referenceProject();

} // namespace tl::project
