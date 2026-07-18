#include "core/tropo/scatter_geometry.h"

#include "core/geo/atmosphere.h"

#include <algorithm>
#include <cmath>

namespace tl::tropo {

Radians beamwidthFromGain(Dbi gain) {
    const double g = std::pow(10.0, gain.value() / 10.0);
    const double deg = std::sqrt(27000.0 / std::max(g, 1.0));
    return Radians::fromDegrees(deg);
}

namespace {
// Crossover of a ray from A (elevation tauA) and a ray from B (elevation tauB) in the
// flat-earth/effective-radius frame. Returns along-path distance from A. The quadratic
// curvature terms cancel to a linear equation:
//   x* = (hB - hA + tauB d + d^2/(2ka)) / (tauA + tauB + d/(ka))
double crossoverDistance(double hA, double hB, double tauA, double tauB, double d, double ka) {
    const double theta = tauA + tauB + d / ka;
    if (theta <= 1e-9) {
        return d * 0.5; // degenerate (would-be LOS); centre of path
    }
    return (hB - hA + tauB * d + d * d / (2.0 * ka)) / theta;
}

double rayHeight(double h0, double tau, double x, double ka) { return h0 + tau * x + x * x / (2.0 * ka); }
} // namespace

ScatterGeometry computeScatterGeometry(const ScatterGeometryInput& input) {
    ScatterGeometry out;
    const double d = input.pathLength.value();
    const double ka = input.kFactor * geo::Atmosphere::kEarthRadiusKm * 1000.0;
    const double tauA = input.takeoffA.value();
    const double tauB = input.takeoffB.value();
    const double hA = input.antennaAmslA.value();
    const double hB = input.antennaAmslB.value();

    out.angularDistance = Radians(d / ka);
    out.scatterAngle = Radians(tauA + tauB + d / ka);
    out.antennaElevationA = input.takeoffA;
    out.antennaElevationB = input.takeoffB;
    out.halfPowerBeamwidthA = beamwidthFromGain(input.gainA);
    out.halfPowerBeamwidthB = beamwidthFromGain(input.gainB);

    // Base of the lens: crossover of the two horizon rays.
    const double xBase = std::clamp(crossoverDistance(hA, hB, tauA, tauB, d, ka), 0.0, d);
    const double hBase = rayHeight(hA, tauA, xBase, ka);

    // Top of the lens: crossover of the upper half-power beam edges.
    const double upA = tauA + out.halfPowerBeamwidthA.value() * 0.5;
    const double upB = tauB + out.halfPowerBeamwidthB.value() * 0.5;
    const double xTop = std::clamp(crossoverDistance(hA, hB, upA, upB, d, ka), 0.0, d);
    const double hTop = rayHeight(hA, upA, xTop, ka);

    out.distanceToVolumeA = Meters(xBase);
    out.distanceToVolumeB = Meters(d - xBase);
    out.volumeBaseAmsl = Meters(hBase);
    out.volumeTopAmsl = Meters(std::max(hTop, hBase));
    out.verticalExtent = Meters(std::max(0.0, hTop - hBase));
    out.slantRangeA = Meters(std::hypot(xBase, hBase - hA));
    out.slantRangeB = Meters(std::hypot(d - xBase, hBase - hB));
    return out;
}

} // namespace tl::tropo
