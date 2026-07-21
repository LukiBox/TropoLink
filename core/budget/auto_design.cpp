#include "core/budget/auto_design.h"

#include "core/tropo/p617.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace tl::budget {

namespace {

// Candidate frequencies: the bands troposcatter equipment is actually built for,
// all inside the P.617-5 validity envelope (0.2-5 GHz).
constexpr std::array<double, 8> kFrequencyGhz = {0.9, 1.35, 2.0, 2.4, 3.0, 3.6, 4.4, 4.8};

// Candidate reflector diameters: standard product sizes, metres.
constexpr std::array<double, 7> kDiameterM = {1.2, 1.8, 2.4, 3.0, 3.7, 4.6, 6.0};

constexpr double kSpeedOfLight = 299792458.0;
constexpr double kPi = 3.14159265358979323846;

std::string format(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return buf;
}

} // namespace

Dbi apertureGain(Meters diameter, Hertz frequency, double efficiency) {
    const double lambda = kSpeedOfLight / frequency.value();
    const double g = efficiency * std::pow(kPi * diameter.value() / lambda, 2.0);
    return Dbi(10.0 * std::log10(std::max(g, 1e-9)));
}

AutoDesignResult autoDesign(const AutoDesignGeometry& geometry, const geo::Atmosphere& atmosphere,
                            const ModulationLibrary& modulations, const RadioParams& current,
                            Hertz currentFrequency, Meters currentDiameter,
                            const AutoDesignConstraints& constraints) {
    AutoDesignResult best;
    best.feasible = false;
    best.noteKey = "auto_no_model";
    if (modulations.entries().empty() || geometry.pathLength.value() <= 0.0) {
        return best;
    }

    struct Candidate {
        double freqGhz = 0.0;
        double diameterM = 0.0;
        Dbi gain{0.0};
        Dbm requiredTx{0.0};
        int modulationIndex = 0;
        Decibels medianLoss{0.0};
        Decibels couplingLoss{0.0};
        Decibels requiredMargin{0.0};
        std::shared_ptr<const tropo::LossModel> model;
    };

    std::vector<Candidate> feasibleCandidates;
    Candidate bestOverall;
    bool haveAny = false;

    for (const double fGhz : kFrequencyGhz) {
        const Hertz frequency = Hertz::fromGigahertz(fGhz);
        for (const double dM : kDiameterM) {
            if (dM > constraints.maxAntennaDiameter.value() + 1e-9) {
                continue;
            }
            const Dbi gain = apertureGain(Meters(dM), frequency, constraints.apertureEfficiency);

            tropo::P617Params p;
            p.frequency = frequency;
            p.pathLength = geometry.pathLength;
            p.takeoffA = geometry.takeoffA;
            p.takeoffB = geometry.takeoffB;
            p.gainA = gain;
            p.gainB = gain;
            p.seaLevelN0 = atmosphere.seaLevelN0;
            p.lapseRateDn = atmosphere.lapseRateDn;
            p.kFactor = atmosphere.kFactor;
            p.surfaceHeightAmsl = geometry.surfaceHeightAmsl;
            p.antennaAmslA = geometry.antennaAmslA;
            p.antennaAmslB = geometry.antennaAmslB;
            p.terrainAvailable = geometry.terrainAvailable;

            auto model = std::make_shared<tropo::P617Model>(p);
            if (!model->validity().valid) {
                continue; // outside the Recommendation's envelope: never extrapolate
            }

            const AvailabilityEngine engine(*model);
            const Decibels requiredMargin = engine.marginForAvailability(
                constraints.targetAvailability, constraints.diversity, constraints.worstMonth);
            const Decibels medianLoss = model->medianLoss();

            // Pick the modulation that needs the least transmit power at the required
            // data rate: lower order needs less SNR but occupies more bandwidth, so
            // the noise floor moves too. Evaluate rather than assume.
            for (std::size_t m = 0; m < modulations.entries().size(); ++m) {
                const Modulation& mod = modulations.entries()[m];
                const Hertz bandwidth = mod.bandwidthFor(constraints.dataRate);
                const Dbm noise = noiseFloorDbm(bandwidth, constraints.noiseFigure);
                // Invert the budget for the transmit power this configuration needs.
                const double txDbm = requiredMargin.value() + constraints.designHeadroom.value() +
                                     constraints.lineLossA.value() + constraints.lineLossB.value() -
                                     gain.value() - gain.value() + medianLoss.value() +
                                     noise.value() + mod.requiredSnr().value();

                Candidate c;
                c.freqGhz = fGhz;
                c.diameterM = dM;
                c.gain = gain;
                c.requiredTx = Dbm(txDbm);
                c.modulationIndex = static_cast<int>(m);
                c.medianLoss = medianLoss;
                c.couplingLoss = model->couplingLoss();
                c.requiredMargin = requiredMargin;
                c.model = model;

                if (!haveAny || txDbm < bestOverall.requiredTx.value()) {
                    bestOverall = c;
                    haveAny = true;
                }
                if (txDbm <= constraints.maxTxPower.value()) {
                    feasibleCandidates.push_back(c);
                }
            }
        }
    }

    if (!haveAny) {
        best.noteKey = "auto_no_model";
        return best;
    }

    Candidate chosen = bestOverall;
    if (!feasibleCandidates.empty()) {
        // Among configurations that close the link, prefer the cheapest to field:
        // within 1 dB of the lowest required power, take the smallest dish, then the
        // lowest frequency. A 1 dB power saving never justifies a bigger reflector.
        const double bestTx =
            std::min_element(feasibleCandidates.begin(), feasibleCandidates.end(),
                             [](const Candidate& a, const Candidate& b) {
                                 return a.requiredTx.value() < b.requiredTx.value();
                             })
                ->requiredTx.value();
        std::vector<Candidate> shortlist;
        for (const auto& c : feasibleCandidates) {
            if (c.requiredTx.value() <= bestTx + 1.0) {
                shortlist.push_back(c);
            }
        }
        std::sort(shortlist.begin(), shortlist.end(), [](const Candidate& a, const Candidate& b) {
            if (std::abs(a.diameterM - b.diameterM) > 1e-9) {
                return a.diameterM < b.diameterM;
            }
            if (std::abs(a.freqGhz - b.freqGhz) > 1e-9) {
                return a.freqGhz < b.freqGhz;
            }
            return a.requiredTx.value() < b.requiredTx.value();
        });
        chosen = shortlist.front();
    }

    best.feasible = !feasibleCandidates.empty();
    best.noteKey = best.feasible ? "auto_ok" : "auto_infeasible";
    best.frequency = Hertz::fromGigahertz(chosen.freqGhz);
    best.antennaDiameter = Meters(chosen.diameterM);
    best.antennaGain = chosen.gain;
    best.modulationIndex = chosen.modulationIndex;
    best.medianLoss = chosen.medianLoss;
    best.couplingLoss = chosen.couplingLoss;

    // Round the transmit power up to a whole dBm: nobody specifies 46.37 dBm.
    const double txDbm = std::ceil(chosen.requiredTx.value());
    best.txPower = Dbm(std::min(txDbm, constraints.maxTxPower.value()));

    // What the delivered configuration actually achieves.
    {
        const Modulation& mod = modulations.entries()[static_cast<std::size_t>(chosen.modulationIndex)];
        const Hertz bandwidth = mod.bandwidthFor(constraints.dataRate);
        const Dbm noise = noiseFloorDbm(bandwidth, constraints.noiseFigure);
        const double margin = best.txPower.value() - constraints.lineLossA.value() -
                              constraints.lineLossB.value() + chosen.gain.value() +
                              chosen.gain.value() - chosen.medianLoss.value() - noise.value() -
                              mod.requiredSnr().value();
        best.fadeMargin = Decibels(margin);
        if (chosen.model) {
            const AvailabilityEngine engine(*chosen.model);
            best.availabilityAnnual =
                engine.availability(best.fadeMargin, constraints.diversity, false);
            best.availabilityWorstMonth =
                engine.availability(best.fadeMargin, constraints.diversity, true);
        }
    }

    // Evidence for the coupling-loss optimum: what the next larger dish would cost.
    for (const auto& c : feasibleCandidates) {
        if (c.freqGhz == chosen.freqGhz && c.modulationIndex == chosen.modulationIndex &&
            c.diameterM > chosen.diameterM) {
            const double penalty = c.requiredTx.value() - chosen.requiredTx.value();
            if (penalty > 0.0 &&
                (best.rejectedLargerDiameter.value() == 0.0 ||
                 c.diameterM < best.rejectedLargerDiameter.value())) {
                best.rejectedLargerDiameter = Meters(c.diameterM);
                best.rejectedLargerPenaltyDb = Decibels(penalty);
            }
        }
    }

    // --- what changed and why -------------------------------------------------
    auto addChange = [&best](const char* field, const std::string& oldValue,
                             const std::string& newValue, const char* reasonKey) {
        if (oldValue == newValue) {
            return;
        }
        best.changes.push_back(AutoDesignChange{field, oldValue, newValue, reasonKey});
    };

    addChange("frequency", format(currentFrequency.gigahertz(), 4) + " GHz",
              format(best.frequency.gigahertz(), 4) + " GHz", "reason_frequency");
    addChange("diameter", format(currentDiameter.value(), 1) + " m",
              format(best.antennaDiameter.value(), 1) + " m", "reason_diameter");
    addChange("gain", format(current.antennaGainA.value(), 1) + " dBi",
              format(best.antennaGain.value(), 1) + " dBi", "reason_gain");
    addChange("txPower", format(current.txPower.value(), 1) + " dBm",
              format(best.txPower.value(), 1) + " dBm", "reason_txpower");
    addChange("modulation", current.modulation.name,
              modulations.entries()[static_cast<std::size_t>(best.modulationIndex)].name,
              "reason_modulation");

    return best;
}

} // namespace tl::budget
