#include "core/budget/auto_design.h"
#include "core/budget/availability.h"
#include "core/budget/link_budget.h"
#include "core/budget/solver.h"
#include "core/tropo/p617.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace tl;
using namespace tl::budget;
using namespace tl::literals;

namespace {

RadioParams referenceRadio() {
    RadioParams r;
    r.txPower = 57.0_dBm;
    r.antennaGainA = 39.1_dBi;
    r.antennaGainB = 39.1_dBi;
    r.lineLossA = 0.5_dB;
    r.lineLossB = 0.5_dB;
    r.noiseFigure = 4.0_dB;
    r.modulation = Modulation{"QPSK", 2.0, Decibels(10.5), 0.35};
    r.dataRate = BitsPerSecond::fromMegabits(2.0);
    return r;
}

tropo::P617Model referenceModel() {
    tropo::P617Params p;
    p.frequency = 4.4_GHz;
    p.pathLength = 103.5_km;
    p.takeoffA = Radians(0.003);
    p.takeoffB = Radians(0.003);
    p.gainA = 39.1_dBi;
    p.gainB = 39.1_dBi;
    p.seaLevelN0 = 320.0;
    p.lapseRateDn = 40.0;
    p.kFactor = 4.0 / 3.0;
    p.surfaceHeightAmsl = 150.0_m;
    p.antennaAmslA = 154.0_m;
    p.antennaAmslB = 154.0_m;
    return tropo::P617Model(p);
}

} // namespace

TEST(AutoDesign, ApertureGainMatchesTextbookDish) {
    // 3 m dish at 4.4 GHz, 55% efficiency: G = 10 log10(0.55 (pi D f / c)^2).
    // lambda = 0.06813 m -> pi*3/0.06813 = 138.3 -> 0.55*138.3^2 = 10523 -> 40.2 dBi.
    const Dbi g = apertureGain(3.0_m, 4.4_GHz, 0.55);
    EXPECT_NEAR(g.value(), 40.2, 0.2);
    // Doubling the diameter is +6 dB; doubling the frequency is +6 dB.
    EXPECT_NEAR(apertureGain(6.0_m, 4.4_GHz, 0.55).value() - g.value(), 6.02, 0.05);
    EXPECT_NEAR(apertureGain(3.0_m, 8.8_GHz, 0.55).value() - g.value(), 6.02, 0.05);
}

TEST(AutoDesign, RespectsApertureToMediumCouplingOptimum) {
    // The whole point of searching rather than assuming "bigger dish is better":
    // coupling loss Lc = 0.07 exp[0.055 (Gt + Gr)] eventually eats the extra gain.
    // Past the optimum, adding gain must stop improving the net link.
    tropo::P617Params p;
    p.frequency = 4.4_GHz;
    p.pathLength = 103.5_km;
    p.takeoffA = Radians(0.003);
    p.takeoffB = Radians(0.003);
    p.seaLevelN0 = 320.0;
    p.lapseRateDn = 40.0;
    p.kFactor = 4.0 / 3.0;
    p.surfaceHeightAmsl = 150.0_m;
    p.antennaAmslA = 154.0_m;
    p.antennaAmslB = 154.0_m;

    auto netGainDb = [&](double gainDbi) {
        p.gainA = Dbi(gainDbi);
        p.gainB = Dbi(gainDbi);
        const tropo::P617Model model(p);
        // Net benefit of the antennas: both gains minus the loss they cause.
        return 2.0 * gainDbi - model.medianLoss().value();
    };

    // Rising while gain is cheap...
    EXPECT_GT(netGainDb(45.0), netGainDb(35.0));
    // ...and past the optimum (~50 dBi per end) more gain is counter-productive.
    EXPECT_LT(netGainDb(60.0), netGainDb(50.0));
}

TEST(AutoDesign, ProducesAFeasibleDesignAndExplainsIt) {
    AutoDesignGeometry g;
    g.pathLength = 103.5_km;
    g.takeoffA = Radians(0.003);
    g.takeoffB = Radians(0.003);
    g.surfaceHeightAmsl = 150.0_m;
    g.antennaAmslA = 154.0_m;
    g.antennaAmslB = 154.0_m;
    g.terrainAvailable = true;

    geo::Atmosphere atm;
    atm.seaLevelN0 = 320.0;
    atm.lapseRateDn = 40.0;
    atm.kFactor = 4.0 / 3.0;

    AutoDesignConstraints c;
    c.targetAvailability = Percent(99.9);
    c.diversity = DiversityMode::Quad;
    c.dataRate = BitsPerSecond::fromMegabits(2.0);

    const auto lib = ModulationLibrary::builtIn();
    const auto result = autoDesign(g, atm, lib, referenceRadio(), 4.4_GHz, 3.0_m, c);

    ASSERT_TRUE(result.feasible);
    EXPECT_EQ(result.noteKey, "auto_ok");
    // Stays inside equipment limits and the P.617 validity envelope.
    EXPECT_LE(result.txPower.value(), c.maxTxPower.value());
    EXPECT_LE(result.antennaDiameter.value(), c.maxAntennaDiameter.value());
    EXPECT_GE(result.frequency.gigahertz(), 0.2);
    EXPECT_LE(result.frequency.gigahertz(), 5.0);
    // It must actually reach the target it was asked for.
    EXPECT_GE(result.availabilityAnnual.value(), c.targetAvailability.value());
    // And it must be able to say what it changed.
    EXPECT_FALSE(result.changes.empty());
    for (const auto& ch : result.changes) {
        EXPECT_FALSE(ch.field.empty());
        EXPECT_FALSE(ch.reasonKey.empty());
        EXPECT_NE(ch.oldValue, ch.newValue);
    }
}

