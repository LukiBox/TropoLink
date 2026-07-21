#pragma once

// Managed, spatially indexed local terrain store.
//
// Imported files (DTED 0/1/2, SRTM HGT, GeoTIFF) are copied into the store directory;
// index.json records provenance (imported vs downloaded), format and coverage.
// Elevation queries pick the finest-resolution dataset covering the point and fall
// through to coarser data on voids.
//
// Thread-safety: the index is guarded by a shared_mutex; GDAL handles are kept
// per-thread, so any number of worker threads may query concurrently.

#include "core/common/expected.h"
#include "core/terrain/dem_dataset.h"

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <vector>

namespace tl::terrain {

enum class Provenance { Imported, Downloaded };

struct StoreEntry {
    std::string fileName; // relative to store directory
    std::string format;   // GDAL driver name (DTED, SRTMHGT, GTiff, ...)
    Provenance provenance = Provenance::Imported;
    BoundingBox bounds;
    double resolutionM = 90.0;
    std::uint64_t fileSize = 0;
};

class TerrainStore {
  public:
    // Opens (creating if needed) a store rooted at the given directory (UTF-8 path).
    [[nodiscard]] static Expected<std::unique_ptr<TerrainStore>> open(const std::string& directory);

    // Copies the file into the store, indexes it, persists index.json.
    [[nodiscard]] Expected<StoreEntry> importFile(const std::string& utf8Path, Provenance provenance);
    [[nodiscard]] Status removeEntry(const std::string& fileName);

    [[nodiscard]] std::vector<StoreEntry> entries() const;
    [[nodiscard]] const std::string& directory() const { return directory_; }

    // Finest-resolution elevation at a point; falls back across datasets on voids.
    // nullopt = no coverage. isVoid = covered but NODATA everywhere.
    [[nodiscard]] std::optional<ElevationSample> elevationAt(const geo::GeoPoint& p) const;

    [[nodiscard]] bool covers(const geo::GeoPoint& p) const;

    // Monotonic revision, bumped on every import/remove; lets caches invalidate.
    [[nodiscard]] std::uint64_t revision() const { return revision_; }

  private:
    explicit TerrainStore(std::string directory) : directory_(std::move(directory)) {}
    [[nodiscard]] Status loadIndex();
    [[nodiscard]] Status saveIndex() const;

    std::string directory_;
    mutable std::shared_mutex mutex_;
    std::vector<StoreEntry> entries_; // kept sorted by resolution, finest first
    std::uint64_t revision_ = 0;
};

} // namespace tl::terrain
