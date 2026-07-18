#pragma once

// The multi-model suite: FSPL baseline + P.617 + TN101 + ITM computed side by side.
// Model disagreement is information, not embarrassment: the spread of the valid
// medians is reported as the uncertainty band of the design.

#include "core/geo/atmosphere.h"
#include "core/terrain/horizon.h"
#include "core/terrain/profile.h"
#include "core/tropo/itm_model.h"
#include "core/tropo/loss_model.h"
#include "core/tropo/p617.h"
#include "core/tropo/scatter_geometry.h"
#include "core/tropo/tn101.h"

#include <map>
#include <memory>
#include <optional>

namespace tl::tropo {

struct SuiteInput {
    geo::GeoPoint siteA;
    geo::GeoPoint siteB;
    Meters antennaAglA{4.0};
    Meters antennaAglB{4.0};
    Hertz frequency = Hertz::fromGigahertz(4.4);
    Dbi gainA{39.1};
    Dbi gainB{39.1};
    geo::Atmosphere atmosphere;
};

struct ModelRow {
    ModelId id{};
    std::string name;
    std::string citation;
    bool valid = true;
    std::vector<std::string> validityIssues;
    Decibels median{0.0};
    Decibels couplingLoss{0.0};
    // loss not exceeded at kStandardPercentiles, annual and worst-month
    std::array<double, 5> annualDb{};
    std::array<double, 5> worstMonthDb{};
    std::string note; // e.g. ITM propagation mode
};

struct SuiteResult {
    // Geometry block
    geo::InverseResult inverse;
    terrain::HorizonResult horizons;
    ScatterGeometry geometry;
    geo::GeoPoint commonVolumePosition;
    Meters commonVolumeAboveTerrain{0.0};
    Decibels fspl{0.0};
    bool terrainAvailable = false;
    bool profileHasVoids = false;

    // Model comparison
    std::vector<ModelRow> rows;      // FSPL first, then scatter models
    Decibels spread{0.0};            // max - min of valid scatter-model medians
    ModelId primary = ModelId::P617; // headline model (user-selected)

    // Live model handles for the availability/budget engine (primary lookup).
    std::map<ModelId, std::shared_ptr<const LossModel>> models;

    [[nodiscard]] std::shared_ptr<const LossModel> primaryModel() const {
        const auto it = models.find(primary);
        return it != models.end() ? it->second : nullptr;
    }
};

// Runs geometry + every model on an extracted profile. Deterministic.
[[nodiscard]] SuiteResult runSuite(const SuiteInput& input, const terrain::Profile& profile,
                                   ModelId primary = ModelId::P617);

} // namespace tl::tropo
