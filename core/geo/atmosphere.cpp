#include "core/geo/atmosphere.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tl::geo {

const char* climateName(Climate climate) {
    switch (climate) {
    case Climate::Equatorial:
        return "Equatorial";
    case Climate::ContinentalSubtropical:
        return "Continental subtropical";
    case Climate::MaritimeSubtropical:
        return "Maritime subtropical";
    case Climate::Desert:
        return "Desert";
    case Climate::ContinentalTemperate:
        return "Continental temperate";
    case Climate::MaritimeTemperateOverLand:
        return "Maritime temperate over land";
    case Climate::MaritimeTemperateOverSea:
        return "Maritime temperate over sea";
    }
    return "Unknown";
}

double Atmosphere::surfaceRefractivityAt(Meters heightAmsl) const {
    constexpr double scaleHeightKm = 7.35; // ITU-R P.617-5 hb
    return seaLevelN0 * std::exp(-heightAmsl.kilometers() / scaleHeightKm);
}

double Atmosphere::kFromLapseRate(double dnPerKm) {
    const double denominator = 157.0 - dnPerKm;
    // Guard super-refractive gradients approaching ducting (denominator -> 0): clamp to
    // a large-but-finite k. Sub-refractive dN < 0 simply produces k < 1, which must work.
    if (denominator < 1.0) {
        return 157.0;
    }
    return 157.0 / denominator;
}

double Atmosphere::lapseRateFromNs(double ns) { return 7.32 * std::exp(0.005577 * ns); }

Climate defaultClimateFor(const GeoPoint& p) {
    const double absLat = std::abs(p.latitude.value());
    if (absLat < 10.0) {
        return Climate::Equatorial;
    }
    if (absLat < 23.5) {
        return Climate::ContinentalSubtropical;
    }
    return Climate::ContinentalTemperate;
}

Expected<RefractivityMaps> RefractivityMaps::load(const std::string& directory) {
    auto loadGrid = [](const std::string& path, std::vector<double>& grid) -> Status {
        // UTF-8 path -> filesystem::path so non-ASCII install paths work on Windows.
        std::ifstream in(std::filesystem::path(std::u8string(path.begin(), path.end())));
        if (!in) {
            return Error{"cannot open refractivity map: " + path};
        }
        grid.clear();
        grid.reserve(static_cast<std::size_t>(kRows) * kCols);
        double value = 0.0;
        while (in >> value) {
            grid.push_back(value);
        }
        if (grid.size() != static_cast<std::size_t>(kRows) * kCols) {
            return Error{"refractivity map has unexpected size: " + path};
        }
        return Status::ok();
    };

    RefractivityMaps maps;
    if (auto s = loadGrid(directory + "/N050.TXT", maps.n0_); !s) {
        return s.error();
    }
    if (auto s = loadGrid(directory + "/DN50.TXT", maps.dn_); !s) {
        return s.error();
    }
    return maps;
}

double RefractivityMaps::sample(const std::vector<double>& grid, const GeoPoint& p) {
    // Grid: first row at 90N, spacing 1.5 degrees; first column at 0E, spacing 1.5 degrees,
    // last column duplicates the first (360 == 0) to simplify interpolation.
    constexpr double spacing = 1.5;
    double lon = p.longitude.value();
    if (lon < 0.0) {
        lon += 360.0;
    }
    const double row = (90.0 - p.latitude.value()) / spacing;
    const double col = lon / spacing;
    const int r0 = std::clamp(static_cast<int>(std::floor(row)), 0, kRows - 2);
    const int c0 = std::clamp(static_cast<int>(std::floor(col)), 0, kCols - 2);
    const double fr = std::clamp(row - r0, 0.0, 1.0);
    const double fc = std::clamp(col - c0, 0.0, 1.0);
    auto at = [&grid](int r, int c) { return grid[static_cast<std::size_t>(r) * kCols + c]; };
    return (1.0 - fr) * ((1.0 - fc) * at(r0, c0) + fc * at(r0, c0 + 1)) +
           fr * ((1.0 - fc) * at(r0 + 1, c0) + fc * at(r0 + 1, c0 + 1));
}

double RefractivityMaps::n0At(const GeoPoint& p) const { return sample(n0_, p); }

double RefractivityMaps::dnAt(const GeoPoint& p) const { return sample(dn_, p); }

Atmosphere RefractivityMaps::atmosphereAt(const GeoPoint& midpoint) const {
    Atmosphere atm;
    atm.seaLevelN0 = n0At(midpoint);
    atm.lapseRateDn = dnAt(midpoint);
    atm.kFactor = Atmosphere::kFromLapseRate(atm.lapseRateDn);
    atm.climate = defaultClimateFor(midpoint);
    return atm;
}

} // namespace tl::geo
