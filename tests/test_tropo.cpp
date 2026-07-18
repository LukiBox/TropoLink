#include "core/tropo/fspl.h"
#include "core/tropo/model_suite.h"
#include "core/tropo/p617.h"
#include "core/tropo/scatter_geometry.h"
#include "core/tropo/statistics.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace tl;
using namespace tl::tropo;
using namespace tl::literals;

namespace {

terrain::Profile flatProfile(double distanceM, double elevationM, int n = 1201) {
    terrain::Profile p;
    p.totalDistance = Meters(distanceM);
    p.step = Meters(distanceM / (n - 1));
    p.hasCoverage = true;
    for (int i = 0; i < n; ++i) {
        terrain::ProfilePoint pt;
        pt.distance = Meters(distanceM * i / (n - 1));
        pt.elevation = Meters(elevationM);
        p.points.push_back(pt);
    }
    return p;
}

} // namespace

// §9: FSPL at 4.4 GHz over 103.5 km ~ 145.6 dB.
TEST(Tropo, FsplReferencePinned) {
    const auto loss = freeSpacePathLoss(4.4_GHz, 103.5_km);
    EXPECT_NEAR(loss.value(), 145.6, 0.1);
    // Worked example: 32.45 + 20log10(4400) + 20log10(103.5) = 145.63 dB
    EXPECT_NEAR(loss.value(), 32.45 + 20.0 * std::log10(4400.0) + 20.0 * std::log10(103.5), 1e-9);
}

TEST(Tropo, InverseNormalSanity) {
    EXPECT_NEAR(inverseComplementaryNormal(0.5), 0.0, 1e-6);
    EXPECT_NEAR(inverseComplementaryNormal(0.10), 1.2816, 5e-4);
    EXPECT_NEAR(inverseComplementaryNormal(0.01), 2.3263, 5e-4);
    EXPECT_NEAR(inverseComplementaryNormal(0.99), -2.3263, 5e-4);
}

// P.841-5 eq. (5): with global parameters, p = 0.30 pw^1.15 in the power-law region.
TEST(Tropo, WorstMonthGlobalRelation) {
    const auto params = WorstMonthParams::global();
    for (const double pw : {0.01, 0.1, 1.0}) {
        const double p = worstMonthToAnnualExcess(pw, params);
        EXPECT_NEAR(p, 0.30 * std::pow(pw, 1.15), 0.02 * std::max(p, 1e-4));
    }
}

TEST(Tropo, WorstMonthRoundTrip) {
    const auto params = WorstMonthParams::forTransHorizon(310.0);
    for (const double p : {0.001, 0.01, 0.1, 1.0, 5.0, 20.0}) {
        const double pw = annualToWorstMonthExcess(p, params);
        const double back = worstMonthToAnnualExcess(pw, params);
        EXPECT_NEAR(back, p, 0.01 * p + 1e-6);
    }
    // Q must stay within P.841's stated [1, 12].
    for (const double p : {1e-5, 0.01, 1.0, 10.0, 50.0, 99.0}) {
        const double q = worstMonthQ(p, params);
        EXPECT_GE(q, 1.0);
        EXPECT_LE(q, 12.0);
    }
}

TEST(Tropo, ScatterGeometrySymmetricPath) {
    ScatterGeometryInput in;
    in.pathLength = 100.0_km;
    in.takeoffA = Radians(0.005);
    in.takeoffB = Radians(0.005);
    in.antennaAmslA = 100.0_m;
    in.antennaAmslB = 100.0_m;
    in.kFactor = 4.0 / 3.0;
    in.gainA = 39.1_dBi;
    in.gainB = 39.1_dBi;
    const auto g = computeScatterGeometry(in);
    // theta = 0.005 + 0.005 + d/(ka) = 0.01 + 100e3/8.4933e6 = 0.021774 rad
    EXPECT_NEAR(g.scatterAngle.value(), 0.01 + 100.0e3 / ((4.0 / 3.0) * 6.370e6), 1e-6);
    // Symmetric: volume mid-path.
    EXPECT_NEAR(g.distanceToVolumeA.value(), 50.0e3, 10.0);
    EXPECT_NEAR(g.distanceToVolumeB.value(), 50.0e3, 10.0);
    EXPECT_GT(g.volumeBaseAmsl.value(), in.antennaAmslA.value());
    EXPECT_GT(g.volumeTopAmsl.value(), g.volumeBaseAmsl.value());
    // Beamwidth from 39.1 dBi: sqrt(27000/8128) ~ 1.82 degrees.
    EXPECT_NEAR(g.halfPowerBeamwidthA.degrees(), 1.82, 0.05);
}