TEST(AutoDesign, HarderPathDemandsMoreThanAnEasyOne) {
    auto design = [](double pathKm, Percent target) {
        AutoDesignGeometry g;
        g.pathLength = Meters(pathKm * 1000.0);
        g.takeoffA = Radians(0.003);
        g.takeoffB = Radians(0.003);
        g.surfaceHeightAmsl = 150.0_m;
        g.antennaAmslA = 154.0_m;
        g.antennaAmslB = 154.0_m;
        g.terrainAvailable = true;
        geo::Atmosphere atm;
        atm.seaLevelN0 = 320.0;
        atm.lapseRateDn = 40.0;
        atm.kFactor = 4.0 / 3.0;
        AutoDesignConstraints c;
        c.targetAvailability = target;
        c.diversity = DiversityMode::Quad;
        c.dataRate = BitsPerSecond::fromMegabits(2.0);
        return autoDesign(g, atm, ModulationLibrary::builtIn(), referenceRadio(), 4.4_GHz, 3.0_m, c);
    };
    // A longer path, or a stricter availability target, cannot be cheaper to equip.
    const auto easy = design(150.0, Percent(99.0));
    const auto longer = design(400.0, Percent(99.0));
    const auto stricter = design(150.0, Percent(99.99));
    const auto burden = [](const AutoDesignResult& r) {
        return r.txPower.value() + 20.0 * std::log10(r.antennaDiameter.value());
    };
    EXPECT_GE(burden(longer), burden(easy));
    EXPECT_GE(burden(stricter), burden(easy));
}

TEST(Budget, WaterfallSumsExactly) {
    const auto radio = referenceRadio();
    const auto budget = computeLinkBudget(radio, Decibels(210.0));
    // EIRP = 57 - 0.5 + 39.1 = 95.6 dBm (§12 quotes 96.1 with zero line loss).
    EXPECT_NEAR(budget.eirp.value(), 95.6, 1e-12);
    EXPECT_NEAR(budget.medianRsl.value(), 95.6 - 210.0 + 39.1 - 0.5, 1e-12);
    // Noise floor: -174 + 10log10(B) + NF, B = 2e6/2*1.35 = 1.35e6 -> 61.3 dB.
    const double expectedNoise = -174.0 + 10.0 * std::log10(radio.effectiveBandwidth().value()) + 4.0;
    EXPECT_NEAR(budget.noiseFloor.value(), expectedNoise, 1e-9);
    EXPECT_NEAR(budget.medianSnr.value(), budget.medianRsl.value() - budget.noiseFloor.value(), 1e-12);
    EXPECT_NEAR(budget.fadeMargin.value(), budget.medianSnr.value() - budget.requiredSnr.value(), 1e-12);

    // The waterfall steps sum exactly from TX power to RSL.
    double running = 0.0;
    bool started = false;
    for (const auto& item : budget.waterfall) {
        if (item.label == "tx_power") {
            running = item.valueDb;
            started = true;
        } else if (started && !item.isLevel) {
            running += item.valueDb;
        }
        if (item.label == "rsl") {
            EXPECT_NEAR(running, item.valueDb, 1e-9);
            break;
        }
        if (started && item.isLevel) {
            EXPECT_NEAR(running, item.valueDb, 1e-9);
        }
    }
}

TEST(Budget, RequiredSnrFromModulation) {
    const auto radio = referenceRadio();
    // QPSK, rolloff 0.35: SNR_req = 10.5 + 10log10(2/1.35) = 12.2 dB.
    EXPECT_NEAR(radio.requiredSnr().value(), 10.5 + 10.0 * std::log10(2.0 / 1.35), 1e-9);
}

TEST(Budget, AvailabilityMonotoneInMarginAndDiversity) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    const auto a10 = engine.availability(10.0_dB, DiversityMode::None, false);
    const auto a20 = engine.availability(20.0_dB, DiversityMode::None, false);
    EXPECT_GT(a20.value(), a10.value());
    const auto dual = engine.availability(10.0_dB, DiversityMode::Space, false);
    const auto quad = engine.availability(10.0_dB, DiversityMode::Quad, false);
    EXPECT_GT(dual.value(), a10.value());
    EXPECT_GT(quad.value(), dual.value());
    // Worst month is harder than the average year.
    const auto wm = engine.availability(10.0_dB, DiversityMode::None, true);
    EXPECT_LE(wm.value(), a10.value() + 1e-9);
}

