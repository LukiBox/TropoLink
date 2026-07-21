#pragma once

// Common interface of the troposcatter loss models. Each implementation traces to a
// published method; outside its validity envelope a model reports the violation
// instead of extrapolating.

#include "core/common/units.h"
#include "core/tropo/statistics.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace tl::tropo {

enum class ModelId { Fspl, P617, Tn101, Itm };

// Fixed percentile set shown in the comparison table.
inline constexpr std::array<double, 5> kStandardPercentiles = {50.0, 90.0, 99.0, 99.9, 99.99};

struct Validity {
    bool valid = true;
    std::vector<std::string> issues; // human-readable "out of validity range" reasons
};

class LossModel {
  public:
    virtual ~LossModel() = default;

    [[nodiscard]] virtual ModelId id() const = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string citation() const = 0;
    [[nodiscard]] virtual Validity validity() const = 0;

    // Basic transmission loss not exceeded for p% of the average year, p in (0, 100).
    // Includes aperture-to-medium coupling loss (reported separately by couplingLoss()).
    [[nodiscard]] virtual Decibels lossNotExceededAnnual(double percent) const = 0;

    // Same threshold statistics for the average worst month, converted per
    // ITU-R P.841-5 (trans-horizon parameters with the model's Ns).
    [[nodiscard]] virtual Decibels lossNotExceededWorstMonth(double percent) const;

    [[nodiscard]] virtual Decibels medianLoss() const { return lossNotExceededAnnual(50.0); }
    [[nodiscard]] virtual Decibels couplingLoss() const { return Decibels(0.0); }

    // Surface refractivity used for the P.841 worst-month conversion.
    [[nodiscard]] virtual double surfaceRefractivityNs() const { return 301.0; }
};

} // namespace tl::tropo
