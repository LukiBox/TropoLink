#include "core/terrain/horizon.h"
#include "core/terrain/profile.h"
#include "core/terrain/terrain_store.h"

#include <gdal_priv.h>
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace tl;
using namespace tl::geo;
using namespace tl::terrain;

namespace {

namespace fs = std::filesystem;

fs::path testDir() {
    const auto dir = fs::temp_directory_path() / "tropolink_tests";
    fs::create_directories(dir);
    return dir;
}

// Writes a synthetic GeoTIFF DEM over [minLat..maxLat] x [minLon..maxLon] where the
// elevation is an analytic function of position.
template <typename F>
std::string writeSyntheticDem(const std::string& name, double minLat, double maxLat, double minLon,
                              double maxLon, int size, F&& elevation, double noDataValue = -32767.0) {
    GDALAllRegister();
    const fs::path path = testDir() / name;
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* ds = driver->Create(path.string().c_str(), size, size, 1, GDT_Float32, nullptr);
    const double pixLon = (maxLon - minLon) / size;
    const double pixLat = (maxLat - minLat) / size;
    double gt[6] = {minLon, pixLon, 0.0, maxLat, 0.0, -pixLat};
    ds->SetGeoTransform(gt);
    ds->GetRasterBand(1)->SetNoDataValue(noDataValue);
    std::vector<float> row(static_cast<std::size_t>(size));
    for (int y = 0; y < size; ++y) {
        const double lat = maxLat - (y + 0.5) * pixLat;
        for (int x = 0; x < size; ++x) {
            const double lon = minLon + (x + 0.5) * pixLon;
            row[static_cast<std::size_t>(x)] = static_cast<float>(elevation(lat, lon));
        }
        (void)ds->GetRasterBand(1)->RasterIO(GF_Write, 0, y, size, 1, row.data(), size, 1, GDT_Float32, 0, 0);
    }
    GDALClose(ds);
    return path.string();
}

Profile syntheticProfile(int n, double stepM, const std::function<double(double)>& elevAt) {
    Profile p;
    p.totalDistance = Meters(stepM * (n - 1));
    p.step = Meters(stepM);
    p.hasCoverage = true;
    for (int i = 0; i < n; ++i) {
        ProfilePoint pt;
        pt.distance = Meters(stepM * i);
        pt.elevation = Meters(elevAt(stepM * i));
        p.points.push_back(pt);
    }
    return p;
}

} // namespace

TEST(Terrain, PlaneDemProfileMatchesAnalytic) {
    // Plane: h = 100 + 1000*(lat - 51) + 500*(lon - 15)  [degrees -> metres]
    const auto path = writeSyntheticDem("plane.tif", 51.0, 52.0, 15.0, 16.0, 400, [](double lat, double lon) {
        return 100.0 + 1000.0 * (lat - 51.0) + 500.0 * (lon - 15.0);
    });
    const auto storeDir = (testDir() / "store_plane").string();
    auto store = TerrainStore::open(storeDir);
    ASSERT_TRUE(store.hasValue());
    ASSERT_TRUE(store.value()->importFile(path, Provenance::Imported).hasValue());

    ProfileRequest req;
    req.siteA = GeoPoint{Degrees(51.2), Degrees(15.2)};
    req.siteB = GeoPoint{Degrees(51.8), Degrees(15.8)};
    const auto profile = extractProfile(*store.value(), req);
    ASSERT_TRUE(profile.hasValue());
    EXPECT_FALSE(profile.value().hasVoids);
    ASSERT_GT(profile.value().points.size(), 100U);
    for (const auto& pt : profile.value().points) {
        const double expected = 100.0 + 1000.0 * (pt.position.latitude.value() - 51.0) +
                                500.0 * (pt.position.longitude.value() - 15.0);
        // Bilinear interpolation of a plane is exact up to pixel-centre quantization.
        EXPECT_NEAR(pt.elevation.value(), expected, 3.5);
    }
}

TEST(Terrain, VoidsAreInterpolatedAndFlagged) {
    // A DEM with a NODATA hole in the middle band.
    const auto path =
        writeSyntheticDem("voids.tif", 51.0, 52.0, 15.0, 16.0, 200, [](double lat, double lon) -> double {
            if (lon > 15.45 && lon < 15.55) {
                return -32767.0; // void
            }
            return 200.0 + 100.0 * std::sin(lat * 3.0) + 100.0 * std::cos(lon * 2.0);
        });
    const auto storeDir = (testDir() / "store_voids").string();
    auto store = TerrainStore::open(storeDir);
    ASSERT_TRUE(store.hasValue());
    ASSERT_TRUE(store.value()->importFile(path, Provenance::Imported).hasValue());

    ProfileRequest req;
    req.siteA = GeoPoint{Degrees(51.5), Degrees(15.2)};
    req.siteB = GeoPoint{Degrees(51.5), Degrees(15.8)};
    const auto profile = extractProfile(*store.value(), req);
    ASSERT_TRUE(profile.hasValue());
    EXPECT_TRUE(profile.value().hasVoids);
    bool sawFlagged = false;
    for (const auto& pt : profile.value().points) {
        if (pt.interpolatedVoid) {
            sawFlagged = true;
            // Interpolated, not silently invented: value must lie between neighbours.
            EXPECT_GT(pt.elevation.value(), 50.0);
            EXPECT_LT(pt.elevation.value(), 450.0);
        }
    }
    EXPECT_TRUE(sawFlagged);
}

