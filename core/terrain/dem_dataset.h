#pragma once

// A single elevation dataset (DTED 0/1/2, SRTM HGT, GeoTIFF) opened through GDAL.
//
// Thread-safety: GDAL dataset handles are NOT thread-safe. A DemDataset instance may
// only be used from one thread at a time; TerrainStore maintains per-thread handles.
// All file paths are UTF-8 (GDAL_FILENAME_IS_UTF8 default).

#include "core/common/expected.h"
#include "core/common/units.h"
#include "core/geo/geodesy.h"

#include <memory>
#include <optional>
#include <string>

class GDALDataset; // fwd — GDAL header stays out of our public headers

namespace tl::terrain {

struct BoundingBox {
    double minLat = 0.0;
    double maxLat = 0.0;
    double minLon = 0.0;
    double maxLon = 0.0;

    [[nodiscard]] bool contains(const geo::GeoPoint& p) const {
        return p.latitude.value() >= minLat && p.latitude.value() <= maxLat &&
               p.longitude.value() >= minLon && p.longitude.value() <= maxLon;
    }
};

// Elevation sample: value plus a void flag. A void means the source held NODATA at the
// location (e.g. an SRTM void); the caller decides how to interpolate and must flag it.
struct ElevationSample {
    Meters elevation{0.0};
    bool isVoid = false;
};

class DemDataset {
public:
    ~DemDataset();
    DemDataset(DemDataset&&) noexcept;
    DemDataset& operator=(DemDataset&&) noexcept;
    DemDataset(const DemDataset&) = delete;
    DemDataset& operator=(const DemDataset&) = delete;

    [[nodiscard]] static Expected<std::unique_ptr<DemDataset>> open(const std::string& utf8Path);

    [[nodiscard]] const BoundingBox& bounds() const { return bounds_; }
    [[nodiscard]] const std::string& path() const { return path_; }
    // Approximate ground resolution of one pixel at the dataset centre.
    [[nodiscard]] Meters resolution() const { return resolution_; }
    [[nodiscard]] const std::string& driverName() const { return driver_; }

    // Bilinear elevation. Returns nullopt when outside coverage; a sample with
    // isVoid = true when inside coverage but NODATA.
    [[nodiscard]] std::optional<ElevationSample> sample(const geo::GeoPoint& p) const;

private:
    DemDataset() = default;

    GDALDataset* dataset_ = nullptr;
    BoundingBox bounds_;
    std::string path_;
    std::string driver_;
    Meters resolution_{90.0};
    double geoTransform_[6] = {};
    double noData_ = -32767.0;
    bool hasNoData_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace tl::terrain
