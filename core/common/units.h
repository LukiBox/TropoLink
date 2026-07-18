#pragma once

// Strong unit types for every physical quantity in TropoLink.
//
// Design rules:
//  * Linear quantities (length, frequency, power, angle, rate) are distinct types;
//    cross-dimension arithmetic does not compile.
//  * The decibel family models the RF algebra:
//        Dbm  + Decibels -> Dbm        (level shifted by gain/loss)
//        Dbm  - Decibels -> Dbm
//        Dbm  - Dbm      -> Decibels   (level difference)
//        Dbm  + Dbi      -> Dbm        (EIRP = power level + antenna gain)
//        Decibels +/- Decibels -> Decibels
//        Dbm  + Dbm      -> does not compile (adding two absolute levels is a category error)
//  * Constructors are explicit; user-defined literals provide readable call sites.

#include <cmath>
#include <compare>

namespace tl {

class Meters {
public:
    constexpr Meters() = default;
    constexpr explicit Meters(double metres) : v_(metres) {}
    [[nodiscard]] static constexpr Meters fromKilometers(double km) { return Meters(km * 1000.0); }
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr double kilometers() const { return v_ / 1000.0; }
    friend constexpr Meters operator+(Meters a, Meters b) { return Meters(a.v_ + b.v_); }
    friend constexpr Meters operator-(Meters a, Meters b) { return Meters(a.v_ - b.v_); }
    friend constexpr Meters operator*(Meters a, double s) { return Meters(a.v_ * s); }
    friend constexpr Meters operator*(double s, Meters a) { return Meters(a.v_ * s); }
    friend constexpr Meters operator/(Meters a, double s) { return Meters(a.v_ / s); }
    friend constexpr double operator/(Meters a, Meters b) { return a.v_ / b.v_; }
    friend constexpr Meters operator-(Meters a) { return Meters(-a.v_); }
    constexpr Meters& operator+=(Meters o) { v_ += o.v_; return *this; }
    constexpr Meters& operator-=(Meters o) { v_ -= o.v_; return *this; }
    friend constexpr auto operator<=>(Meters, Meters) = default;

private:
    double v_ = 0.0;
};

class Degrees; // forward

class Radians {
public:
    constexpr Radians() = default;
    constexpr explicit Radians(double rad) : v_(rad) {}
    [[nodiscard]] static constexpr Radians fromMilliradians(double mrad) { return Radians(mrad / 1000.0); }
    [[nodiscard]] static constexpr Radians fromDegrees(double deg) {
        return Radians(deg * 3.14159265358979323846 / 180.0);
    }
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr double milliradians() const { return v_ * 1000.0; }
    [[nodiscard]] constexpr double degrees() const { return v_ * 180.0 / 3.14159265358979323846; }
    friend constexpr Radians operator+(Radians a, Radians b) { return Radians(a.v_ + b.v_); }
    friend constexpr Radians operator-(Radians a, Radians b) { return Radians(a.v_ - b.v_); }
    friend constexpr Radians operator*(Radians a, double s) { return Radians(a.v_ * s); }
    friend constexpr Radians operator*(double s, Radians a) { return Radians(a.v_ * s); }
    friend constexpr Radians operator/(Radians a, double s) { return Radians(a.v_ / s); }
    friend constexpr double operator/(Radians a, Radians b) { return a.v_ / b.v_; }
    friend constexpr Radians operator-(Radians a) { return Radians(-a.v_); }
    constexpr Radians& operator+=(Radians o) { v_ += o.v_; return *this; }
    friend constexpr auto operator<=>(Radians, Radians) = default;

private:
    double v_ = 0.0;
};

// Geographic angle in degrees (latitude, longitude, azimuth). Distinct from Radians so
// a latitude cannot be fed where a radian-valued scatter angle is expected.
class Degrees {
public:
    constexpr Degrees() = default;
    constexpr explicit Degrees(double deg) : v_(deg) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr Radians toRadians() const { return Radians::fromDegrees(v_); }
    friend constexpr Degrees operator+(Degrees a, Degrees b) { return Degrees(a.v_ + b.v_); }
    friend constexpr Degrees operator-(Degrees a, Degrees b) { return Degrees(a.v_ - b.v_); }
    friend constexpr Degrees operator*(Degrees a, double s) { return Degrees(a.v_ * s); }
    friend constexpr Degrees operator-(Degrees a) { return Degrees(-a.v_); }
    friend constexpr auto operator<=>(Degrees, Degrees) = default;

private:
    double v_ = 0.0;
};

class Hertz {
public:
    constexpr Hertz() = default;
    constexpr explicit Hertz(double hz) : v_(hz) {}
    [[nodiscard]] static constexpr Hertz fromMegahertz(double mhz) { return Hertz(mhz * 1.0e6); }
    [[nodiscard]] static constexpr Hertz fromGigahertz(double ghz) { return Hertz(ghz * 1.0e9); }
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr double megahertz() const { return v_ / 1.0e6; }
    [[nodiscard]] constexpr double gigahertz() const { return v_ / 1.0e9; }
    friend constexpr Hertz operator+(Hertz a, Hertz b) { return Hertz(a.v_ + b.v_); }
    friend constexpr Hertz operator-(Hertz a, Hertz b) { return Hertz(a.v_ - b.v_); }
    friend constexpr Hertz operator*(Hertz a, double s) { return Hertz(a.v_ * s); }
    friend constexpr Hertz operator/(Hertz a, double s) { return Hertz(a.v_ / s); }
    friend constexpr double operator/(Hertz a, Hertz b) { return a.v_ / b.v_; }
    friend constexpr auto operator<=>(Hertz, Hertz) = default;

private:
    double v_ = 0.0;
};

class BitsPerSecond {
public:
    constexpr BitsPerSecond() = default;
    constexpr explicit BitsPerSecond(double bps) : v_(bps) {}
    [[nodiscard]] static constexpr BitsPerSecond fromMegabits(double mbps) {
        return BitsPerSecond(mbps * 1.0e6);
    }
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr double megabits() const { return v_ / 1.0e6; }
    friend constexpr BitsPerSecond operator*(BitsPerSecond a, double s) { return BitsPerSecond(a.v_ * s); }
    friend constexpr double operator/(BitsPerSecond a, BitsPerSecond b) { return a.v_ / b.v_; }
    friend constexpr auto operator<=>(BitsPerSecond, BitsPerSecond) = default;

private:
    double v_ = 0.0;
};

class Kelvin {
public:
    constexpr Kelvin() = default;
    constexpr explicit Kelvin(double k) : v_(k) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    friend constexpr auto operator<=>(Kelvin, Kelvin) = default;

private:
    double v_ = 0.0;
};

// Relative decibels: gains, losses, margins, level differences.
class Decibels {
public:
    constexpr Decibels() = default;
    constexpr explicit Decibels(double db) : v_(db) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    friend constexpr Decibels operator+(Decibels a, Decibels b) { return Decibels(a.v_ + b.v_); }
    friend constexpr Decibels operator-(Decibels a, Decibels b) { return Decibels(a.v_ - b.v_); }
    friend constexpr Decibels operator*(Decibels a, double s) { return Decibels(a.v_ * s); }
    friend constexpr Decibels operator*(double s, Decibels a) { return Decibels(a.v_ * s); }
    friend constexpr Decibels operator/(Decibels a, double s) { return Decibels(a.v_ / s); }
    friend constexpr Decibels operator-(Decibels a) { return Decibels(-a.v_); }
    constexpr Decibels& operator+=(Decibels o) { v_ += o.v_; return *this; }
    constexpr Decibels& operator-=(Decibels o) { v_ -= o.v_; return *this; }
    friend constexpr auto operator<=>(Decibels, Decibels) = default;

private:
    double v_ = 0.0;
};

// Antenna gain relative to isotropic. Not interchangeable with plain Decibels:
// converting is an explicit, visible step.
class Dbi {
public:
    constexpr Dbi() = default;
    constexpr explicit Dbi(double dbi) : v_(dbi) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr Decibels asDecibels() const { return Decibels(v_); }
    friend constexpr auto operator<=>(Dbi, Dbi) = default;

private:
    double v_ = 0.0;
};

// Absolute power level in dB relative to 1 mW.
class Dbm {
public:
    constexpr Dbm() = default;
    constexpr explicit Dbm(double dbm) : v_(dbm) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] double milliwatts() const { return std::pow(10.0, v_ / 10.0); }
    [[nodiscard]] double watts() const { return milliwatts() / 1000.0; }

