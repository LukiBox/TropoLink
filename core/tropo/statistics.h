#pragma once

// Percentage-of-time statistics shared by the propagation models.
//
//  * Inverse complementary cumulative normal (the "Qi" function): rational
//    approximation from Abramowitz & Stegun 26.2.23 (the same approximation the ITS
//    ITM algorithm uses) — deterministic, no library dependence.
//  * Annual <-> worst-month conversion: ITU-R P.841-5. For trans-horizon paths the
//    Table 1 parameters are beta = 0.13, Q1 = 5.8 - 0.03 exp(Ns / 75), with Ns the
//    surface refractivity in the common volume; Q clamped to [1, 12].

#include "core/common/units.h"

namespace tl::tropo {

// Standard normal deviate z with complementary probability q (0 < q < 1):
// P(Z > z) = q. Abramowitz & Stegun 26.2.23, |error| < 4.5e-4.
[[nodiscard]] double inverseComplementaryNormal(double q);

struct WorstMonthParams {
    double beta = 0.13;
    double q1 = 2.85; // global planning default; use forTransHorizon() for tropo paths

    [[nodiscard]] static WorstMonthParams global() { return {0.13, 2.85}; }
    [[nodiscard]] static WorstMonthParams forTransHorizon(double surfaceRefractivityNs);
};

// Q conversion factor as a function of the ANNUAL percentage of excess p (%),
// ITU-R P.841-5 eq. (2): pw = Q(p) * p, Q in [1, 12].
[[nodiscard]] double worstMonthQ(double annualPercentExcess, const WorstMonthParams& params);

// pw (%) from p (%): worst-month percentage of excess for the same threshold.
[[nodiscard]] double annualToWorstMonthExcess(double annualPercentExcess, const WorstMonthParams& params);

// Inverse: annual percentage of excess p (%) from worst-month pw (%), P.841-5 eq. (3)-(4)
// (closed form in the power-law region, monotone bisection elsewhere).
[[nodiscard]] double worstMonthToAnnualExcess(double worstMonthPercentExcess, const WorstMonthParams& params);

} // namespace tl::tropo
