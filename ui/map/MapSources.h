#pragma once

// Offline tile sources for the map engine.
//
//  * MBTilesSource: raster topo packs (MBTiles = SQLite; TMS row order). Read-only,
//    fully offline.
//  * HillshadeSource: shaded relief rendered from the loaded DEMs themselves in
//    background threads (Horn's method), so terrain is visible with no basemap pack.
//
// Both are thread-safe producers: tiles are rendered/decoded on the global thread
// pool and delivered through the tileReady signal.

#include "core/terrain/terrain_store.h"

#include <QCache>
#include <QImage>
#include <QObject>
#include <QSet>

#include <memory>

struct sqlite3;

struct TileKey {
    int z = 0;
    int x = 0;
    int y = 0; // XYZ (web) scheme
    friend bool operator==(const TileKey& a, const TileKey& b) {
        return a.z == b.z && a.x == b.x && a.y == b.y;
    }
};
inline size_t qHash(const TileKey& k, size_t seed = 0) {
    return ::qHash((quint64(k.z) << 48) ^ (quint64(k.x) << 24) ^ quint64(k.y), seed);
}

class MBTilesSource : public QObject {
    Q_OBJECT
public:
    explicit MBTilesSource(QObject* parent = nullptr);
    ~MBTilesSource() override;

    bool open(const QString& path);
    void close();
    [[nodiscard]] bool isOpen() const { return db_ != nullptr; }
    [[nodiscard]] int minZoom() const { return minZoom_; }
    [[nodiscard]] int maxZoom() const { return maxZoom_; }

    // Returns a cached tile or null; schedules decoding when missing.
    QImage tile(const TileKey& key);

signals:
    void tileReady();

private:
    sqlite3* db_ = nullptr;
    QString path_;
    QCache<TileKey, QImage> cache_{512};
    QSet<TileKey> pending_;
    int minZoom_ = 0;
    int maxZoom_ = 14;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

class HillshadeSource : public QObject {
    Q_OBJECT
public:
    explicit HillshadeSource(tl::terrain::TerrainStore* store, QObject* parent = nullptr);
    ~HillshadeSource() override;

    QImage tile(const TileKey& key, bool darkTheme);
    void invalidate(); // terrain store changed

signals:
    void tileReady();

private:
    tl::terrain::TerrainStore* store_;
    QCache<TileKey, QImage> cache_{512};
    QSet<TileKey> pending_;
    quint64 storeRevision_ = 0;
    bool darkTheme_ = true;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};
