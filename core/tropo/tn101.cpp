#include "core/tropo/tn101.h"

#include "core/geo/atmosphere.h"
#include "core/tropo/fspl.h"

#include <itm.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tl::tropo {

Meters interdecileDeltaH(const terrain::Profile& profile) {
    const auto& pts = profile.points;
    if (pts.size() < 10) {
        return Meters(0.0);
    }
    // Detrend with a least-squares line, then take the 10%-90% residual range.
    const double n = static_cast<double>(pts.size());
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    for (const auto& p : pts) {
        const double x = p.distance.value();
        const double y = p.elevation.value();
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }
    const double denom = n * sxx - sx * sx;
    const double slope = denom != 0.0 ? (n * sxy - sx * sy) / denom : 0.0;
    const double intercept = (sy - slope * sx) / n;
    std::vector<double> residuals;
    residuals.reserve(pts.size());
    for (const auto& p : pts) {
        residuals.push_back(p.elevation.value() - (intercept + slope * p.distance.value()));
    }
    std::sort(residuals.begin(), residuals.end());
    const auto idx = [&residuals](double f) {
        return residuals[static_cast<std::size_t>(f * static_cast<double>(residuals.size() - 1))];
    };
    return Meters(std::max(0.0, idx(0.9) - idx(0.1)));
}

Tn101Model::Tn101Model(const Tn101Params& params) : params_(params) {
    const double dM = params.pathLength.value();
    const double fMhz = params.frequency.megahertz();
    const double aE = params.kFactor * geo::Atmosphere::kEarthRadiusKm * 1000.0;
    const double theta = params.takeoffA.value() + params.takeoffB.value() + dM / aE;

    if (fMhz < 100.0 || fMhz > 10000.0) {
        validity_.issues.push_back("frequency " + std::to_string(fMhz) +
                                   " MHz outside TN101 envelope (100-10000 MHz)");
    }
    if (dM < 50.0e3 || dM > 1000.0e3) {
        validity_.issues.push_back("path length outside TN101 troposcatter envelope (50-1000 km)");
    }
    if (theta <= 0.0) {
        validity_.issues.push_back("scatter angle is not positive (path is not trans-horizon)");
    }
    if (!params.terrainAvailable) {
        validity_.issues.push_back("no terrain data: horizon geometry is smooth-earth estimate");
    }

    // Effective heights: antenna height above ground (engineered fixed sites).
    heMeters_[0] = std::max(0.5, params.antennaHeightAglA.value());
    heMeters_[1] = std::max(0.5, params.antennaHeightAglB.value());

    const double thetaHzn[2] = {params.takeoffA.value(), params.takeoffB.value()};
    const double dHzn[2] = {params.horizonDistanceA.value(), params.horizonDistanceB.value()};

    // TN101 Ch. 9 scatter attenuation below free space, via the Algorithm's
    // computerized F(theta d) + 10 log(k H theta^4) - Ns adjustment + H0.
    // theta_los = -(theta_t + theta_r) makes the internal angular distance equal theta.
    double h0Cache = -1.0; // <0: no cached H0, compute fresh (see TroposcatterLoss)
    const double thetaLos = -(thetaHzn[0] + thetaHzn[1]);
    const double aScat = TroposcatterLoss(dM, thetaHzn, dHzn, heMeters_, aE, params.surfaceRefractivityNs,
                                          fMhz, thetaLos, &h0Cache);
    // Crossover height for provenance display [Algorithm 4.66; TN101v1 9.3b].
    const double ad = std::abs(dHzn[0] - dHzn[1]);
    crossoverHeightM_ = (dM - ad) * (dM + ad) * theta * 0.25 / std::max(dM, 1.0);

    if (aScat >= 1000.0) {
        validity_.issues.push_back("TN101 A_scat undefined here (r1, r2 < 0.2; TN101 §9.2)");
    }
    validity_.valid = validity_.issues.empty();

    couplingLoss_ = Decibels(0.07 * std::exp(0.055 * (params.gainA.value() + params.gainB.value())));
    // Absorption estimate after TN101 §3 (oxygen + water vapour, surface values).
    const double fGhz = params.frequency.gigahertz();
    const double gammaDbPerKm = 0.0067 + 5.0e-5 * fGhz * fGhz;
    absorption_ = Decibels(gammaDbPerKm * params.pathLength.kilometers());

    referenceAttenuation_ = aScat + absorption_.value() + couplingLoss_.value();
}

Decibels Tn101Model::lossNotExceededAnnual(double percent) const {
    const double p = std::clamp(percent, 0.001, 99.999);
    const double fMhz = params_.frequency.megahertz();
    const double dM = params_.pathLength.value();
    // Location and situation variability disabled (mdvar 13 = 10 + broadcast):
    // an engineered point-to-point link keeps only the time distribution
    // [TN101 Ch. 10; Algorithm §5].
    long warnings = 0;
    const double a = Variability(p, 50.0, 50.0, heMeters_, params_.terrainIrregularityDeltaH.value(), fMhz,
                                 dM, referenceAttenuation_, params_.climate, 13, &warnings);
    return freeSpacePathLoss(params_.frequency, params_.pathLength) + Decibels(a);
}

} // namespace tl::tropo
