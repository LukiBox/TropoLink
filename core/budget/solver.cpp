#include "core/budget/solver.h"

#include <algorithm>
#include <cmath>

namespace tl::budget {

namespace {
// Aperture-to-medium coupling loss for a symmetric pair of antennas (P.617-5 eq. (3)).
double couplingLossDb(double perAntennaGainDbi) {
    return 0.07 * std::exp(0.055 * 2.0 * perAntennaGainDbi);
}
} // namespace

DesignSolver::DesignSolver(const AvailabilityEngine& engine, const RadioParams& radio,
                           Decibels medianPathLoss)
    : engine_(engine), radio_(radio), medianPathLoss_(medianPathLoss) {}

SolverResult DesignSolver::solve(const SolverRequest& request) const {
    SolverResult out;
    const LinkBudget current = computeLinkBudget(radio_, medianPathLoss_);
    out.currentMedianMargin = current.fadeMargin;
    out.requiredMedianMargin =
        engine_.marginForAvailability(request.targetAvailability, request.diversity, request.worstMonth);
    const double deltaDb = out.requiredMedianMargin.value() - current.fadeMargin.value();

    switch (request.solveFor) {
    case SolveFor::TxPower: {
        const Dbm required = radio_.txPower + Decibels(deltaDb);
        out.requiredTxPower = required;
        out.feasible = required.value() <= 70.0; // 10 kW practical ceiling flag
        out.note = out.feasible ? "" : "required power exceeds 10 kW";
        break;
    }
    case SolveFor::AntennaGain: {
        // Margin as a function of the per-antenna gain change dG (both ends changed
        // equally): margin(dG) = margin0 + 2 dG - [Lc(G0+dG) - Lc(G0)].
        // Note: the path-loss median is held fixed apart from the explicit coupling
        // term (consistent with showing Lc as its own line).
        const double g0 = radio_.antennaGainA.value();
        const double lc0 = couplingLossDb(g0);
        auto marginAt = [&](double dG) {
            return current.fadeMargin.value() + 2.0 * dG - (couplingLossDb(g0 + dG) - lc0);
        };
        double lo = -40.0;
        double hi = 40.0;
        // marginAt is increasing until coupling loss growth overtakes 2 dG; find the
        // peak first, then solve on the rising branch.
        double peakG = hi;
        double peakM = marginAt(hi);
        for (double g = lo; g <= hi; g += 0.25) {
            if (marginAt(g) > peakM) {
                peakM = marginAt(g);
                peakG = g;
            }
        }
        if (peakM < out.requiredMedianMargin.value()) {
            out.feasible = false;
            out.note = "target availability unreachable by antenna gain alone (coupling loss limit)";
            break;
        }
        hi = peakG;
        for (int i = 0; i < 100; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (marginAt(mid) < out.requiredMedianMargin.value()) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        out.requiredAntennaGain = Dbi(g0 + 0.5 * (lo + hi));
        out.feasible = true;
        break;
    }
    case SolveFor::DataRate: {
        // Bandwidth scales with rate; required SNR (from Eb/N0) is rate-invariant, so
        // margin falls 10 log10 per rate decade: B* = B0 10^(-delta/10).
        const double b0 = radio_.effectiveBandwidth().value();
        const double bStar = b0 * std::pow(10.0, -deltaDb / 10.0);
        const double rate = bStar * radio_.modulation.bitsPerSymbol / (1.0 + radio_.modulation.rolloff);
        out.maxDataRate = BitsPerSecond(std::max(0.0, rate));
        out.feasible = rate >= 1.0e3; // at least 1 kbit/s to count as a usable link
        out.note = out.feasible ? "" : "no usable data rate at this availability";
        break;
    }
    }
    return out;
}

} // namespace tl::budget
