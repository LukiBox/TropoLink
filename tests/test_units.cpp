#include "core/common/units.h"

#include <gtest/gtest.h>

#include <type_traits>

using namespace tl;
using namespace tl::literals;

// --- compile-time unit discipline (the static-assert tests of §10) ------------

template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };

// dBm + dBm is a category error and must not compile.
static_assert(!Addable<Dbm, Dbm>, "adding two absolute levels must not compile");
// Cross-dimension arithmetic must not compile.
static_assert(!Addable<Meters, Hertz>, "metres + hertz must not compile");
static_assert(!Addable<Meters, Decibels>, "metres + decibels must not compile");
static_assert(!Addable<Radians, Meters>, "radians + metres must not compile");
// The legitimate RF algebra must compile.
static_assert(Addable<Dbm, Decibels>, "level + gain must compile");
static_assert(Addable<Dbm, Dbi>, "EIRP = level + antenna gain must compile");
static_assert(Addable<Decibels, Decibels>, "dB + dB must compile");

TEST(Units, DecibelAlgebra) {
    const Dbm tx = 57.0_dBm;
    const Dbm eirp = tx - 0.5_dB + 39.1_dBi;
    EXPECT_NEAR(eirp.value(), 95.6, 1e-12);
    const Decibels difference = eirp - tx;
    EXPECT_NEAR(difference.value(), 38.6, 1e-12);
}

TEST(Units, WattsToDbm) {
    EXPECT_NEAR(Watts(500.0).dbm().value(), 56.9897, 1e-3);
    EXPECT_NEAR(Dbm(57.0).watts(), 501.187, 0.01);
}

TEST(Units, LengthAndFrequencyConversions) {
    EXPECT_DOUBLE_EQ((103.5_km).value(), 103500.0);
    EXPECT_DOUBLE_EQ(Meters(103500.0).kilometers(), 103.5);
    EXPECT_DOUBLE_EQ((4.4_GHz).megahertz(), 4400.0);
    EXPECT_DOUBLE_EQ(Hertz::fromMegahertz(4400.0).gigahertz(), 4.4);
}

TEST(Units, Angles) {
    EXPECT_NEAR(Radians::fromDegrees(180.0).value(), 3.14159265358979, 1e-12);
    EXPECT_NEAR(Radians(0.02).milliradians(), 20.0, 1e-12);
    EXPECT_NEAR((20.0_mrad).degrees(), 1.14591559, 1e-6);
    EXPECT_NEAR(Degrees(51.5).toRadians().value(), 0.8988445647, 1e-9);
}
