#include "core/geo/atmosphere.h"

#include <gtest/gtest.h>

using namespace tl;
using namespace tl::geo;

TEST(Atmosphere, KFactorFromLapseRate) {
    // Standard refraction dN = 39.75 -> k = 4/3 by definition of the 157 relation.
    EXPECT_NEAR(Atmosphere::kFromLapseRate(39.25), 4.0 / 3.0, 0.01);
    // Sub-refractive (dN < 0) must give k < 1 and must not blow up.
    EXPECT_LT(Atmosphere::kFromLapseRate(-20.0), 1.0);
    EXPECT_GT(Atmosphere::kFromLapseRate(-20.0), 0.5);
    // Near-ducting gradients are clamped, not infinite.
    EXPECT_LE(Atmosphere::kFromLapseRate(156.9), 157.0);
}

TEST(Atmosphere, ExponentialAtmosphere) {
    Atmosphere atm;
    atm.seaLevelN0 = 315.0;
    EXPECT_NEAR(atm.surfaceRefractivityAt(Meters(0.0)), 315.0, 1e-9);
    EXPECT_NEAR(atm.surfaceRefractivityAt(Meters(7350.0)), 315.0 / 2.718281828, 0.5);
}

TEST(Atmosphere, SubRefractiveGeometryDoesNotCrash) {
    Atmosphere atm;
    atm.kFactor = 0.8; // sub-refractive case must work, not crash
    EXPECT_GT(atm.effectiveEarthRadius().value(), 0.0);
}

TEST(Atmosphere, RefractivityMapsLoadAndLookup) {
    const auto maps = RefractivityMaps::load(TROPOLINK_TEST_DATA_DIR);
    ASSERT_TRUE(maps.hasValue()) << (maps.hasValue() ? std::string() : maps.error().message);
    // Western Poland: N0 in a plausible continental band, dN positive (P.452 sign).
    const GeoPoint poland{Degrees(51.97), Degrees(15.27)};
    const double n0 = maps.value().n0At(poland);
    const double dn = maps.value().dnAt(poland);
    EXPECT_GT(n0, 290.0);
    EXPECT_LT(n0, 350.0);
    EXPECT_GT(dn, 20.0);
    EXPECT_LT(dn, 60.0);
    const auto atm = maps.value().atmosphereAt(poland);
    EXPECT_GT(atm.kFactor, 1.1);
    EXPECT_LT(atm.kFactor, 1.7);
    EXPECT_EQ(atm.climate, Climate::ContinentalTemperate);
}

TEST(Atmosphere, DeterministicLookup) {
    const auto maps = RefractivityMaps::load(TROPOLINK_TEST_DATA_DIR);
    ASSERT_TRUE(maps.hasValue());
    const GeoPoint p{Degrees(51.97), Degrees(15.27)};
    EXPECT_DOUBLE_EQ(maps.value().n0At(p), maps.value().n0At(p));
}
