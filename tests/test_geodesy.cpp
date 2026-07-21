#include "core/geo/geodesy.h"

#include <gtest/gtest.h>

using namespace tl;
using namespace tl::geo;

namespace {
const GeoPoint kSiteA{Degrees(51.50609699), Degrees(15.33150851)};
const GeoPoint kSiteB{Degrees(52.43470597), Degrees(15.21931198)};
} // namespace

// Reference scenario (§9): distance ~103.5 km, azimuths near-north / near-south.
TEST(Geodesy, ReferenceScenarioDistancePinned) {
    const auto r = Geodesy::inverse(kSiteA, kSiteB);
    EXPECT_NEAR(r.distance.kilometers(), 103.5, 0.2);
    EXPECT_NEAR(r.forwardAzimuth.value(), 355.5, 1.0);
    EXPECT_NEAR(r.reverseAzimuth.value(), 175.4, 1.0);
}

// GeographicLib reference case: Karney's canonical test pair must agree to
// sub-millimetre with published values.
TEST(Geodesy, KarneyReferenceCase) {
    // JFK -> LHR style canonical example from GeographicLib documentation:
    // (40.6, -73.8) to (51.6, -0.5), s12 = 5551759.4003 m.
    const auto r =
        Geodesy::inverse(GeoPoint{Degrees(40.6), Degrees(-73.8)}, GeoPoint{Degrees(51.6), Degrees(-0.5)});
    EXPECT_NEAR(r.distance.value(), 5551759.4003, 0.001);
}

TEST(Geodesy, DirectInverseRoundTrip) {
    const auto inv = Geodesy::inverse(kSiteA, kSiteB);
    const auto reached = Geodesy::direct(kSiteA, inv.forwardAzimuth, inv.distance);
    EXPECT_NEAR(reached.latitude.value(), kSiteB.latitude.value(), 1e-9);
    EXPECT_NEAR(reached.longitude.value(), kSiteB.longitude.value(), 1e-9);
}

TEST(Geodesy, SampleLineEndpointsAndMonotonicity) {
    const auto samples = Geodesy::sampleLine(kSiteA, kSiteB, 101);
    ASSERT_EQ(samples.size(), 101U);
    EXPECT_NEAR(samples.front().point.latitude.value(), kSiteA.latitude.value(), 1e-9);
    EXPECT_NEAR(samples.back().point.latitude.value(), kSiteB.latitude.value(), 1e-9);
    for (std::size_t i = 1; i < samples.size(); ++i) {
        EXPECT_GT(samples[i].distanceFromStart.value(), samples[i - 1].distanceFromStart.value());
    }
    const auto inv = Geodesy::inverse(kSiteA, kSiteB);
    EXPECT_NEAR(samples.back().distanceFromStart.value(), inv.distance.value(), 1e-6);
}
