#include "core/tropo/statistics.h"

#include <algorithm>
#include <cmath>

namespace tl::tropo {

double inverseComplementaryNormal(double q) {
    q = std::clamp(q, 1e-12, 1.0 - 1e-12);
    const bool upper = q < 0.5;
    const double p = upper ? q : 1.0 - q;
    // A&S 26.2.23.
    const double t = std::sqrt(-2.0 * std::log(p));
    const double c0 = 2.515516698;
    const double c1 = 0.802853;
    const double c2 = 0.010328;
    const double d1 = 1.432788;
    const double d2 = 0.189269;
    const double d3 = 0.001308;
    const double z = t - (c0 + c1 * t + c2 * t * t) / (1.0 + d1 * t + d2 * t * t + d3 * t * t * t);
    return upper ? z : -z;
}

WorstMonthParams WorstMonthParams::forTransHorizon(double surfaceRefractivityNs) {
    // ITU-R P.841-5, Table 1, "Trans-horizon land/sea".
    return {0.13, 5.8 - 0.03 * std::exp(surfaceRefractivityNs / 75.0)};
}

double worstMonthQ(double p, const WorstMonthParams& params) {
    const double q1 = std::max(params.q1, 1.0);
    const double beta = params.beta;
    // Region boundaries per P.841-5 eq. (2). Q(p) is continuous and non-increasing:
    //   Q = 12                                for p <= p0 = (Q1/12)^(1/beta)
    //   Q = Q1 p^-beta                        for p0 < p <= 3
    //   log-linear from Q(3) down to Q(30)=1  for 3 < p <= 30
    //   Q = 1                                 for p > 30
    if (p <= 0.0) {
        return 12.0;
    }
    const double p0 = std::pow(q1 / 12.0, 1.0 / beta);
    if (p <= p0) {
        return 12.0;
    }
    if (p <= 3.0) {
        return std::clamp(q1 * std::pow(p, -beta), 1.0, 12.0);
    }
    if (p <= 30.0) {
        const double q3 = std::clamp(q1 * std::pow(3.0, -beta), 1.0, 12.0);
        const double f = std::log10(p / 3.0); // log10(30/3) == 1
        return std::clamp(q3 + (1.0 - q3) * f, 1.0, 12.0);
    }
    return 1.0;
}

double annualToWorstMonthExcess(double p, const WorstMonthParams& params) {
    const double pw = worstMonthQ(p, params) * p;
    return std::min(pw, 100.0);
}

double worstMonthToAnnualExcess(double pw, const WorstMonthParams& params) {
    if (pw <= 0.0) {
        return 0.0;
    }
    const double q1 = std::max(params.q1, 1.0);
    const double beta = params.beta;
    // Power-law region closed form, P.841-5 eq. (4):  p = (pw / Q1)^(1/(1-beta)).
    const double p0 = std::pow(q1 / 12.0, 1.0 / beta);
    if (pw <= 12.0 * p0) {
        return pw / 12.0;
    }
    const double pAt3 = annualToWorstMonthExcess(3.0, params);
    if (pw <= pAt3) {
        return std::pow(pw / q1, 1.0 / (1.0 - beta));
    }
    // Interpolated region: pw(p) is strictly increasing; bisect.
    double lo = 3.0;
    double hi = 100.0;
    for (int i = 0; i < 80; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (annualToWorstMonthExcess(mid, params) < pw) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

} // namespace tl::tropo
