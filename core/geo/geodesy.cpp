#include "core/geo/geodesy.h"

#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/GeodesicLine.hpp>

namespace tl::geo {

namespace {
const GeographicLib::Geodesic& wgs84() { return GeographicLib::Geodesic::WGS84(); }

// GeographicLib returns azimuths in (-180, 180]; operators expect [0, 360).
double normalizeAzimuth(double azDeg) {
    if (azDeg < 0.0) {
        azDeg += 360.0;
    }
    return azDeg;
}
} // namespace

InverseResult Geodesy::inverse(const GeoPoint& a, const GeoPoint& b) {
    double s12 = 0.0;
    double azi1 = 0.0;
    double azi2 = 0.0;
    wgs84().Inverse(a.latitude.value(), a.longitude.value(), b.latitude.value(), b.longitude.value(), s12,
                    azi1, azi2);
    // azi2 is the forward azimuth at point 2; the azimuth from B back towards A is azi2 + 180.
    return InverseResult{Meters(s12), Degrees(normalizeAzimuth(azi1)),
                         Degrees(normalizeAzimuth(azi2 + 180.0 > 360.0 ? azi2 - 180.0 : azi2 + 180.0))};
}

GeoPoint Geodesy::direct(const GeoPoint& start, Degrees azimuth, Meters distance) {
    double lat2 = 0.0;
    double lon2 = 0.0;
    wgs84().Direct(start.latitude.value(), start.longitude.value(), azimuth.value(), distance.value(), lat2,
                   lon2);
    return GeoPoint{Degrees(lat2), Degrees(lon2)};
}

std::vector<PathSample> Geodesy::sampleLine(const GeoPoint& a, const GeoPoint& b, int n) {
    std::vector<PathSample> samples;
    if (n < 2) {
        n = 2;
    }
    samples.reserve(static_cast<std::size_t>(n));

    double s12 = 0.0;
    double azi1 = 0.0;
    double azi2 = 0.0;
    wgs84().Inverse(a.latitude.value(), a.longitude.value(), b.latitude.value(), b.longitude.value(), s12,
                    azi1, azi2);
    const GeographicLib::GeodesicLine line =
        wgs84().Line(a.latitude.value(), a.longitude.value(), azi1,
                     GeographicLib::Geodesic::LATITUDE | GeographicLib::Geodesic::LONGITUDE |
                         GeographicLib::Geodesic::DISTANCE_IN);

    for (int i = 0; i < n; ++i) {
        const double s = s12 * static_cast<double>(i) / static_cast<double>(n - 1);
        double lat = 0.0;
        double lon = 0.0;
        line.Position(s, lat, lon);
        samples.push_back(PathSample{GeoPoint{Degrees(lat), Degrees(lon)}, Meters(s)});
    }
    return samples;
}

} // namespace tl::geo
