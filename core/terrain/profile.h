#pragma once

// Path profile extraction: DEM sampled along the WGS-84 geodesic at a resolution
// matched to the data, bilinear interpolation, void handling (voids interpolated from
// neighbours and FLAGGED — never silently invented terrain).
//
// Extraction is multithreaded (std::jthread worker chunks), cancellable through
// std::stop_token, with progress reporting. Output is deterministic: every sample is
// written to its preassigned slot regardless of thread scheduling.

#include "core/common/expected.h"
#include "core/common/units.h"
#include "core/geo/geodesy.h"
#include "core/terrain/terrain_store.h"

#include <functional>
#include <stop_token>
#include <vector>

namespace tl::terrain {

struct ProfilePoint {
    Meters distance{0.0};          // along-path distance from site A
    Meters elevation{0.0};         // terrain elevation AMSL
    bool interpolatedVoid = false; // true where the DEM had a void and we interpolated
    geo::GeoPoint position;
};

struct Profile {
    std::vector<ProfilePoint> points;
    Meters step{0.0};
    Meters totalDistance{0.0};
    bool hasVoids = false;
    bool hasCoverage = false; // false when no DEM covered any part of the path

    [[nodiscard]] Meters elevationAt(Meters distance) const; // linear between samples
    [[nodiscard]] Meters meanElevation() const;
};

struct ProfileRequest {
    geo::GeoPoint siteA;
    geo::GeoPoint siteB;
    // 0 = automatic: finest covering dataset resolution, clamped to [30 m, 90 m].
    Meters step{0.0};
};

using ProgressCallback = std::function<void(double fraction)>;

[[nodiscard]] Expected<Profile> extractProfile(const TerrainStore& store, const ProfileRequest& request,
                                               std::stop_token stopToken = {},
                                               const ProgressCallback& progress = {});

} // namespace tl::terrain
