#include "core/geo/coords.h"

#include <GeographicLib/MGRS.hpp>
#include <GeographicLib/UTMUPS.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
#include <regex>

namespace tl::geo {

namespace {

std::string trimmed(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool validLatLon(double lat, double lon) {
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

// --- MGRS ------------------------------------------------------------------

std::optional<GeoPoint> tryParseMgrs(const std::string& raw) {
    std::string s = toUpper(raw);
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c) != 0; }),
            s.end());
    // Grid-zone designator + 100 km square + even number of digits, e.g. 33UXT6605507249.
    static const std::regex mgrsRe(R"(^\d{1,2}[C-HJ-NP-X][A-HJ-NP-Z]{2}(\d{0,10})$)");
    std::smatch m;
    if (!std::regex_match(s, m, mgrsRe) || (m[1].length() % 2) != 0) {
        return std::nullopt;
    }
    try {
        int zone = 0;
        bool northp = false;
        double x = 0.0;
        double y = 0.0;
        int precision = 0;
        GeographicLib::MGRS::Reverse(s, zone, northp, x, y, precision, true);
        double lat = 0.0;
        double lon = 0.0;
        GeographicLib::UTMUPS::Reverse(zone, northp, x, y, lat, lon);
        return GeoPoint{Degrees(lat), Degrees(lon)};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// --- UTM -------------------------------------------------------------------

std::optional<GeoPoint> tryParseUtm(const std::string& raw) {
    // "33U 466055 5707249" (band letter) or "33N 466055 5707249" (hemisphere N/S).
    static const std::regex utmRe(
        R"(^\s*(\d{1,2})\s*([A-Za-z])\s+(\d+(?:\.\d+)?)\s*[,;]?\s+(\d+(?:\.\d+)?)\s*$)");
    std::smatch m;
    if (!std::regex_match(raw, m, utmRe)) {
        return std::nullopt;
    }
    try {
        const int zone = std::stoi(m[1].str());
        const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(m[2].str()[0])));
        const double easting = std::stod(m[3].str());
        const double northing = std::stod(m[4].str());
        if (zone < 1 || zone > 60) {
            return std::nullopt;
        }
        // Band letters C..M are the southern hemisphere; N..X northern. Bare N/S also accepted.
        bool northp = true;
        if (letter == 'S' || (letter >= 'C' && letter <= 'M')) {
            northp = false;
        }
        double lat = 0.0;
        double lon = 0.0;
        GeographicLib::UTMUPS::Reverse(zone, northp, easting, northing, lat, lon);
        if (!validLatLon(lat, lon)) {
            return std::nullopt;
        }
        return GeoPoint{Degrees(lat), Degrees(lon)};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// --- DMS / decimal degrees -------------------------------------------------

// One angle: decimal degrees or degrees-minutes-seconds with °'" or d/m/s or spaces,
// with optional hemisphere letter before or after.
struct ParsedAngle {
    double degrees;
    char hemisphere; // 'N','S','E','W' or 0
};

std::optional<ParsedAngle> parseAngle(std::string text) {
    text = trimmed(text);
    if (text.empty()) {
        return std::nullopt;
    }
    char hemi = 0;
    auto takeHemisphere = [&](char c) {
        const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (u == 'N' || u == 'S' || u == 'E' || u == 'W') {
            hemi = u;
            return true;
        }
        return false;
    };
    if (takeHemisphere(text.front())) {
        text = trimmed(text.substr(1));
    } else if (takeHemisphere(text.back())) {
        text = trimmed(text.substr(0, text.size() - 1));
    }

    // Normalize unicode degree sign and prime characters to plain separators.
    static const std::array<std::pair<const char*, char>, 6> replacements{{{"\xC2\xB0", ' '},     // °
                                                                           {"\xE2\x80\xB2", ' '}, // ′
                                                                           {"\xE2\x80\xB3", ' '}, // ″
                                                                           {"\xE2\x80\x99", ' '}, // ’
                                                                           {"\xE2\x80\x9D", ' '}, // ”
                                                                           {"\xEF\xBF\xBD", ' '}}};
    for (const auto& [seq, repl] : replacements) {
        std::size_t pos = 0;
        const std::string needle(seq);
        while ((pos = text.find(needle, pos)) != std::string::npos) {
            text.replace(pos, needle.size(), 1, repl);
        }
    }
    for (char& c : text) {
        const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (c == '\'' || c == '"' || u == 'D' || u == 'M' || u == 'S' || c == ':') {
            c = ' ';
        }
    }

    static const std::regex angleRe(
        R"(^\s*([+-]?\d+(?:\.\d+)?)(?:\s+(\d+(?:\.\d+)?))?(?:\s+(\d+(?:\.\d+)?))?\s*$)");
    std::smatch m;
    if (!std::regex_match(text, m, angleRe)) {
        return std::nullopt;
    }
    const double deg = std::stod(m[1].str());
    const double minutes = m[2].matched ? std::stod(m[2].str()) : 0.0;
    const double seconds = m[3].matched ? std::stod(m[3].str()) : 0.0;
    if (minutes >= 60.0 || seconds >= 60.0) {
        return std::nullopt;
    }
    const double sign = (deg < 0.0 || m[1].str().front() == '-') ? -1.0 : 1.0;
    const double magnitude = std::abs(deg) + minutes / 60.0 + seconds / 3600.0;
    return ParsedAngle{sign * magnitude, hemi};
}

std::optional<GeoPoint> tryParseLatLonPair(const std::string& raw) {
    // Split into two halves. Prefer an explicit comma/semicolon; otherwise split between
    // a trailing hemisphere letter of the first angle and the start of the second, or at
    // the middle whitespace gap.
    std::string s = trimmed(raw);
    std::vector<std::string> halves;
    const auto comma = s.find_first_of(",;");
    if (comma != std::string::npos) {
        halves = {s.substr(0, comma), s.substr(comma + 1)};
    } else {
        // Try splitting after an N/S hemisphere letter (latitude first).
        static const std::regex splitRe(R"(^(.*?[NnSs])\s+(.*)$)");
        std::smatch m;
        if (std::regex_match(s, m, splitRe) && m[2].str().find_first_of("0123456789") != std::string::npos) {
            halves = {m[1].str(), m[2].str()};
        } else {
            // Fall back: split at the middle token gap of a plain "lat lon" pair.
            static const std::regex pairRe(R"(^\s*([+-]?\d+(?:\.\d+)?)\s+([+-]?\d+(?:\.\d+)?)\s*$)");
            if (std::regex_match(s, m, pairRe)) {
                halves = {m[1].str(), m[2].str()};
            } else {
                return std::nullopt;
            }
        }
    }

    const auto a1 = parseAngle(halves[0]);
    const auto a2 = parseAngle(halves[1]);
    if (!a1 || !a2) {
        return std::nullopt;
    }

    double lat = 0.0;
    double lon = 0.0;
    const bool firstIsLon = a1->hemisphere == 'E' || a1->hemisphere == 'W';
    const ParsedAngle& latAngle = firstIsLon ? *a2 : *a1;
    const ParsedAngle& lonAngle = firstIsLon ? *a1 : *a2;
    if (latAngle.hemisphere == 'E' || latAngle.hemisphere == 'W' || lonAngle.hemisphere == 'N' ||
        lonAngle.hemisphere == 'S') {
        return std::nullopt;
    }
    lat = latAngle.degrees * (latAngle.hemisphere == 'S' ? -1.0 : 1.0);
    lon = lonAngle.degrees * (lonAngle.hemisphere == 'W' ? -1.0 : 1.0);
    if (!validLatLon(lat, lon)) {
        return std::nullopt;
    }
    return GeoPoint{Degrees(lat), Degrees(lon)};
}

} // namespace

Expected<GeoPoint> Coords::parse(const std::string& text) {
    const std::string s = trimmed(text);
    if (s.empty()) {
        return Error{"empty coordinate string"};
    }
    if (auto p = tryParseMgrs(s)) {
        return *p;
    }
    if (auto p = tryParseUtm(s)) {
        return *p;
    }
    if (auto p = tryParseLatLonPair(s)) {
        return *p;
    }
    return Error{"unrecognized coordinate format: '" + s + "'"};
}

std::string Coords::formatDecimalDegrees(const GeoPoint& p, int decimals) {
    char buf[80];
    const double lat = p.latitude.value();
    const double lon = p.longitude.value();
    std::snprintf(buf, sizeof(buf), "%.*f%c %.*f%c", decimals, std::abs(lat), lat >= 0.0 ? 'N' : 'S',
                  decimals, std::abs(lon), lon >= 0.0 ? 'E' : 'W');
    return buf;
}

std::string Coords::formatDms(const GeoPoint& p, int secondsDecimals) {
    auto formatOne = [secondsDecimals](double deg, char pos, char neg) {
        const char hemi = deg >= 0.0 ? pos : neg;
        double magnitude = std::abs(deg);
        int d = static_cast<int>(magnitude);
        double minutesF = (magnitude - d) * 60.0;
        int m = static_cast<int>(minutesF);
        double s = (minutesF - m) * 60.0;
        // Guard against 60.0" from floating rounding.
        const double roundUnit = std::pow(10.0, -secondsDecimals);
        if (s >= 60.0 - roundUnit / 2.0) {
            s = 0.0;
            if (++m == 60) {
                m = 0;
                ++d;
            }
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d\xC2\xB0%02d'%0*.*f\"%c", d, m,
                      secondsDecimals > 0 ? secondsDecimals + 3 : 2, secondsDecimals, s, hemi);
        return std::string(buf);
    };
    return formatOne(p.latitude.value(), 'N', 'S') + " " + formatOne(p.longitude.value(), 'E', 'W');
}

Expected<std::string> Coords::formatMgrs(const GeoPoint& p, int precision) {
    try {
        int zone = 0;
        bool northp = false;
        double x = 0.0;
        double y = 0.0;
        GeographicLib::UTMUPS::Forward(p.latitude.value(), p.longitude.value(), zone, northp, x, y);
        std::string mgrs;
        GeographicLib::MGRS::Forward(zone, northp, x, y, p.latitude.value(), precision, mgrs);
        return mgrs;
    } catch (const std::exception& e) {
        return Error{std::string("MGRS conversion failed: ") + e.what()};
    }
}

Expected<std::string> Coords::formatUtm(const GeoPoint& p) {
    try {
        int zone = 0;
        bool northp = false;
        double x = 0.0;
        double y = 0.0;
        GeographicLib::UTMUPS::Forward(p.latitude.value(), p.longitude.value(), zone, northp, x, y);
        // UTM latitude bands C..X, 8 degrees each, letters I and O skipped, X reaching 84 N.
        const double lat = p.latitude.value();
        if (lat < -80.0 || lat > 84.0) {
            return Error{"latitude outside UTM band coverage (use UPS)"};
        }
        static const char bands[] = "CDEFGHJKLMNPQRSTUVWX";
        int idx = static_cast<int>(std::floor((lat + 80.0) / 8.0));
        idx = std::clamp(idx, 0, 19);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d%c %.0f %.0f", zone, bands[idx], x, y);
        return std::string(buf);
    } catch (const std::exception& e) {
        return Error{std::string("UTM conversion failed: ") + e.what()};
    }
}

Expected<std::string> Coords::format(const GeoPoint& p, CoordFormat format) {
    switch (format) {
    case CoordFormat::DecimalDegrees:
        return formatDecimalDegrees(p);
    case CoordFormat::Dms:
        return formatDms(p);
    case CoordFormat::Mgrs:
        return formatMgrs(p);
    case CoordFormat::Utm:
        return formatUtm(p);
    }
    return Error{"unknown coordinate format"};
}

} // namespace tl::geo
