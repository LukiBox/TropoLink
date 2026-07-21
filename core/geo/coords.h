#pragma once

// Coordinate I/O in all operator formats: decimal degrees, DMS, MGRS (first-class,
// the military standard) and UTM. Parsing is forgiving: pasted strings in any of the
// four formats are accepted. Conversions via GeographicLib MGRS/UTMUPS.

#include "core/common/expected.h"
#include "core/geo/geodesy.h"

#include <string>

namespace tl::geo {

enum class CoordFormat { DecimalDegrees, Dms, Mgrs, Utm };

class Coords {
  public:
    // Accepts, among others:
    //   "51.50609699N, 15.33150851E"   "51.506097, 15.331509"   "-33.86 151.21"
    //   "51°30'21.9\"N 15°19'53.4\"E"  "51 30 21.9 N, 15 19 53.4 E"  "51d30m21.9sN ..."
    //   "33UXT 66055 07249"            "33UXT6605507249"
    //   "33U 466055 5707249"
    [[nodiscard]] static Expected<GeoPoint> parse(const std::string& text);

    [[nodiscard]] static std::string formatDecimalDegrees(const GeoPoint& p, int decimals = 6);
    [[nodiscard]] static std::string formatDms(const GeoPoint& p, int secondsDecimals = 2);
    // MGRS at 1 m precision (5+5 digits) by default.
    [[nodiscard]] static Expected<std::string> formatMgrs(const GeoPoint& p, int precision = 5);
    [[nodiscard]] static Expected<std::string> formatUtm(const GeoPoint& p);

    [[nodiscard]] static Expected<std::string> format(const GeoPoint& p, CoordFormat format);
};

} // namespace tl::geo
