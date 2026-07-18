#pragma once

// WGS-84 geodesics via GeographicLib (Karney's method). Never haversine, never
// spherical approximations. Reference: C. F. F. Karney, "Algorithms for geodesics",
// J. Geodesy 87, 43-55 (2013).

#include "core/common/units.h"

#include <vector>

namespace tl::geo {

struct GeoPoint {
    Degrees latitude;
    Degrees longitude;
};

struct InverseResult {
    Meters distance;           // geodesic distance s12
    Degrees forwardAzimuth;    // azimuth at point 1, clockwise from true north
    Degrees reverseAzimuth;    // azimuth of the path *back* from point 2 towards point 1
};

struct PathSample {
    GeoPoint point;
    Meters distanceFromStart;
};

class Geodesy {
public:
    // Great-ellipse inverse problem: distance and both azimuths.
    [[nodiscard]] static InverseResult inverse(const GeoPoint& a, const GeoPoint& b);

    // Direct problem: destination given start, azimuth and distance.
    [[nodiscard]] static GeoPoint direct(const GeoPoint& start, Degrees azimuth, Meters distance);

    // n evenly spaced samples along the geodesic from a to b, inclusive of both ends (n >= 2).
    [[nodiscard]] static std::vector<PathSample> sampleLine(const GeoPoint& a, const GeoPoint& b, int n);
};

} // namespace tl::geo
