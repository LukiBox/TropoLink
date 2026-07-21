#include "ui/map/TileMapItem.h"

#include "ui/models/AppController.h"

#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QtMath>

#include <cmath>

namespace {
constexpr double kTileSize = 256.0;

double lonToWorldX(double lon) { return (lon + 180.0) / 360.0; }
double latToWorldY(double lat) {
    const double rad = lat * M_PI / 180.0;
    return (1.0 - std::asinh(std::tan(rad)) / M_PI) / 2.0;
}
double worldYToLat(double y) {
    return std::atan(std::sinh(M_PI * (1.0 - 2.0 * y))) * 180.0 / M_PI;
}

// Texture cache key: layer (2 bits) | z (6) | x (24) | y (24).
quint64 texKey(int layer, const TileKey& k) {
    return (quint64(layer) << 56) | (quint64(k.z) << 48) | (quint64(k.x) << 24) | quint64(k.y);
}

// The scene-graph root owns the per-tile textures: the render thread deletes it
// (and therefore them) whenever the item or the window goes away — correct
// lifetime with no manual cross-thread cleanup.
class TileCacheNode : public QSGNode {
public:
    ~TileCacheNode() override { qDeleteAll(textures); }
    QHash<quint64, QSGTexture*> textures;
    QSet<quint64> usedThisFrame;
    quint64 epoch = ~quint64(0);
};

} // namespace

TileMapItem::TileMapItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::NoButton); // interaction handled by QML handlers
}

void TileMapItem::bumpEpoch() {
    ++epoch_;
    update();
}

void TileMapItem::setCenterLat(double v) {
    centerLat_ = std::clamp(v, -85.0, 85.0);
    emit viewChanged();
    update();
}
void TileMapItem::setCenterLon(double v) {
    centerLon_ = std::clamp(v, -180.0, 180.0);
    emit viewChanged();
    update();
}
void TileMapItem::setZoom(double v) {
    zoom_ = std::clamp(v, kMinZoom, kMaxZoom);
    emit viewChanged();
    update();
}

void TileMapItem::setController(AppController* c) {
    if (controller_ == c) {
        return;
    }
    controller_ = c;
    topo_.reset();
    if (controller_ != nullptr && controller_->terrainStore() != nullptr) {
        topo_ = std::make_unique<TopoTileSource>(controller_->terrainStore());
        connect(topo_.get(), &TopoTileSource::tileReady, this, [this] { update(); });
        connect(controller_, &AppController::terrainChanged, this, [this] {
            if (topo_) {
                topo_->invalidate();
            }
            bumpEpoch();
        });
    }
    emit controllerChanged();
    bumpEpoch();
}

void TileMapItem::setDarkTheme(bool v) {
    if (darkTheme_ == v) {
        return;
    }
    darkTheme_ = v;
    emit viewChanged();
    bumpEpoch();
}

void TileMapItem::setBasemapPath(const QString& path) {
    // Guard required, not merely an optimisation: basemapChanged() is the NOTIFY
    // for this property, so re-emitting it re-evaluates the QML binding that feeds
    // it, calling straight back in here — a binding loop that also reopened the
    // MBTiles SQLite file on every pass.
    if (basemapPath_ == path) {
        return;
    }
    basemapPath_ = path;
    if (path.isEmpty()) {
        basemap_.reset();
    } else {
        basemap_ = std::make_unique<MBTilesSource>();
        if (!basemap_->open(path)) {
            basemap_.reset();
            basemapPath_.clear();
        } else {
            connect(basemap_.get(), &MBTilesSource::tileReady, this, [this] { update(); });
        }
    }
    emit basemapChanged();
    bumpEpoch();
}

