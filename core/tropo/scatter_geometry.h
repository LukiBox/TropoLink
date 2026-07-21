#pragma once

// Troposcatter path geometry: scatter angle and common scattering volume.
//
// Frame: flat-earth representation with effective radius k*a. A ray leaving an
// antenna at height h with local elevation angle tau follows
//     h(x) = h + tau x + x^2 / (2 k a)
// which is exactly the convention used for the horizon angles (TN101 §6, ITM).
//
// Scatter angle:  theta = theta_t + theta_r + d/(k a)     [ITU-R P.617-5 eq. (1)-(2)]
// Common-volume height above sea level: intersection of the two horizon rays
// (closed form; equivalent to P.617-5 eq. (7a) to small-angle order).

#include "core/common/units.h"
#include "core/geo/geodesy.h"

namespace tl::tropo {

struct ScatterGeometryInput {
    Meters pathLength{0.0};
    Radians takeoffA{0.0}; // horizon angle from site A (radians, may be negative)
    Radians takeoffB{0.0};
    Meters antennaAmslA{0.0}; // antenna centre height above sea level
    Meters antennaAmslB{0.0};
    double kFactor = 4.0 / 3.0;
    Dbi gainA{0.0}; // used for the beamwidth-limited top of the volume
    Dbi gainB{0.0};
};

struct ScatterGeometry {
    Radians scatterAngle{0.0};     // theta = theta_t + theta_r + d/(ka)
    Radians angularDistance{0.0};  // d/(ka) term alone
    Meters distanceToVolumeA{0.0}; // along-path distance from A to the volume centre
    Meters distanceToVolumeB{0.0};
    Meters slantRangeA{0.0}; // slant distance antenna -> common volume
    Meters slantRangeB{0.0};
    Meters volumeBaseAmsl{0.0}; // crossover of the two horizon rays
    Meters volumeTopAmsl{0.0};  // crossover of the two upper half-power beam edges
    Meters verticalExtent{0.0};
    Radians antennaElevationA{0.0}; // pointing elevation towards the volume (== takeoff)
    Radians antennaElevationB{0.0};
    Radians halfPowerBeamwidthA{0.0};
    Radians halfPowerBeamwidthB{0.0};
};

// Half-power beamwidth estimated from gain: theta_3dB(deg) ~ sqrt(27000 / g),
// g = 10^(G/10)  (parabolic-aperture rule of thumb; documented in model_references).
[[nodiscard]] Radians beamwidthFromGain(Dbi gain);

[[nodiscard]] ScatterGeometry computeScatterGeometry(const ScatterGeometryInput& input);

} // namespace tl::tropo