    friend constexpr Dbm operator+(Dbm level, Decibels shift) { return Dbm(level.v_ + shift.value()); }
    friend constexpr Dbm operator-(Dbm level, Decibels shift) { return Dbm(level.v_ - shift.value()); }
    friend constexpr Dbm operator+(Dbm level, Dbi gain) { return Dbm(level.v_ + gain.value()); }
    friend constexpr Decibels operator-(Dbm a, Dbm b) { return Decibels(a.v_ - b.v_); }
    // Two absolute levels cannot be summed: category error, must not compile.
    friend Dbm operator+(Dbm, Dbm) = delete;
    friend constexpr auto operator<=>(Dbm, Dbm) = default;

private:
    double v_ = 0.0;
};

class Watts {
public:
    constexpr Watts() = default;
    constexpr explicit Watts(double w) : v_(w) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] Dbm dbm() const { return Dbm(10.0 * std::log10(v_ * 1000.0)); }
    friend constexpr Watts operator+(Watts a, Watts b) { return Watts(a.v_ + b.v_); }
    friend constexpr Watts operator*(Watts a, double s) { return Watts(a.v_ * s); }
    friend constexpr auto operator<=>(Watts, Watts) = default;

private:
    double v_ = 0.0;
};

class Percent {
public:
    constexpr Percent() = default;
    constexpr explicit Percent(double pct) : v_(pct) {}
    [[nodiscard]] constexpr double value() const { return v_; }
    [[nodiscard]] constexpr double fraction() const { return v_ / 100.0; }
    friend constexpr auto operator<=>(Percent, Percent) = default;

private:
    double v_ = 0.0;
};

