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
        const double margin = budget.fadeMargin.value() + 2.0 * dG -
                              (lc(g0 + dG) - lc(g0));
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