void TileMapItem::setOnlineSource(const QString& id) {
    if (onlineSourceId_ == id) {
        return;
    }
    onlineSourceId_ = id;
    online_.reset();
#ifndef TROPOLINK_AIRGAP
    // Remote endpoints exist only in the standard flavor: the Air-Gap binary
    // contains no tile URL at all, not merely no code to fetch them.
    if (id == QLatin1String("opentopomap")) {
        online_ = std::make_unique<HttpTileSource>(
            id, QStringLiteral("https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png"), 17);
    } else if (id == QLatin1String("osm")) {
        online_ = std::make_unique<HttpTileSource>(
            id, QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png"), 19);
    }
    if (online_) {
        connect(online_.get(), &HttpTileSource::tileReady, this, [this] { update(); });
    }
#endif
    emit basemapChanged();
    bumpEpoch();
}

QString TileMapItem::attribution() const {
    if (basemap_ && basemap_->isOpen()) {
        const QString a = basemap_->attribution();
        return a.isEmpty() ? QStringLiteral("MBTiles basemap") : a;
    }
#ifndef TROPOLINK_AIRGAP
    if (onlineSourceId_ == QLatin1String("opentopomap")) {
        return QStringLiteral(
            "\xC2\xA9 OpenStreetMap contributors, SRTM | style \xC2\xA9 OpenTopoMap (CC-BY-SA)");
    }
    if (onlineSourceId_ == QLatin1String("osm")) {
        return QStringLiteral("\xC2\xA9 OpenStreetMap contributors");
    }
#endif
    return QStringLiteral("TropoLink DEM relief");
}

QPointF TileMapItem::fromCoordinate(double lat, double lon) const {
    const double scale = kTileSize * std::pow(2.0, zoom_);
    const double cx = lonToWorldX(centerLon_) * scale;
    const double cy = latToWorldY(centerLat_) * scale;
    const double px = lonToWorldX(lon) * scale;
    const double py = latToWorldY(lat) * scale;
    return {width() / 2.0 + (px - cx), height() / 2.0 + (py - cy)};
}

QVariantMap TileMapItem::toCoordinate(QPointF point) const {
    const double scale = kTileSize * std::pow(2.0, zoom_);
    const double cx = lonToWorldX(centerLon_) * scale;
    const double cy = latToWorldY(centerLat_) * scale;
    const double wx = (cx + (point.x() - width() / 2.0)) / scale;
    const double wy = (cy + (point.y() - height() / 2.0)) / scale;
    return QVariantMap{{"lat", worldYToLat(std::clamp(wy, 0.0, 1.0))},
                       {"lon", std::fmod(wx, 1.0) * 360.0 - 180.0}};
}

void TileMapItem::pan(double dxPixels, double dyPixels) {
    const double scale = kTileSize * std::pow(2.0, zoom_);
    const double cy = latToWorldY(centerLat_) - dyPixels / scale;
    centerLat_ = std::clamp(worldYToLat(std::clamp(cy, 0.001, 0.999)), -85.0, 85.0);
    centerLon_ = std::clamp(centerLon_ - dxPixels * 360.0 / scale, -180.0, 180.0);
    emit viewChanged();
    update();
}

void TileMapItem::zoomAround(QPointF pivot, double zoomDelta) {
    const auto before = toCoordinate(pivot);
    zoom_ = std::clamp(zoom_ + zoomDelta, kMinZoom, kMaxZoom);
    const auto after = toCoordinate(pivot);
    centerLat_ = std::clamp(centerLat_ + before["lat"].toDouble() - after["lat"].toDouble(), -85.0, 85.0);
    centerLon_ =
        std::clamp(centerLon_ + before["lon"].toDouble() - after["lon"].toDouble(), -180.0, 180.0);
    emit viewChanged();
    update();
}

void TileMapItem::centerOn(double lat, double lon, double zoomLevel) {
    centerLat_ = std::clamp(lat, -85.0, 85.0);
    centerLon_ = std::clamp(lon, -180.0, 180.0);
    if (zoomLevel > 0.0) {
        zoom_ = std::clamp(zoomLevel, kMinZoom, kMaxZoom);
    }
    emit viewChanged();
    update();
}

double TileMapItem::metersPerPixel() const {
    const double scale = kTileSize * std::pow(2.0, zoom_);
    return std::cos(centerLat_ * M_PI / 180.0) * 40075016.686 / scale;
}

void TileMapItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    emit viewChanged();
    update();
}