// P.617-5 worked example, self-computed from the published equations:
// f = 4400 MHz, d = 103.5 km, theta_t = theta_r = 3 mrad, k = 4/3, N0 = 320, dN = 40,
// Gt = Gr = 39.1 dBi, hs = 0.15 km:
//   theta_e = 103.5e3/(4/3 * 6370) = 12.187 mrad, theta = 18.187 mrad
//   Lc = 0.07 exp(0.055 * 78.2) = 5.181 dB
//   F  = 0.18*320*exp(-0.15/7.35) - 0.23*40 = 56.44 - 9.2 = 47.24 dB (approx)
//   L(50) = 47.24 + 22log10(4400) + 35log10(18.187) + 17log10(103.5) + 5.18
TEST(Tropo, P617MedianMatchesHandComputation) {
    P617Params p;
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
    const P617Model model(p);
    ASSERT_TRUE(model.validity().valid);

    const double thetaE = 103.5 * 1000.0 / ((4.0 / 3.0) * 6370.0);
    const double theta = 6.0 + thetaE;
    EXPECT_NEAR(model.thetaMrad(), theta, 1e-6);
    const double lc = 0.07 * std::exp(0.055 * 78.2);
    EXPECT_NEAR(model.couplingLoss().value(), lc, 1e-6);
    const double fTerm = 0.18 * 320.0 * std::exp(-0.15 / 7.35) - 0.23 * 40.0;
    EXPECT_NEAR(model.fTerm(), fTerm, 1e-6);
    const double expected = fTerm + 22.0 * std::log10(4400.0) + 35.0 * std::log10(theta) +
                            17.0 * std::log10(103.5) + lc;
    EXPECT_NEAR(model.medianLoss().value(), expected, 1e-6);
    // The distribution behaves: higher availability -> more loss.
    EXPECT_GT(model.lossNotExceededAnnual(99.9).value(), model.medianLoss().value());
    EXPECT_LT(model.lossNotExceededAnnual(20.0).value(), model.medianLoss().value());
    // Worst month is never easier than the average year at high percentiles.
    EXPECT_GE(model.lossNotExceededWorstMonth(99.9).value(),
              model.lossNotExceededAnnual(99.9).value() - 1e-9);
}

TEST(Tropo, P617ValidityEnvelopeEnforced) {
    P617Params p;
    p.frequency = Hertz::fromMegahertz(100.0); // below the envelope
    p.pathLength = 103.5_km;
    p.takeoffA = Radians(0.003);
    p.takeoffB = Radians(0.003);
    const P617Model model(p);
    EXPECT_FALSE(model.validity().valid);
    ASSERT_FALSE(model.validity().issues.empty());

    P617Params p2 = p;
    p2.frequency = 4.4_GHz;
    p2.pathLength = 50.0_km; // below distance envelope
    const P617Model model2(p2);
    EXPECT_FALSE(model2.validity().valid);
}

TEST(Tropo, SuiteOnReferenceGeometry) {
    // Reference §9 geometry over synthetic flat terrain at 120 m: all three scatter
    // models valid, medians tens of dB above FSPL and within a plausible band of one
    // another; spread computed as max - min.
    SuiteInput in;
    in.siteA = geo::GeoPoint{Degrees(51.50609699), Degrees(15.33150851)};
    in.siteB = geo::GeoPoint{Degrees(52.43470597), Degrees(15.21931198)};
    in.antennaAglA = 4.0_m;
    in.antennaAglB = 4.0_m;
    in.frequency = 4.4_GHz;
    in.gainA = 39.1_dBi;
    in.gainB = 39.1_dBi;
    in.atmosphere.seaLevelN0 = 320.0;
    in.atmosphere.lapseRateDn = 40.0;
    in.atmosphere.kFactor = 4.0 / 3.0;
    in.atmosphere.climate = geo::Climate::ContinentalTemperate;

    const auto inverse = geo::Geodesy::inverse(in.siteA, in.siteB);
    const auto profile = flatProfile(inverse.distance.value(), 120.0);
    const auto result = runSuite(in, profile);

    EXPECT_NEAR(result.fspl.value(), 145.6, 0.2);
    EXPECT_NEAR(result.inverse.distance.kilometers(), 103.5, 0.2);

    int validScatterModels = 0;
    for (const auto& row : result.rows) {
        if (row.id == ModelId::Fspl) {
            continue;
        }
        if (row.valid) {
            ++validScatterModels;
            // Troposcatter at 103 km / 4.4 GHz: far above FSPL, below 280 dB.
            EXPECT_GT(row.median.value(), result.fspl.value() + 30.0) << row.name;
            EXPECT_LT(row.median.value(), 280.0) << row.name;
        }
    }
    EXPECT_GE(validScatterModels, 2);
    EXPECT_GE(result.spread.value(), 0.0);
    EXPECT_LT(result.spread.value(), 40.0);

    // Coupling loss visible: 0.07 exp(0.055 * 78.2) ~ 5.2 dB at 39.1 dBi both ends.
    const auto& p617row = result.rows[1];
    EXPECT_NEAR(p617row.couplingLoss.value(), 5.18, 0.05);

    // Spread = max - min of the valid medians, recomputed here.
    double lo = 1e18;
    double hi = -1e18;
    for (const auto& row : result.rows) {
        if (row.id != ModelId::Fspl && row.valid) {
            lo = std::min(lo, row.median.value());
            hi = std::max(hi, row.median.value());
        }
    }
    EXPECT_NEAR(result.spread.value(), hi - lo, 1e-9);
}

TEST(Tropo, DeterministicSuite) {
    SuiteInput in;
    in.siteA = geo::GeoPoint{Degrees(51.50609699), Degrees(15.33150851)};
    in.siteB = geo::GeoPoint{Degrees(52.43470597), Degrees(15.21931198)};
    const auto inverse = geo::Geodesy::inverse(in.siteA, in.siteB);
    const auto profile = flatProfile(inverse.distance.value(), 120.0);
    const auto a = runSuite(in, profile);
    const auto b = runSuite(in, profile);
    ASSERT_EQ(a.rows.size(), b.rows.size());
    for (std::size_t i = 0; i < a.rows.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.rows[i].median.value(), b.rows[i].median.value());
        for (std::size_t k = 0; k < a.rows[i].annualDb.size(); ++k) {
            EXPECT_DOUBLE_EQ(a.rows[i].annualDb[k], b.rows[i].annualDb[k]);
        }
    }
    EXPECT_DOUBLE_EQ(a.spread.value(), b.spread.value());
}
