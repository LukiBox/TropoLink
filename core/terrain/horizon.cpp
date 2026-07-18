#include "core/terrain/horizon.h"

#include "core/geo/atmosphere.h"

#include <algorithm>
#include <cmath>

namespace tl::terrain {

Meters earthBulge(Meters d1, Meters total, double kFactor) {
    const double ka = kFactor * geo::Atmosphere::kEarthRadiusKm * 1000.0;
    const double x = d1.value();
    const double d = total.value();
    return Meters(x * (d - x) / (2.0 * ka));
}

Meters fresnelRadius1(Meters d1, Meters total, Hertz frequency) {
    const double c = 299792458.0;
    const double lambda = c / frequency.value();
    const double x = d1.value();
    const double d = total.value();
    if (x <= 0.0 || x >= d) {
        return Meters(0.0);
    }
    return Meters(std::sqrt(lambda * x * (d - x) / d));
}

HorizonResult analyzeHorizons(const Profile& profile, const HorizonRequest& request) {
    HorizonResult result;
    if (profile.points.size() < 3) {
        return result;
    }
    const double ka = request.kFactor * geo::Atmosphere::kEarthRadiusKm * 1000.0;
    const double d = profile.totalDistance.value();
    const double hA = profile.points.front().elevation.value() + request.antennaHeightAglA.value();
    const double hB = profile.points.back().elevation.value() + request.antennaHeightAglB.value();
    const std::size_t n = profile.points.size();

    // Takeoff angle from A: maximum over interior points of the elevation angle with
    // effective-earth correction. Smooth-earth grazing if every angle is below the
    // smooth-earth horizon ray (negative takeoff angles are legitimate).
    double bestA = -1e9;
    std::size_t bestAIdx = 1;
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double di = profile.points[i].distance.value();
        const double hi = profile.points[i].elevation.value();
        const double angle = (hi - hA) / di - di / (2.0 * ka);
        if (angle > bestA) {
            bestA = angle;
            bestAIdx = i;
        }
    }
    double bestB = -1e9;
    std::size_t bestBIdx = n - 2;
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double di = d - profile.points[i].distance.value();
        const double hi = profile.points[i].elevation.value();
        const double angle = (hi - hB) / di - di / (2.0 * ka);
        if (angle > bestB) {
            bestB = angle;
            bestBIdx = i;
        }
    }

    result.takeoffAngleA = Radians(bestA);
    result.takeoffAngleB = Radians(bestB);
    result.horizonDistanceA = profile.points[bestAIdx].distance;
    result.horizonDistanceB = Meters(d) - profile.points[bestBIdx].distance;
    result.horizonHeightA = profile.points[bestAIdx].elevation;
    result.horizonHeightB = profile.points[bestBIdx].elevation;

    // Direct-ray clearance: straight line from antenna A to antenna B in the
    // effective-earth representation (terrain lifted by the bulge).
    double worst = 1e18;
    double worstAt = 0.0;
    bool obstructed = false;
    bool fresnelClear = true;
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double x = profile.points[i].distance.value();
        const double rayH = hA + (hB - hA) * (x / d);
        const double terrainEff =
            profile.points[i].elevation.value() + earthBulge(Meters(x), Meters(d), request.kFactor).value();
        const double clearance = rayH - terrainEff;
        if (clearance < worst) {
            worst = clearance;
            worstAt = x;
        }
        if (clearance < 0.0) {
            obstructed = true;
        }
        const double f1 = fresnelRadius1(Meters(x), Meters(d), request.frequency).value();
        if (clearance < 0.6 * f1) {
            fresnelClear = false;
        }
    }
    result.directPathObstructed = obstructed;
    result.fresnelZoneClear = fresnelClear;
    result.worstClearance = Meters(worst);
    result.worstClearanceDistance = Meters(worstAt);
    return result;
}

} // namespace tl::terrain
