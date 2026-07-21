#pragma once

// Tile sources for the map engine.
//
//  * MBTilesSource: raster topo packs (MBTiles = SQLite; TMS row order). Read-only,
//    fully offline.
//  * TopoTileSource: "paper topographic map" rendering straight from the loaded
//    DEMs in background threads — hypsometric tint, Horn hillshade, elevation
//    contours with labelled index contours. Fully offline; this is the default look.
//  * HttpTileSource (standard flavor only): online XYZ raster tiles (OpenTopoMap /
//    OSM) with a persistent on-disk cache. The Air-Gap flavor compiles the network
//    path out entirely; the class then only serves from an existing disk cache.
//
// All are thread-safe producers: tiles are rendered/decoded on the global thread
// pool and delivered through the tileReady signal.

#include "core/terrain/terrain_store.h"

#include <QCache>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QSet>

#include <memory>

struct sqlite3;
class QNetworkAccessManager;

struct TileKey {
    int z = 0;
    int x = 0;
    int y = 0; // XYZ (web) scheme
    friend bool operator==(const TileKey& a, const TileKey& b) {
        return a.z == b.z && a.x == b.x && a.y == b.y;
    }
};

// The paper-topo tile render itself (pure function of the store and key); exposed
// for the offscreen render-preview tool alongside the async TopoTileSource.
QImage renderTopoTile(const tl::terrain::TerrainStore* store, const TileKey& key, bool dark);
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
    [[nodiscard]] QString attribution() const { return attribution_; }

    // Returns a cached tile or null; schedules decoding when missing. A 1x1 image
    // marks "pack has no such tile" so callers can fall through to another layer.
    QImage tile(const TileKey& key);

  signals:
    void tileReady();

  private:
    sqlite3* db_ = nullptr;
    QString path_;
    QCache<TileKey, QImage> cache_{48 * 1024 * 1024}; // cost = bytes
    QSet<TileKey> pending_;
    int minZoom_ = 0;
    int maxZoom_ = 14;
    QString attribution_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

// Paper-topographic renderer over the terrain store.
class TopoTileSource : public QObject {
    Q_OBJECT
  public:
    explicit TopoTileSource(tl::terrain::TerrainStore* store, QObject* parent = nullptr);
    ~TopoTileSource() override;

    QImage tile(const TileKey& key, bool darkTheme);
    void invalidate(); // terrain store changed

  signals:
    void tileReady();

  private:
    tl::terrain::TerrainStore* store_;
    QCache<TileKey, QImage> cache_{96 * 1024 * 1024}; // cost = bytes
    QSet<TileKey> pending_;
    quint64 storeRevision_ = 0;
    bool darkTheme_ = true;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

// Online XYZ raster source with persistent disk cache (AppData/tiles/<id>/z/x/y.png).
// {z}/{x}/{y} and {s} (subdomain a/b/c) placeholders. In the Air-Gap flavor the
// network fetch is compiled out; only previously cached tiles are served.
class HttpTileSource : public QObject {
    Q_OBJECT
  public:
    HttpTileSource(const QString& id, const QString& urlTemplate, int maxZoom, QObject* parent = nullptr);
    ~HttpTileSource() override;

    [[nodiscard]] int maxZoom() const { return maxZoom_; }
    [[nodiscard]] QString id() const { return id_; }
    static QString cacheDirFor(const QString& id);

    QImage tile(const TileKey& key);

  signals:
    void tileReady();

  private:
    void pump(); // start queued fetches, max kConcurrent in flight

    QString id_;
    QString urlTemplate_;
    QString cacheDir_;
    int maxZoom_ = 17;
    QCache<TileKey, QImage> cache_{48 * 1024 * 1024}; // cost = bytes
    QSet<TileKey> pending_;                           // being loaded (disk or network)
    QQueue<TileKey> fetchQueue_;
    int inFlight_ = 0;
    QNetworkAccessManager* nam_ = nullptr; // created lazily, standard flavor only
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};