TEST(Budget, MarginAvailabilityTrueInverse) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    for (const double target : {99.0, 99.9, 99.99}) {
        for (const auto div : {DiversityMode::None, DiversityMode::Space, DiversityMode::Quad}) {
            const auto margin = engine.marginForAvailability(Percent(target), div, false);
            const auto achieved = engine.availability(margin, div, false);
            EXPECT_NEAR(achieved.value(), target, 0.005) << "target " << target;
        }
    }
}

TEST(Budget, DiversityGainMatchesReferenceBehaviour) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    // Quad diversity gain at 99.9% must exceed dual, both positive, and dual gain in
    // the classic 5-20 dB band for Rayleigh selection at deep percentiles.
    const auto dual = engine.diversityGain(Percent(99.9), DiversityMode::Space, false);
    const auto quad = engine.diversityGain(Percent(99.9), DiversityMode::Quad, false);
    EXPECT_GT(dual.value(), 2.0);
    EXPECT_LT(dual.value(), 25.0);
    EXPECT_GT(quad.value(), dual.value());
}

TEST(Budget, SolverPowerAndRoundTrip) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    const auto radio = referenceRadio();
    const auto budget = computeLinkBudget(radio, model.medianLoss());
    const DesignSolver solver(engine, radio, model.medianLoss());

    SolverRequest req;
    req.solveFor = SolveFor::TxPower;
    req.targetAvailability = Percent(99.9);
    req.diversity = DiversityMode::Quad;
    const auto result = solver.solve(req);
    ASSERT_TRUE(result.requiredTxPower.has_value());
    // Applying the solved power must yield exactly the required margin.
    const double newMargin =
        budget.fadeMargin.value() + (result.requiredTxPower->value() - radio.txPower.value());
    EXPECT_NEAR(newMargin, result.requiredMedianMargin.value(), 1e-6);
}

TEST(Budget, SolverGainAccountsForCouplingLoss) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    const auto radio = referenceRadio();
    const DesignSolver solver(engine, radio, model.medianLoss());
    SolverRequest req;
    req.solveFor = SolveFor::AntennaGain;
    req.targetAvailability = Percent(99.99);
    req.diversity = DiversityMode::Quad;
    const auto result = solver.solve(req);
    if (result.feasible) {
        ASSERT_TRUE(result.requiredAntennaGain.has_value());
        // Verify the solution satisfies the margin equation including coupling loss.
        const double g0 = radio.antennaGainA.value();
        const double dG = result.requiredAntennaGain->value() - g0;
        const auto budget = computeLinkBudget(radio, model.medianLoss());
        const auto lc = [](double g) { return 0.07 * std::exp(0.055 * 2.0 * g); };
        const double margin = budget.fadeMargin.value() + 2.0 * dG - (lc(g0 + dG) - lc(g0));
        EXPECT_NEAR(margin, result.requiredMedianMargin.value(), 1e-4);
    }
}

TEST(Budget, SolverRateClosedForm) {
    const auto model = referenceModel();
    const AvailabilityEngine engine(model);
    const auto radio = referenceRadio();
    const DesignSolver solver(engine, radio, model.medianLoss());
    SolverRequest req;
    req.solveFor = SolveFor::DataRate;
    req.targetAvailability = Percent(99.9);
    req.diversity = DiversityMode::Quad;
    const auto result = solver.solve(req);
    ASSERT_TRUE(result.maxDataRate.has_value());
    if (result.feasible) {
        // Recompute the budget at the solved rate: margin == required margin.
        RadioParams solved = radio;
        solved.dataRate = *result.maxDataRate;
        const auto budget = computeLinkBudget(solved, model.medianLoss());
        EXPECT_NEAR(budget.fadeMargin.value(), result.requiredMedianMargin.value(), 1e-6);
    }
}

TEST(Budget, DiversitySeparationCalculator) {
    const auto sep = diversitySeparation(3.0_m, 4.4_GHz, Radians(0.0182), 103.5_km);
    // 0.634 sqrt(9 + 400) = 12.8 m horizontal; 0.634 sqrt(9 + 225) = 9.7 m vertical.
    EXPECT_NEAR(sep.horizontal.value(), 12.83, 0.05);
    EXPECT_NEAR(sep.vertical.value(), 9.70, 0.05);
    EXPECT_GT(sep.frequencySeparationMhz, 10.0);
    EXPECT_LT(sep.frequencySeparationMhz, 300.0);
    EXPECT_GT(sep.angleSeparation.value(), 0.0);
}
