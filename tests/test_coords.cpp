#include "core/geo/coords.h"

#include <gtest/gtest.h>

using namespace tl;
using namespace tl::geo;

TEST(Coords, ParseDecimalDegreesWithHemisphere) {
    const auto p = Coords::parse("51.50609699N, 15.33150851E");
    ASSERT_TRUE(p.hasValue());
    EXPECT_NEAR(p.value().latitude.value(), 51.50609699, 1e-9);
    EXPECT_NEAR(p.value().longitude.value(), 15.33150851, 1e-9);
}

TEST(Coords, ParsePlainPairAndNegative) {
    const auto p = Coords::parse("-33.865143 151.209900");
    ASSERT_TRUE(p.hasValue());
    EXPECT_NEAR(p.value().latitude.value(), -33.865143, 1e-9);
    EXPECT_NEAR(p.value().longitude.value(), 151.2099, 1e-9);
}

TEST(Coords, ParseDmsVariants) {
    const auto p1 = Coords::parse("51\xC2\xB0"
                                  "30'21.9\"N 15\xC2\xB0"
                                  "19'53.4\"E");
    ASSERT_TRUE(p1.hasValue());
    EXPECT_NEAR(p1.value().latitude.value(), 51.50608333, 1e-6);
    EXPECT_NEAR(p1.value().longitude.value(), 15.33150, 1e-4);

    const auto p2 = Coords::parse("51d30m21.9sN, 15d19m53.4sE");
    ASSERT_TRUE(p2.hasValue());
    EXPECT_NEAR(p2.value().latitude.value(), p1.value().latitude.value(), 1e-9);

    const auto p3 = Coords::parse("51 30 21.9 N, 15 19 53.4 E");
    ASSERT_TRUE(p3.hasValue());
    EXPECT_NEAR(p3.value().longitude.value(), p1.value().longitude.value(), 1e-9);
}

TEST(Coords, MgrsRoundTripExact) {
    const GeoPoint site{Degrees(51.50609699), Degrees(15.33150851)};
    const auto mgrs = Coords::formatMgrs(site);
    ASSERT_TRUE(mgrs.hasValue());
    const auto back = Coords::parse(mgrs.value());
    ASSERT_TRUE(back.hasValue());
    // 1 m MGRS precision -> centre-of-cell round trip within ~1 m (~1e-5 deg).
    EXPECT_NEAR(back.value().latitude.value(), site.latitude.value(), 2e-5);
    EXPECT_NEAR(back.value().longitude.value(), site.longitude.value(), 2e-5);
}

TEST(Coords, MgrsParseWithAndWithoutSpaces) {
    const auto a = Coords::parse("33UXT 66055 07249");
    const auto b = Coords::parse("33UXT6605507249");
    ASSERT_TRUE(a.hasValue());
    ASSERT_TRUE(b.hasValue());
    EXPECT_NEAR(a.value().latitude.value(), b.value().latitude.value(), 1e-9);
}

TEST(Coords, UtmRoundTrip) {
    const GeoPoint site{Degrees(51.50609699), Degrees(15.33150851)};
    const auto utm = Coords::formatUtm(site);
    ASSERT_TRUE(utm.hasValue());
    const auto back = Coords::parse(utm.value());
    ASSERT_TRUE(back.hasValue());
    EXPECT_NEAR(back.value().latitude.value(), site.latitude.value(), 2e-5);
    EXPECT_NEAR(back.value().longitude.value(), site.longitude.value(), 2e-5);
}

TEST(Coords, RejectsGarbage) {
    EXPECT_FALSE(Coords::parse("").hasValue());
    EXPECT_FALSE(Coords::parse("hello world").hasValue());
    EXPECT_FALSE(Coords::parse("99.9N 200.0E").hasValue());
}

TEST(Coords, FormatsAreStable) {
    const GeoPoint site{Degrees(51.50609699), Degrees(15.33150851)};
    EXPECT_EQ(Coords::formatDecimalDegrees(site), "51.506097N 15.331509E");
    const auto dms = Coords::formatDms(site);
    EXPECT_NE(dms.find("51\xC2\xB0"), std::string::npos);
    EXPECT_NE(dms.find("N"), std::string::npos);
}
