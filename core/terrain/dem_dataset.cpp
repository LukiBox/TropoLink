#include "core/terrain/dem_dataset.h"

#include <gdal_priv.h>

#include <cmath>
#include <mutex>

namespace tl::terrain {

namespace {
void ensureGdalInitialized() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        GDALAllRegister();
        CPLSetErrorHandler(CPLQuietErrorHandler); // no stderr chatter from a GUI tool
    });
}
} // namespace

DemDataset::~DemDataset() {
    if (dataset_ != nullptr) {
        GDALClose(dataset_);
    }
}

DemDataset::DemDataset(DemDataset&& other) noexcept {
    *this = std::move(other);
}

DemDataset& DemDataset::operator=(DemDataset&& other) noexcept {
    if (this != &other) {
        if (dataset_ != nullptr) {
            GDALClose(dataset_);
        }
        dataset_ = other.dataset_;
        other.dataset_ = nullptr;
        bounds_ = other.bounds_;
        path_ = std::move(other.path_);
        driver_ = std::move(other.driver_);
        resolution_ = other.resolution_;
        for (int i = 0; i < 6; ++i) {
            geoTransform_[i] = other.geoTransform_[i];
        }
        noData_ = other.noData_;
        hasNoData_ = other.hasNoData_;
        width_ = other.width_;
        height_ = other.height_;
    }
    return *this;
}

Expected<std::unique_ptr<DemDataset>> DemDataset::open(const std::string& utf8Path) {
    ensureGdalInitialized();

    auto* raw = static_cast<GDALDataset*>(GDALOpen(utf8Path.c_str(), GA_ReadOnly));
    if (raw == nullptr) {
        return Error{"GDAL cannot open elevation file: " + utf8Path};
    }
    auto dem = std::unique_ptr<DemDataset>(new DemDataset());
    dem->dataset_ = raw;
    dem->path_ = utf8Path;
    dem->driver_ = raw->GetDriver() != nullptr ? raw->GetDriver()->GetDescription() : "unknown";
    dem->width_ = raw->GetRasterXSize();
    dem->height_ = raw->GetRasterYSize();

    if (raw->GetGeoTransform(dem->geoTransform_) != CE_None) {
        return Error{"elevation file has no geotransform: " + utf8Path};
    }
    if (raw->GetRasterCount() < 1) {
        return Error{"elevation file has no raster band: " + utf8Path};
    }
    const double* gt = dem->geoTransform_;
    if (std::abs(gt[2]) > 1e-12 || std::abs(gt[4]) > 1e-12) {
        return Error{"rotated rasters are not supported: " + utf8Path};
    }

    // Corners (gt[5] is negative for north-up rasters).
    const double lon0 = gt[0];
    const double lon1 = gt[0] + gt[1] * dem->width_;
    const double lat0 = gt[3];
    const double lat1 = gt[3] + gt[5] * dem->height_;
    dem->bounds_.minLon = std::min(lon0, lon1);
    dem->bounds_.maxLon = std::max(lon0, lon1);
    dem->bounds_.minLat = std::min(lat0, lat1);
    dem->bounds_.maxLat = std::max(lat0, lat1);

    GDALRasterBand* band = raw->GetRasterBand(1);
    int hasNoData = 0;
    const double noData = band->GetNoDataValue(&hasNoData);
    dem->hasNoData_ = hasNoData != 0;
    dem->noData_ = noData;

    const double centreLatRad = (dem->bounds_.minLat + dem->bounds_.maxLat) * 0.5 * 3.14159265358979 / 180.0;
    const double metersPerDegLat = 111320.0;
    const double pixLatM = std::abs(gt[5]) * metersPerDegLat;
    const double pixLonM = std::abs(gt[1]) * metersPerDegLat * std::cos(centreLatRad);
    dem->resolution_ = Meters(std::max(1.0, std::min(pixLatM, pixLonM)));

    return dem;
}

std::optional<ElevationSample> DemDataset::sample(const geo::GeoPoint& p) const {
    if (!bounds_.contains(p)) {
        return std::nullopt;
    }
    const double* gt = geoTransform_;
    // Pixel-centre coordinates.
    const double px = (p.longitude.value() - gt[0]) / gt[1] - 0.5;
    const double py = (p.latitude.value() - gt[3]) / gt[5] - 0.5;

    const int x0 = std::clamp(static_cast<int>(std::floor(px)), 0, width_ - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(py)), 0, height_ - 1);
    const int x1 = std::min(x0 + 1, width_ - 1);
    const int y1 = std::min(y0 + 1, height_ - 1);
    const double fx = std::clamp(px - x0, 0.0, 1.0);
    const double fy = std::clamp(py - y0, 0.0, 1.0);

    float quad[4] = {};
    GDALRasterBand* band = dataset_->GetRasterBand(1);
    // Read the 2x2 neighbourhood (two 1x2 reads keeps the window in-bounds).
    if (band->RasterIO(GF_Read, x0, y0, x1 - x0 + 1, 1, &quad[0], x1 - x0 + 1, 1, GDT_Float32, 0, 0) !=
        CE_None) {
        return ElevationSample{Meters(0.0), true};
    }
    if (band->RasterIO(GF_Read, x0, y1, x1 - x0 + 1, 1, &quad[2], x1 - x0 + 1, 1, GDT_Float32, 0, 0) !=
        CE_None) {
        return ElevationSample{Meters(0.0), true};
    }
    if (x1 == x0) {
        quad[1] = quad[0];
        quad[3] = quad[2];
    }

    auto isVoidValue = [this](float v) {
        if (hasNoData_ && std::abs(static_cast<double>(v) - noData_) < 0.5) {
            return true;
        }
        // DTED/SRTM magic void value even when the driver does not report NODATA.
        return v <= -32000.0f || std::isnan(v);
    };

    const bool voids[4] = {isVoidValue(quad[0]), isVoidValue(quad[1]), isVoidValue(quad[2]),
                           isVoidValue(quad[3])};
    const int voidCount = static_cast<int>(voids[0]) + static_cast<int>(voids[1]) +
                          static_cast<int>(voids[2]) + static_cast<int>(voids[3]);

    if (voidCount == 0) {
        const double top = quad[0] * (1.0 - fx) + quad[1] * fx;
        const double bottom = quad[2] * (1.0 - fx) + quad[3] * fx;
        return ElevationSample{Meters(top * (1.0 - fy) + bottom * fy), false};
    }
    if (voidCount == 4) {
        return ElevationSample{Meters(0.0), true};
    }
    // Partial void: use the nearest valid neighbour rather than inventing a blend.
    const double dist[4] = {fx * fx + fy * fy, (1 - fx) * (1 - fx) + fy * fy, fx * fx + (1 - fy) * (1 - fy),
                            (1 - fx) * (1 - fx) + (1 - fy) * (1 - fy)};
    // Index into quad by proximity: quad[0] is (x0,y0) so its distance is fx,fy from that corner.
    int best = -1;
    double bestDist = 1e9;
    const double cornerDist[4] = {dist[0], dist[1], dist[2], dist[3]};
    for (int i = 0; i < 4; ++i) {
        if (!voids[i] && cornerDist[i] < bestDist) {
            bestDist = cornerDist[i];
            best = i;
        }
    }
    return ElevationSample{Meters(static_cast<double>(quad[best])), false};
}

} // namespace tl::terrain
