#pragma once

// Radio-horizon geometry over the extracted profile: takeoff angles, horizon
// distances, direct-ray obstruction and first-Fresnel-zone clearance.
//
// All angles use the flat-earth-with-effective-radius representation: the elevation
// angle from an antenna at height h_a to profile point i at distance d_i is
//     theta_i = (h_i - h_a) / d_i  -  d_i / (2 k a)      [radians]
// (NBS TN101 §6; ITM uses the identical construction in FindHorizons).

#include "core/common/units.h"
#include "core/terrain/profile.h"

namespace tl::terrain {

struct HorizonResult {
    Radians takeoffAngleA{0.0}; // horizon elevation angle from site A (can be negative)
    Radians takeoffAngleB{0.0};
    Meters horizonDistanceA{0.0}; // distance from A to its horizon point
    Meters horizonDistanceB{0.0}; // distance from B to its horizon point
    Meters horizonHeightA{0.0};   // terrain elevation AMSL at A's horizon point
    Meters horizonHeightB{0.0};
    bool directPathObstructed = false;  // ray A->B intersects terrain
    bool fresnelZoneClear = false;      // >= 60% of first Fresnel radius everywhere
    Meters worstClearance{0.0};         // minimum clearance of the direct ray over terrain
    Meters worstClearanceDistance{0.0}; // where it occurs (from A)
};

struct HorizonRequest {
    Meters antennaHeightAglA{10.0};
    Meters antennaHeightAglB{10.0};
    double kFactor = 4.0 / 3.0;
    Hertz frequency = Hertz::fromGigahertz(4.4); // for Fresnel radius
};

[[nodiscard]] HorizonResult analyzeHorizons(const Profile& profile, const HorizonRequest& request);

// Effective-earth curvature bulge at along-path distance d1 from A on a path of
// length d:  c(d1) = d1 (d - d1) / (2 k a). Used by the profile view as well.
[[nodiscard]] Meters earthBulge(Meters d1, Meters total, double kFactor);

// First Fresnel zone radius at distance d1 from A (d2 = d - d1).
[[nodiscard]] Meters fresnelRadius1(Meters d1, Meters total, Hertz frequency);

} // namespace tl::terrain
