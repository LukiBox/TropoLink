#pragma once

// Availability statistics for troposcatter links.
//
// Model: the long-term (climate) distribution of the HOURLY-MEDIAN loss comes from
// the selected propagation model, L(p) (approximately log-normal, per P.617-5 §4).
// Within the hour the signal is Rayleigh distributed (P.617-5 Attachment 1 §2), so
// the received power is exponential around the hourly median. Diversity of order n
// is modelled as selection combining of independent branches (short-term only —
// the long-term component is fully correlated across branches):
//
//   outage = Int_0^1 [1 - exp(-ln2 * 10^((SNRreq - SNRmed(u))/10))]^n du
//
// evaluated on a fixed grid (deterministic). The margin <-> availability inversion
// is a monotone bisection, exact to the stated tolerance (round-trip tested).

#include "core/common/units.h"
#include "core/tropo/loss_model.h"

#include <memory>
#include <vector>

namespace tl::budget {

enum class DiversityMode { None, Space, Frequency, Angle, Quad };

[[nodiscard]] constexpr int diversityOrder(DiversityMode mode) {
    switch (mode) {
    case DiversityMode::None:
        return 1;
    case DiversityMode::Space:
    case DiversityMode::Frequency:
    case DiversityMode::Angle:
        return 2; // dual diversity; angle ~ vertical space (P.617-5 §7.3)
    case DiversityMode::Quad:
        return 4; // space + frequency
    }
    return 1;
}

[[nodiscard]] const char* diversityName(DiversityMode mode);

class AvailabilityEngine {
  public:
    // Captures the fade-depth distribution Delta L(u) = L(100u) - L(50) of the model,
    // annual and worst-month, on a fixed quantile grid.
    explicit AvailabilityEngine(const tropo::LossModel& model);

    // Availability (percent of time SNR >= required) for a given MEDIAN fade margin.
    [[nodiscard]] Percent availability(Decibels medianFadeMargin, DiversityMode diversity,
                                       bool worstMonth) const;

    // Inverse: median fade margin required for a target availability. True inverse of
    // availability() to better than 0.01 dB.
    [[nodiscard]] Decibels marginForAvailability(Percent target, DiversityMode diversity,
                                                 bool worstMonth) const;

    // Diversity improvement at a target availability: margin(None) - margin(mode).
    [[nodiscard]] Decibels diversityGain(Percent target, DiversityMode mode, bool worstMonth) const;

    // Availability-vs-margin curve for the report figure (margins in dB).
    struct CurvePoint {
        double marginDb;
        double availabilityPercent;
    };
    [[nodiscard]] std::vector<CurvePoint> curve(DiversityMode diversity, bool worstMonth,
                                                double minMarginDb = -10.0, double maxMarginDb = 50.0,
                                                int points = 121) const;

  private:
    [[nodiscard]] double outage(double marginDb, int order, bool worstMonth) const;

    std::vector<double> fadeDepthAnnualDb_; // Delta L at grid quantiles
    std::vector<double> fadeDepthWorstMonthDb_;
    static constexpr int kGrid = 1201;
};

// Space-diversity separation rule (P.617-5 eq. (44)-(45), f > 1 GHz):
//   horizontal: 0.634 sqrt(D^2 + Ih^2), Ih = 20 m;  vertical: Iv = 15 m.
struct DiversitySeparation {
    Meters horizontal{0.0};
    Meters vertical{0.0};
    double frequencySeparationMhz = 0.0; // P.617-5 eq. (46)
    Radians angleSeparation{0.0};        // P.617-5 eq. (47)
};

[[nodiscard]] DiversitySeparation diversitySeparation(Meters antennaDiameter, Hertz frequency,
                                                      Radians scatterAngle, Meters pathLength);

} // namespace tl::budget
