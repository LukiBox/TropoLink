#pragma once

// NBS Technical Note 101 (Rice, Longley, Norton, Barsis, 1967), Chapter 9:
// long-term median troposcatter transmission loss, with the Chapter 10 long-term
// variability. The classic method — the historical workhorse of military
// troposcatter design and the sanity anchor of every design review.
//
// Implementation notes (all cited in docs/model_references.md):
//  * The attenuation function F(theta d), the frequency-gain function H0 (TN101
//    eqs. 9.4-9.5, Fig. 9.1-9.5) and the scattering-efficiency factor eta_s
//    (eq. 9.3a) are evaluated through their published computerized forms in the ITS
//    "Algorithm" (Hufford, 1982; the same fits vendored in third_party/ntia_itm:
//    TroposcatterLoss.cpp / H0Function.cpp, Algorithm §4.63, 6.9, 6.13).
//  * The Ns dependence of F is the Algorithm's -0.1 (Ns - 301) exp(-theta d / 40 km).
//  * Long-term variability V(50, de) and Y(q, de) per TN101 Ch. 10 / TN101v2 III.69-70,
//    via the Algorithm's climate-parameterized curves (Variability.cpp), with
//    location/situation variability disabled (point-to-point engineered link).
//  * Atmospheric absorption after TN101 §3: gamma ~ 0.0067 + 5e-5 f_GHz^2 dB/km.
//  * Aperture-to-medium coupling loss shown separately (same P.617-5 eq. (3) formula,
//    consistent across models so the comparison isolates the scatter prediction).

#include "core/terrain/profile.h"
#include "core/tropo/loss_model.h"

namespace tl::tropo {

struct Tn101Params {
    Hertz frequency{};
    Meters pathLength{};
    Radians takeoffA{};
    Radians takeoffB{};
    Meters horizonDistanceA{};
    Meters horizonDistanceB{};
    Meters antennaHeightAglA{};
    Meters antennaHeightAglB{};
    Dbi gainA{};
    Dbi gainB{};
    double surfaceRefractivityNs = 301.0;
    double kFactor = 4.0 / 3.0;
    int climate = 5; // ITM climate code (5 = continental temperate)
    Meters terrainIrregularityDeltaH{90.0};
    bool terrainAvailable = true;
};

// Interdecile terrain irregularity (delta h) of a profile after detrending — the
// TN101/ITM terrain roughness parameter.
[[nodiscard]] Meters interdecileDeltaH(const terrain::Profile& profile);

class Tn101Model final : public LossModel {
  public:
    explicit Tn101Model(const Tn101Params& params);

    [[nodiscard]] ModelId id() const override { return ModelId::Tn101; }
    [[nodiscard]] std::string name() const override { return "NBS TN101"; }
    [[nodiscard]] std::string citation() const override {
        return "NBS Tech. Note 101 (1967), Ch. 9-10, via ITS Algorithm (1982) §4.63, 5, 6.9, 6.13";
    }
    [[nodiscard]] Validity validity() const override { return validity_; }
    [[nodiscard]] Decibels lossNotExceededAnnual(double percent) const override;
    [[nodiscard]] Decibels couplingLoss() const override { return couplingLoss_; }
    [[nodiscard]] double surfaceRefractivityNs() const override { return params_.surfaceRefractivityNs; }

    [[nodiscard]] Decibels atmosphericAbsorption() const { return absorption_; }
    [[nodiscard]] Meters crossoverHeight() const { return Meters(crossoverHeightM_); }

  private:
    Tn101Params params_;
    Validity validity_;
    Decibels couplingLoss_{0.0};
    Decibels absorption_{0.0};
    double referenceAttenuation_ = 0.0; // A_ref: scatter attenuation + absorption + Lc
    double crossoverHeightM_ = 0.0;
    double heMeters_[2] = {4.0, 4.0};
};

} // namespace tl::tropo
