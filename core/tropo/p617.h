#pragma once

// ITU-R P.617-5 (08/2019), Annex 1 §4: tropospheric-scatter transmission loss.
// The primary model. Implemented step-by-step from the Recommendation:
//   eq. (1)-(2)  scatter angle  theta = theta_e + theta_t + theta_r  [mrad],
//                theta_e = d 10^3 / (k a),  a = 6370 km
//   eq. (3)      aperture-to-medium coupling loss Lc = 0.07 exp[0.055 (Gt + Gr)]
//   eq. (4)      L_bs(p) = F + 22 log f + 35 log theta + 17 log d + Lc - Y(p)
//   eq. (5)      F = 0.18 N0 exp(-h_s / h_b) - 0.23 dN
//   eq. (6)      Y(p) = +-0.035 N0 exp(-h0 / h_b) (-log10(p'/50))^0.67
//   eq. (7a)     h0: height of the common scattering volume
// N0 and dN come from the integral digital maps (§2) or operator override.

#include "core/tropo/loss_model.h"

namespace tl::tropo {

struct P617Params {
    Hertz frequency{};
    Meters pathLength{};
    Radians takeoffA{};        // theta_t
    Radians takeoffB{};        // theta_r
    Dbi gainA{};
    Dbi gainB{};
    double seaLevelN0 = 315.0; // N0 from the digital map at the common volume
    double lapseRateDn = 40.0; // dN (positive per P.452 convention)
    double kFactor = 4.0 / 3.0;
    Meters surfaceHeightAmsl{};  // h_s: mean height of the surface under the path
    Meters antennaAmslA{};       // h_t
    Meters antennaAmslB{};       // h_r
    bool terrainAvailable = true;
};

class P617Model final : public LossModel {
public:
    explicit P617Model(const P617Params& params);

    [[nodiscard]] ModelId id() const override { return ModelId::P617; }
    [[nodiscard]] std::string name() const override { return "ITU-R P.617-5"; }
    [[nodiscard]] std::string citation() const override {
        return "Rec. ITU-R P.617-5 (08/2019), Annex 1 §4.1-4.2";
    }
    [[nodiscard]] Validity validity() const override { return validity_; }
    [[nodiscard]] Decibels lossNotExceededAnnual(double percent) const override;
    [[nodiscard]] Decibels couplingLoss() const override { return couplingLoss_; }
    [[nodiscard]] double surfaceRefractivityNs() const override { return ns_; }

    // Exposed for provenance display and tests.
    [[nodiscard]] double thetaMrad() const { return thetaMrad_; }
    [[nodiscard]] Meters commonVolumeHeight() const { return Meters(h0Km_ * 1000.0); }
    [[nodiscard]] double fTerm() const { return f_; }

private:
    P617Params params_;
    Validity validity_;
    Decibels couplingLoss_{0.0};
    double thetaMrad_ = 0.0;
    double h0Km_ = 0.0;
    double f_ = 0.0;       // F term of eq. (5)
    double median_ = 0.0;  // L_bs(50)
    double ns_ = 301.0;    // surface refractivity at h_s (for P.841)
};

} // namespace tl::tropo
