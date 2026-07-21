#pragma once

// ITM / Longley-Rice (NTIA Irregular Terrain Model) point-to-point run over the real
// terrain profile — the independent cross-check. Uses the official NTIA C++
// implementation vendored in third_party/ntia_itm (public domain), which performs its
// own horizon finding, terrain roughness and climate variability.

#include "core/terrain/profile.h"
#include "core/tropo/loss_model.h"

#include <vector>

namespace tl::tropo {

struct ItmParams {
    Hertz frequency{};
    Meters antennaHeightAglA{};
    Meters antennaHeightAglB{};
    Dbi gainA{};
    Dbi gainB{};
    double seaLevelN0 = 315.0;
    int climate = 5;             // ITM climate code
    int polarization = 1;        // 0 horizontal, 1 vertical
    double groundEpsilon = 15.0; // average ground
    double groundSigma = 0.005;
};

class ItmModel final : public LossModel {
  public:
    ItmModel(const terrain::Profile& profile, const ItmParams& params);

    [[nodiscard]] ModelId id() const override { return ModelId::Itm; }
    [[nodiscard]] std::string name() const override { return "ITM (Longley-Rice)"; }
    [[nodiscard]] std::string citation() const override {
        return "NTIA ITM v1.x (Longley-Rice), P2P mode; Hufford's Algorithm (1982)";
    }
    [[nodiscard]] Validity validity() const override { return validity_; }
    [[nodiscard]] Decibels lossNotExceededAnnual(double percent) const override;
    [[nodiscard]] Decibels couplingLoss() const override { return couplingLoss_; }
    [[nodiscard]] double surfaceRefractivityNs() const override { return ns_; }

    // Propagation mode ITM selected at the median (LOS / diffraction / troposcatter).
    [[nodiscard]] const std::string& propagationMode() const { return mode_; }

  private:
    ItmParams params_;
    std::vector<double> pfl_; // [n-1, step_m, elevations...]
    Validity validity_;
    Decibels couplingLoss_{0.0};
    std::string mode_ = "unknown";
    double ns_ = 301.0;
};

} // namespace tl::tropo
