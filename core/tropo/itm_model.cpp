#include "core/tropo/itm_model.h"

#include <Enums.h>
#include <Errors.h>
#include <itm.h>

#include <algorithm>
#include <cmath>

namespace tl::tropo {

ItmModel::ItmModel(const terrain::Profile& profile, const ItmParams& params) : params_(params) {
    const std::size_t n = profile.points.size();
    if (n < 2) {
        validity_.valid = false;
        validity_.issues.push_back("no terrain profile for ITM");
        return;
    }
    pfl_.resize(n + 2);
    pfl_[0] = static_cast<double>(n - 1);
    pfl_[1] = profile.totalDistance.value() / static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        pfl_[i + 2] = profile.points[i].elevation.value();
    }

    const double fMhz = params.frequency.megahertz();
    if (fMhz < 20.0 || fMhz > 20000.0) {
        validity_.issues.push_back("frequency outside ITM envelope (20 MHz - 20 GHz)");
    }
    const double dKm = profile.totalDistance.kilometers();
    if (dKm < 1.0 || dKm > 2000.0) {
        validity_.issues.push_back("distance outside ITM envelope (1-2000 km)");
    }
    const double hA = params.antennaHeightAglA.value();
    const double hB = params.antennaHeightAglB.value();
    if (hA < 0.5 || hA > 3000.0 || hB < 0.5 || hB > 3000.0) {
        validity_.issues.push_back("antenna height outside ITM envelope (0.5-3000 m)");
    }

    couplingLoss_ = Decibels(0.07 * std::exp(0.055 * (params.gainA.value() + params.gainB.value())));

    // Probe run at the median to obtain the propagation mode, Ns and warnings.
    if (validity_.issues.empty()) {
        double a = 0.0;
        long warnings = 0;
        IntermediateValues iv{};
        const int rc = ITM_P2P_TLS_Ex(hA, hB, pfl_.data(), params.climate, params.seaLevelN0, fMhz,
                                      params.polarization, params.groundEpsilon, params.groundSigma, 13,
                                      50.0, 50.0, 50.0, &a, &warnings, &iv);
        if (rc != SUCCESS && rc != SUCCESS_WITH_WARNINGS) {
            validity_.issues.push_back("ITM rejected the inputs (code " + std::to_string(rc) + ")");
        } else {
            ns_ = iv.N_s;
            const int mode = iv.mode;
            if (mode == MODE__LINE_OF_SIGHT) {
                mode_ = "line-of-sight";
                validity_.issues.push_back("ITM classifies this path as line-of-sight, not troposcatter");
            } else if (mode == MODE__DIFFRACTION) {
                mode_ = "diffraction";
                validity_.issues.push_back(
                    "ITM classifies this path as diffraction-dominated; scatter cross-check only");
            } else if (mode == MODE__TROPOSCATTER) {
                mode_ = "troposcatter";
            }
        }
    }
    validity_.valid = validity_.issues.empty() ||
                      (validity_.issues.size() == 1 && mode_ == "diffraction");
}

Decibels ItmModel::lossNotExceededAnnual(double percent) const {
    const double p = std::clamp(percent, 0.101, 99.899); // ITM validates 0.1 < q < 99.9
    double a = 0.0;
    long warnings = 0;
    IntermediateValues iv{};
    const double fMhz = params_.frequency.megahertz();
    // mdvar 13: location/situation variability off — engineered point-to-point link.
    ITM_P2P_TLS_Ex(std::max(0.5, params_.antennaHeightAglA.value()),
                   std::max(0.5, params_.antennaHeightAglB.value()),
                   const_cast<double*>(pfl_.data()), params_.climate, params_.seaLevelN0, fMhz,
                   params_.polarization, params_.groundEpsilon, params_.groundSigma, 13, p, 50.0, 50.0, &a,
                   &warnings, &iv);
    return Decibels(a) + couplingLoss_;
}

} // namespace tl::tropo