QSGNode* TileMapItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* root = static_cast<TileCacheNode*>(oldNode);
    if (root == nullptr) {
        root = new TileCacheNode;
    }
    // Rebuild the (cheap) tile nodes each frame; the textures are cached.
    while (root->firstChild() != nullptr) {
        auto* child = root->firstChild();
        root->removeChildNode(child);
        delete child;
    }
    if (root->epoch != epoch_) {
        qDeleteAll(root->textures);
        root->textures.clear();
        root->epoch = epoch_;
    }
    root->usedThisFrame.clear();
    if (width() <= 0 || height() <= 0 || window() == nullptr) {
        return root;
    }

    const int z = std::clamp(static_cast<int>(std::floor(zoom_)), static_cast<int>(kMinZoom),
                             static_cast<int>(kMaxZoom));
    const double frac = std::pow(2.0, zoom_ - z); // 1..2 fractional scale
    const double tilePixels = kTileSize * frac;
    const int worldTiles = 1 << z;

    const double cx = lonToWorldX(centerLon_) * worldTiles; // in tiles
    const double cy = latToWorldY(centerLat_) * worldTiles;

    const int tilesX = static_cast<int>(std::ceil(width() / tilePixels)) + 2;
    const int tilesY = static_cast<int>(std::ceil(height() / tilePixels)) + 2;
    const int startX = static_cast<int>(std::floor(cx - tilesX / 2.0));
    const int startY = static_cast<int>(std::floor(cy - tilesY / 2.0));

    // Resolve the image for a tile with layer fall-through. Returns the layer used
    // (for the texture key) or -1 when nothing is available yet.
    auto resolve = [this](const TileKey& key, QImage& image) -> int {
        if (basemap_ && basemap_->isOpen() && key.z >= basemap_->minZoom()) {
            if (key.z <= basemap_->maxZoom()) {
                image = basemap_->tile(key);
                if (!image.isNull() && image.width() > 1) {
                    return 0;
                }
                if (image.isNull()) {
                    return -1; // still decoding: wait rather than flash another layer
                }
                // width == 1: pack has no tile here — fall through.
            } else {
                // Over-zoom: crop the covering max-zoom tile so a z<=14 pack stays
                // usable when the operator zooms closer.
                const int dz = key.z - basemap_->maxZoom();
                if (dz <= 4) {
                    const TileKey parent{basemap_->maxZoom(), key.x >> dz, key.y >> dz};
                    const QImage parentImage = basemap_->tile(parent);
                    if (parentImage.isNull()) {
                        return -1;
                    }
                    if (parentImage.width() > 1) {
                        const int sub = 256 >> dz;
                        const int ox = (key.x & ((1 << dz) - 1)) * sub;
                        const int oy = (key.y & ((1 << dz) - 1)) * sub;
                        image = parentImage.copy(ox, oy, sub, sub)
                                    .scaled(256, 256, Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation);
                        return 0;
                    }
                }
            }
        }
        if (online_) {
            image = online_->tile(TileKey{key.z, key.x, key.y});
            if (!image.isNull()) {
                return 1;
            }
            // Loading (or absent in Air-Gap): fall through to the offline rendering.
        }
        if (topo_) {
            image = topo_->tile(key, darkTheme_);
            if (!image.isNull()) {
                return 2;
            }
        }
        return -1;
    };

    for (int ty = startY; ty < startY + tilesY; ++ty) {
        if (ty < 0 || ty >= worldTiles) {
            continue;
        }
        for (int tx = startX; tx < startX + tilesX; ++tx) {
            const int wrappedX = ((tx % worldTiles) + worldTiles) % worldTiles;
            const TileKey key{z, wrappedX, ty};

            QImage image;
            const int layer = resolve(key, image);
            if (layer < 0 || image.isNull() || image.width() <= 1) {
                continue;
            }
            const quint64 tk = texKey(layer, key);
            QSGTexture* texture = root->textures.value(tk, nullptr);
            if (texture == nullptr) {
                texture = window()->createTextureFromImage(image);
                if (texture == nullptr) {
                    continue;
                }
                root->textures.insert(tk, texture);
            }
            root->usedThisFrame.insert(tk);
            auto* node = new QSGSimpleTextureNode;
            node->setOwnsTexture(false); // cache owns
            node->setTexture(texture);
            node->setFiltering(QSGTexture::Linear);
            const double px = width() / 2.0 + (tx - cx) * tilePixels;
            const double py = height() / 2.0 + (ty - cy) * tilePixels;
            node->setRect(QRectF(px, py, tilePixels + 0.5, tilePixels + 0.5));
            root->appendChildNode(node);
        }
    }

    // Bounded cache: drop textures not used this frame once we hold too many.
    if (root->textures.size() > 512) {
        for (auto it = root->textures.begin(); it != root->textures.end();) {
            if (!root->usedThisFrame.contains(it.key())) {
                delete it.value();
                it = root->textures.erase(it);
            } else {
                ++it;
            }
        }
    }
    return root;
}
