#include "core/common/sha256.h"
#include "core/project/csv_export.h"
#include "core/project/kml_export.h"
#include "core/project/project.h"
#include "core/report/report_content.h"
#include "core/tropo/model_suite.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace tl;
using namespace tl::project;

namespace {
std::string tempTlk(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "tropolink_tests";
    std::filesystem::create_directories(dir);
    return (dir / name).string();
}

terrain::Profile flatProfile(double distanceM, double elevationM, int n = 601) {
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

TEST(Sha256, KnownVectors) {
    EXPECT_EQ(Sha256::hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(Sha256::hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(Sha256::hex("The quick brown fox jumps over the lazy dog"),
              "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST(Project, TlkRoundTripLossless) {
    Project original = referenceProject();
    original.snapshots.push_back(ResultSnapshot{"link-1",
                                                "2026-07-18T12:00:00Z",
                                                "1.0.0",
                                                "3.9",
                                                "2.3",
                                                {{"P.617", "P.617-5"}},
                                                {{"distance_km", 103.5}, {"median_db", 210.7}},
                                                "deadbeef"});

    std::string stage = "serialize";
    try {
        const std::string beforeJson = projectToJson(original);
        EXPECT_FALSE(beforeJson.empty());
        const auto path = tempTlk("roundtrip.tlk");
        stage = "save";
        ASSERT_TRUE(saveProject(original, path));
        stage = "load";
        const auto loaded = loadProject(path);
        ASSERT_TRUE(loaded.hasValue()) << (loaded.hasValue() ? std::string() : loaded.error().message);
        stage = "reserialize";
        // Lossless: canonical JSON of saved and reloaded projects must be identical.
        EXPECT_EQ(projectToJson(original), projectToJson(loaded.value()));
    } catch (const std::exception& e) {
        FAIL() << "exception at stage '" << stage << "': " << e.what();
    }
}

TEST(Project, SchemaVersionGuard) {
    const auto rejected = projectFromJson(R"({"schemaVersion": 99, "name": "future"})");
    EXPECT_FALSE(rejected.hasValue());
}

TEST(Project, ReferenceProjectMatchesBrief) {
    const auto p = referenceProject();
    ASSERT_EQ(p.sites.size(), 2U);
    EXPECT_NEAR(p.sites[0].position.latitude.value(), 51.50609699, 1e-9);
    EXPECT_NEAR(p.sites[1].position.latitude.value(), 52.43470597, 1e-9);
    ASSERT_EQ(p.links.size(), 1U);
    EXPECT_NEAR(p.links[0].frequency.gigahertz(), 4.4, 1e-12);
    EXPECT_NEAR(p.links[0].radio.txPower.value(), 57.0, 1e-12);
    EXPECT_NEAR(p.links[0].radio.antennaGainA.value(), 39.1, 1e-12);
    EXPECT_NEAR(p.sites[0].antennaHeightAgl.value(), 4.0, 1e-12);
}

TEST(Project, KmlContainsSitesPathAndVolume) {
    const auto p = referenceProject();
    tropo::SuiteInput in;
    in.siteA = p.sites[0].position;
    in.siteB = p.sites[1].position;
    const auto inverse = geo::Geodesy::inverse(in.siteA, in.siteB);
    const auto profile = flatProfile(inverse.distance.value(), 120.0);
    const auto suite = tropo::runSuite(in, profile);

    const auto kml = buildKml(p, p.links[0], suite);
    EXPECT_NE(kml.find("<kml"), std::string::npos);
    EXPECT_NE(kml.find("Site A"), std::string::npos);
    EXPECT_NE(kml.find("Site B"), std::string::npos);
    EXPECT_NE(kml.find("LineString"), std::string::npos);
    EXPECT_NE(kml.find("Common volume"), std::string::npos);
    // Basic well-formedness: every open tag family used is closed.
    EXPECT_NE(kml.find("</Document>"), std::string::npos);
    EXPECT_NE(kml.find("</kml>"), std::string::npos);
}

TEST(Report, ContentHashDeterministic) {
    const auto p = referenceProject();
    tropo::SuiteInput in;
    in.siteA = p.sites[0].position;
    in.siteB = p.sites[1].position;
    in.atmosphere.seaLevelN0 = 320.0;
    const auto inverse = geo::Geodesy::inverse(in.siteA, in.siteB);
    const auto profile = flatProfile(inverse.distance.value(), 120.0);
    const auto suite = tropo::runSuite(in, profile);
    const auto primary = suite.primaryModel();
    ASSERT_NE(primary, nullptr);
    const budget::AvailabilityEngine engine(*primary);
    const auto budget = budget::computeLinkBudget(p.links[0].radio, primary->medianLoss());

    report::ReportInputs inputs{p,
                                p.links[0],
                                suite,
                                budget,
                                engine,
                                engine.availability(budget.fadeMargin, p.links[0].diversity, false),
                                engine.availability(budget.fadeMargin, p.links[0].diversity, true),
                                budget::diversitySeparation(p.links[0].antennaDiameter, p.links[0].frequency,
                                                            suite.geometry.scatterAngle,
                                                            suite.inverse.distance),
                                {},
                                "1.0.0",
                                "3.x",
                                "2.x",
                                "2026-07-18T12:00:00Z",
                                ""};

    auto contentPl = report::buildReportContent(inputs, report::Language::Polish);
    auto contentPl2 = report::buildReportContent(inputs, report::Language::Polish);
    EXPECT_EQ(contentPl.contentSha256, contentPl2.contentSha256);
    EXPECT_FALSE(contentPl.contentSha256.empty());

    auto contentEn = report::buildReportContent(inputs, report::Language::English);
    EXPECT_NE(contentEn.contentSha256, contentPl.contentSha256); // language changes content
    // Bilingual sanity: the Polish report holds Polish section titles.
    EXPECT_EQ(contentPl.sections.front().title, "Stanowiska");
    EXPECT_EQ(contentEn.sections.front().title, "Sites");
}

TEST(Project, CsvExports) {
    const auto profile = flatProfile(1000.0, 55.0, 11);
    const auto csv = profileCsv(profile);
    EXPECT_NE(csv.find("distance_m"), std::string::npos);
    // Header + 11 samples.
    EXPECT_EQ(std::count(csv.begin(), csv.end(), '\n'), 12);
}