// ---- literals ------------------------------------------------------------

namespace literals {
constexpr Meters operator""_m(long double v) { return Meters(static_cast<double>(v)); }
constexpr Meters operator""_m(unsigned long long v) { return Meters(static_cast<double>(v)); }
constexpr Meters operator""_km(long double v) { return Meters::fromKilometers(static_cast<double>(v)); }
constexpr Meters operator""_km(unsigned long long v) { return Meters::fromKilometers(static_cast<double>(v)); }
constexpr Degrees operator""_deg(long double v) { return Degrees(static_cast<double>(v)); }
constexpr Degrees operator""_deg(unsigned long long v) { return Degrees(static_cast<double>(v)); }
constexpr Radians operator""_mrad(long double v) { return Radians::fromMilliradians(static_cast<double>(v)); }
constexpr Hertz operator""_MHz(long double v) { return Hertz::fromMegahertz(static_cast<double>(v)); }
constexpr Hertz operator""_MHz(unsigned long long v) { return Hertz::fromMegahertz(static_cast<double>(v)); }
constexpr Hertz operator""_GHz(long double v) { return Hertz::fromGigahertz(static_cast<double>(v)); }
constexpr Hertz operator""_GHz(unsigned long long v) { return Hertz::fromGigahertz(static_cast<double>(v)); }
constexpr Decibels operator""_dB(long double v) { return Decibels(static_cast<double>(v)); }
constexpr Decibels operator""_dB(unsigned long long v) { return Decibels(static_cast<double>(v)); }
constexpr Dbm operator""_dBm(long double v) { return Dbm(static_cast<double>(v)); }
constexpr Dbm operator""_dBm(unsigned long long v) { return Dbm(static_cast<double>(v)); }
constexpr Dbi operator""_dBi(long double v) { return Dbi(static_cast<double>(v)); }
constexpr Dbi operator""_dBi(unsigned long long v) { return Dbi(static_cast<double>(v)); }
constexpr Watts operator""_W(long double v) { return Watts(static_cast<double>(v)); }
constexpr Watts operator""_W(unsigned long long v) { return Watts(static_cast<double>(v)); }
constexpr Percent operator""_pct(long double v) { return Percent(static_cast<double>(v)); }
} // namespace literals

} // namespace tl
