#include "core/tropo/p617.h"

#include "core/geo/atmosphere.h"

#include <algorithm>
#include <cmath>

namespace tl::tropo {

namespace {
constexpr double kScaleHeightKm = 7.35; // h_b, P.617-5 §4.1 Step 4
} // namespace

P617Model::P617Model(const P617Params& params) : params_(params) {
    const double dKm = params.pathLength.kilometers();
    const double fMhz = params.frequency.megahertz();
    const double kaKm = params.kFactor * geo::Atmosphere::kEarthRadiusKm;

    // eq. (1)-(2): angles in mrad.
    const double thetaEMrad = dKm * 1000.0 / kaKm;
    thetaMrad_ = params.takeoffA.milliradians() + params.takeoffB.milliradians() + thetaEMrad;

    // Validity envelope. P.617's scatter method addresses trans-horizon paths roughly
    // 100-1000 km at 0.2-5 GHz with a positive scatter angle.
    if (dKm < 100.0 || dKm > 1000.0) {
        validity_.issues.push_back("path length " + std::to_string(dKm) +
                                   " km outside P.617 envelope (100-1000 km)");
    }
    if (fMhz < 200.0 || fMhz > 5000.0) {
        validity_.issues.push_back("frequency " + std::to_string(fMhz) +
                                   " MHz outside P.617 envelope (200-5000 MHz)");
    }
    if (thetaMrad_ <= 0.0) {
        validity_.issues.push_back("scatter angle is not positive (path is not trans-horizon)");
    }
    if (!params.terrainAvailable) {
        validity_.issues.push_back("no terrain data: horizon angles are smooth-earth estimates");
    }
    validity_.valid = validity_.issues.empty() || (validity_.issues.size() == 1 && !params.terrainAvailable);

    // eq. (3): aperture-to-medium coupling loss.
    couplingLoss_ = Decibels(0.07 * std::exp(0.055 * (params.gainA.value() + params.gainB.value())));

    // eq. (5): F term. h_s in km.
    const double hsKm = params.surfaceHeightAmsl.kilometers();
    f_ = 0.18 * params.seaLevelN0 * std::exp(-hsKm / kScaleHeightKm) - 0.23 * params.lapseRateDn;

    // eq. (7a): common-volume height h0 (km AMSL), evaluated in the equivalent
    // flat-earth ray-crossover form, which reduces to (7a) at small angles and stays
    // well-defined for negative (smooth-earth) horizon angles:
    //   x* = (h_r - h_t + theta_r d + d^2/(2ka)) / theta,   h0 = h_t + theta_t x* + x*^2/(2ka)
    const double theta = thetaMrad_ / 1000.0;
    const double hT = params.antennaAmslA.kilometers();
    const double hR = params.antennaAmslB.kilometers();
    const double tauT = params.takeoffA.value();
    const double tauR = params.takeoffB.value();
    if (theta > 1e-6) {
        const double xStar = std::clamp((hR - hT + tauR * dKm + dKm * dKm / (2.0 * kaKm)) / theta, 0.0, dKm);
        h0Km_ = hT + tauT * xStar + xStar * xStar / (2.0 * kaKm);
    } else {
        h0Km_ = hT;
    }
    h0Km_ = std::max(h0Km_, 0.0);

    ns_ = params.seaLevelN0 * std::exp(-hsKm / kScaleHeightKm);

    // eq. (4) at p = 50 (Y = 0).
    if (thetaMrad_ > 0.0) {
        median_ = f_ + 22.0 * std::log10(fMhz) + 35.0 * std::log10(thetaMrad_) + 17.0 * std::log10(dKm) +
                  couplingLoss_.value();
    } else {
        median_ = 0.0;
    }
}

Decibels P617Model::lossNotExceededAnnual(double percent) const {
    const double p = std::clamp(percent, 0.001, 99.999);
    // eq. (6): conversion factor Y(p). For p >= 50 Y is negative, increasing the loss.
    const double scale = 0.035 * params_.seaLevelN0 * std::exp(-h0Km_ / kScaleHeightKm);
    double y = 0.0;
    if (p < 50.0) {
        y = scale * std::pow(-std::log10(p / 50.0), 0.67);
    } else if (p > 50.0) {
        y = -scale * std::pow(-std::log10((100.0 - p) / 50.0), 0.67);
    }
    return Decibels(median_ - y);
}

} // namespace tl::tropo
