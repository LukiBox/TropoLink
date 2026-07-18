#include "core/budget/availability.h"

#include <algorithm>
#include <cmath>

namespace tl::budget {

const char* diversityName(DiversityMode mode) {
    switch (mode) {
    case DiversityMode::None:
        return "none";
    case DiversityMode::Space:
        return "space";
    case DiversityMode::Frequency:
        return "frequency";
    case DiversityMode::Angle:
        return "angle";
    case DiversityMode::Quad:
        return "quad";
    }
    return "none";
}

AvailabilityEngine::AvailabilityEngine(const tropo::LossModel& model) {
    fadeDepthAnnualDb_.resize(kGrid);
    fadeDepthWorstMonthDb_.resize(kGrid);
    const double median = model.lossNotExceededAnnual(50.0).value();
    const double medianWm = model.lossNotExceededWorstMonth(50.0).value();
    for (int i = 0; i < kGrid; ++i) {
        // Quantile grid, clamped away from 0/100 (the models clamp internally too).
        const double u = (static_cast<double>(i) + 0.5) / kGrid;
        const double p = std::clamp(u * 100.0, 0.05, 99.95);
        fadeDepthAnnualDb_[static_cast<std::size_t>(i)] = model.lossNotExceededAnnual(p).value() - median;
        fadeDepthWorstMonthDb_[static_cast<std::size_t>(i)] =
            model.lossNotExceededWorstMonth(p).value() - medianWm;
    }
}

double AvailabilityEngine::outage(double marginDb, int order, bool worstMonth) const {
    const auto& fade = worstMonth ? fadeDepthWorstMonthDb_ : fadeDepthAnnualDb_;
    constexpr double ln2 = 0.6931471805599453;
    double sum = 0.0;
    for (int i = 0; i < kGrid; ++i) {
        // Hourly-median SNR margin at this time quantile.
        const double m = marginDb - fade[static_cast<std::size_t>(i)];
        // Rayleigh (exponential power) short-term outage around that median.
        const double r = std::pow(10.0, -m / 10.0);
        const double q1 = 1.0 - std::exp(-ln2 * r);
        double q = q1;
        for (int b = 1; b < order; ++b) {
            q *= q1;
        }
        sum += q;
    }
    return sum / kGrid;
}

Percent AvailabilityEngine::availability(Decibels medianFadeMargin, DiversityMode diversity,
                                         bool worstMonth) const {
    const double out = outage(medianFadeMargin.value(), diversityOrder(diversity), worstMonth);
    return Percent(100.0 * (1.0 - out));
}

Decibels AvailabilityEngine::marginForAvailability(Percent target, DiversityMode diversity,
                                                   bool worstMonth) const {
    const double targetOutage = std::clamp(1.0 - target.fraction(), 1e-9, 1.0);
    const int order = diversityOrder(diversity);
    double lo = -60.0;
    double hi = 120.0;
    for (int i = 0; i < 100; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (outage(mid, order, worstMonth) > targetOutage) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return Decibels(0.5 * (lo + hi));
}

Decibels AvailabilityEngine::diversityGain(Percent target, DiversityMode mode, bool worstMonth) const {
    return Decibels(marginForAvailability(target, DiversityMode::None, worstMonth).value() -
                    marginForAvailability(target, mode, worstMonth).value());
}

std::vector<AvailabilityEngine::CurvePoint> AvailabilityEngine::curve(DiversityMode diversity,
                                                                      bool worstMonth, double minMarginDb,
                                                                      double maxMarginDb, int points) const {
    std::vector<CurvePoint> curve;
    curve.reserve(static_cast<std::size_t>(points));
    for (int i = 0; i < points; ++i) {
        const double m = minMarginDb + (maxMarginDb - minMarginDb) * i / (points - 1);
        curve.push_back({m, availability(Decibels(m), diversity, worstMonth).value()});
    }
    return curve;
}

DiversitySeparation diversitySeparation(Meters antennaDiameter, Hertz frequency, Radians scatterAngle,
                                        Meters pathLength) {
    DiversitySeparation out;
    const double d = antennaDiameter.value();
    constexpr double ih = 20.0; // m, P.617-5 §7.1
    constexpr double iv = 15.0;
    out.horizontal = Meters(0.634 * std::sqrt(d * d + ih * ih));
    out.vertical = Meters(0.634 * std::sqrt(d * d + iv * iv));
    // P.617-5 eq. (46); evaluated with theta in mrad, d in km, f in MHz and the
    // aperture term in the denominator (dimensional consistency note in
    // docs/model_references.md).
    const double fMhz = frequency.megahertz();
    const double thetaMrad = scatterAngle.milliradians();
    const double dKm = pathLength.kilometers();
    if (dKm > 0.0) {
        out.frequencySeparationMhz = 1.44 * fMhz * thetaMrad / (dKm * std::sqrt(d * d + iv * iv));
    }
    // P.617-5 eq. (47): angle spacing equivalent to the vertical spacing.
    out.angleSeparation = Radians(std::atan(out.vertical.value() / (500.0 * dKm)));
    return out;
}

} // namespace tl::budget