TEST(Terrain, VeryLongPathSampleCountStaysBounded) {
    // Regression: an intercontinental path used to be sampled at the DEM step for its
    // whole length, allocating tens of thousands of points per interactive recompute
    // (the UI froze and eventually died while a pin was being dragged). The extractor
    // now widens the step instead, capping the profile at 16384 samples while keeping
    // the endpoints and total distance exact.
    const auto path = writeSyntheticDem("longpath.tif", 35.0, 61.0, 14.0, 16.0, 64,
                                        [](double lat, double) { return 50.0 + 2.0 * (lat - 35.0); });
    const auto storeDir = (testDir() / "store_long").string();
    auto store = TerrainStore::open(storeDir);
    ASSERT_TRUE(store.hasValue());
    ASSERT_TRUE(store.value()->importFile(path, Provenance::Imported).hasValue());

    ProfileRequest req; // ~2670 km, Mediterranean to central Sweden
    req.siteA = GeoPoint{Degrees(36.0), Degrees(15.0)};
    req.siteB = GeoPoint{Degrees(60.0), Degrees(15.0)};
    const auto profile = extractProfile(*store.value(), req);
    ASSERT_TRUE(profile.hasValue());
    const auto& p = profile.value();
    EXPECT_LE(p.points.size(), 16384U);
    EXPECT_GT(p.points.size(), 4096U); // still finely sampled, not degenerate
    EXPECT_NEAR(p.totalDistance.kilometers(), 2664.0, 20.0);
    EXPECT_NEAR(p.points.back().distance.value(), p.totalDistance.value(), 1.0);
    // Step widened to cover the distance with the capped sample count.
    EXPECT_NEAR(p.step.value() * (p.points.size() - 1), p.totalDistance.value(), 1.0);
}

TEST(Terrain, HorizonAnglesMatchHandComputedFixture) {
    // Flat 100 m terrain with a single 500 m ridge at 30 km out of 100 km.
    // Antenna A at 100 + 10 m AGL. Hand-computed takeoff angle to the ridge crest:
    //   angle = (500 - 110) / 30000 - 30000 / (2 * (4/3) * 6370000)
    //         = 0.0130 - 0.0017657... = 0.0112343 rad
    const double ka = (4.0 / 3.0) * 6370000.0;
    const double expectedA = (500.0 - 110.0) / 30000.0 - 30000.0 / (2.0 * ka);
    // Ridge exactly one sample wide so the crest is the unique horizon point.
    auto profile = syntheticProfile(1001, 100.0, [](double d) {
        if (std::abs(d - 30000.0) < 50.0) {
            return 500.0;
        }
        return 100.0;
    });
    HorizonRequest req;
    req.antennaHeightAglA = Meters(10.0);
    req.antennaHeightAglB = Meters(10.0);
    req.kFactor = 4.0 / 3.0;
    const auto result = analyzeHorizons(profile, req);
    EXPECT_NEAR(result.takeoffAngleA.value(), expectedA, 1e-5);
    EXPECT_NEAR(result.horizonDistanceA.value(), 30000.0, 200.0);
    EXPECT_TRUE(result.directPathObstructed);
    EXPECT_FALSE(result.fresnelZoneClear);
}

TEST(Terrain, SmoothEarthNegativeTakeoff) {
    // Featureless flat terrain: takeoff angles must go negative (below horizontal)
    // and the direct path must still be obstructed by the bulge on a 100 km path.
    auto profile = syntheticProfile(1001, 100.0, [](double) { return 100.0; });
    HorizonRequest req;
    req.antennaHeightAglA = Meters(4.0);
    req.antennaHeightAglB = Meters(4.0);
    req.kFactor = 4.0 / 3.0;
    const auto result = analyzeHorizons(profile, req);
    EXPECT_LT(result.takeoffAngleA.value(), 0.0);
    EXPECT_TRUE(result.directPathObstructed);
}

TEST(Terrain, EarthBulgeAndFresnel) {
    // Mid-path bulge on 100 km with k = 4/3: d1 d2 / (2 k a) = 50e3^2/(2*8.4933e6*1e3)
    const double bulge = earthBulge(Meters(50000.0), Meters(100000.0), 4.0 / 3.0).value();
    EXPECT_NEAR(bulge, 147.2, 1.0);
    // First Fresnel radius mid-path at 4.4 GHz over 100 km: sqrt(lambda d1 d2 / d).
    const double f1 = fresnelRadius1(Meters(50000.0), Meters(100000.0), Hertz::fromGigahertz(4.4)).value();
    EXPECT_NEAR(f1, std::sqrt(0.06813 * 25000.0), 1.0);
}
