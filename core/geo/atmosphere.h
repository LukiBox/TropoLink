#pragma once

// Atmospheric refractivity model.
//
// References:
//  * dN follows the ITU-R P.452 convention: the average radio-refractive index
//    lapse-rate through the lowest 1 km, a POSITIVE quantity under standard
//    refraction (dN ~ 40 over mid-latitude land).
//  * Effective earth radius factor from the lapse rate:
//        k = 157 / (157 - dN)          [ITU-R P.452; dN = 40 gives k ~ 4/3;
//                                       sub-refractive dN < 0 gives k < 1]
//  * Lapse rate estimated from surface refractivity:
//        dN = 7.32 exp(0.005577 Ns)    [NBS TN101, eq. 4.3, sign per P.452 convention]
//  * Exponential atmosphere:  N(h) = N0 exp(-h / hb), hb = 7.35 km  [ITU-R P.453/P.617]
//  * N0 and dN digital maps: N050.TXT / DN50.TXT from ITU-R P.452 (integral part of
//    ITU-R P.617-5, Table 1): 1.5-degree grid, 121 rows from 90N, 241 columns from 0E.

#include "core/common/expected.h"
#include "core/common/units.h"
#include "core/geo/geodesy.h"

#include <string>
#include <vector>

namespace tl::geo {

// ITM / TN101 radio climate regions (numbering follows NTIA ITM).
enum class Climate {
    Equatorial = 1,
    ContinentalSubtropical = 2,
    MaritimeSubtropical = 3,
    Desert = 4,
    ContinentalTemperate = 5,
    MaritimeTemperateOverLand = 6,
    MaritimeTemperateOverSea = 7,
};

[[nodiscard]] const char* climateName(Climate climate);

struct Atmosphere {
    double kFactor = 4.0 / 3.0; // effective earth radius factor (user-overridable)
    double seaLevelN0 = 315.0;  // average annual sea-level surface refractivity
    double lapseRateDn = 40.0;  // dN through lowest 1 km, N-units/km, positive per P.452
    Climate climate = Climate::ContinentalTemperate;
    bool kFactorOverridden = false; // true once the operator forces a k value

    static constexpr double kEarthRadiusKm = 6370.0; // ITU-R P.617-5 eq. (2)

    [[nodiscard]] Meters effectiveEarthRadius() const {
        return Meters::fromKilometers(kFactor * kEarthRadiusKm);
    }
    // Surface refractivity at height h above sea level (exponential atmosphere).
    [[nodiscard]] double surfaceRefractivityAt(Meters heightAmsl) const;

    // k derived from the lapse rate:  k = 157 / (157 - dN). Sub-refractive dN < 0 yields k < 1.
    [[nodiscard]] static double kFromLapseRate(double dnPerKm);
    // Lapse rate estimated from surface refractivity Ns (TN101 eq. 4.3, P.452 sign).
    [[nodiscard]] static double lapseRateFromNs(double ns);
};

// Rough default climate pick from coordinates; documented heuristic, always
// operator-overridable. Poland resolves to ContinentalTemperate.
[[nodiscard]] Climate defaultClimateFor(const GeoPoint& p);

// Reader for the ITU-R P.452 / P.617-5 digital maps (N050.TXT, DN50.TXT).
class RefractivityMaps {
  public:
    // Loads both maps from a directory containing N050.TXT and DN50.TXT.
    [[nodiscard]] static Expected<RefractivityMaps> load(const std::string& directory);

    [[nodiscard]] double n0At(const GeoPoint& p) const;
    [[nodiscard]] double dnAt(const GeoPoint& p) const;

    // Atmosphere defaulted from the maps at a path midpoint (k from dN, climate from latitude).
    [[nodiscard]] Atmosphere atmosphereAt(const GeoPoint& midpoint) const;

  private:
    static constexpr int kRows = 121;
    static constexpr int kCols = 241;
    std::vector<double> n0_;
    std::vector<double> dn_;

    [[nodiscard]] static double sample(const std::vector<double>& grid, const GeoPoint& p);
};

} // namespace tl::geo
