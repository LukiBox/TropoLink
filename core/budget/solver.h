#pragma once

// Design solver — the "full project" mode. Lock a target availability and solve for
// the missing variable: required antenna gain, required TX power, or maximum data
// rate. Inverts the calculator into a design tool.

#include "core/budget/availability.h"
#include "core/budget/link_budget.h"

#include <optional>

namespace tl::budget {

enum class SolveFor { TxPower, AntennaGain, DataRate };

struct SolverRequest {
    SolveFor solveFor = SolveFor::TxPower;
    Percent targetAvailability{99.9};
    bool worstMonth = false;
    DiversityMode diversity = DiversityMode::Quad;
};

struct SolverResult {
    bool feasible = false;
    std::string note;
    // Populated depending on solveFor:
    std::optional<Dbm> requiredTxPower;     // and its Watts equivalent for display
    std::optional<Dbi> requiredAntennaGain; // per antenna, both ends equal
    std::optional<BitsPerSecond> maxDataRate;
    Decibels requiredMedianMargin{0.0}; // margin the target availability demands
    Decibels currentMedianMargin{0.0};
};

class DesignSolver {
  public:
    DesignSolver(const AvailabilityEngine& engine, const RadioParams& radio, Decibels medianPathLoss);

    [[nodiscard]] SolverResult solve(const SolverRequest& request) const;

  private:
    const AvailabilityEngine& engine_;
    RadioParams radio_;
    Decibels medianPathLoss_;
};

} // namespace tl::budget
