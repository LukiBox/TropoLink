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
} // namespace

TileMapItem::TileMapItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::NoButton); // interaction handled by QML handlers
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
    zoom_ = std::clamp(v, 3.0, 15.0);
    emit viewChanged();
    update();
}

void TileMapItem::setController(AppController* c) {
    if (controller_ == c) {
        return;
    }
    controller_ = c;
    hillshade_.reset();
    if (controller_ != nullptr && controller_->terrainStore() != nullptr) {
        hillshade_ = std::make_unique<HillshadeSource>(controller_->terrainStore());
        connect(hillshade_.get(), &HillshadeSource::tileReady, this, [this] { update(); });
        connect(controller_, &AppController::terrainChanged, this, [this] {
            if (hillshade_) {
                hillshade_->invalidate();
            }
            update();
        });
    }
    emit controllerChanged();
    update();
}

void TileMapItem::setDarkTheme(bool v) {
    if (darkTheme_ == v) {
        return;
    }
    darkTheme_ = v;
    emit viewChanged();
    update();
}

void TileMapItem::setBasemapPath(const QString& path) {
    basemapPath_ = path;
    if (path.isEmpty()) {
        basemap_.reset();
    } else {
        basemap_ = std::make_unique<MBTilesSource>();
        if (!basemap_->open(path)) {
            basemap_.reset();
        } else {
            connect(basemap_.get(), &MBTilesSource::tileReady, this, [this] { update(); });
        }
    }
    emit basemapChanged();
    update();
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
    zoom_ = std::clamp(zoom_ + zoomDelta, 3.0, 15.0);
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
        zoom_ = std::clamp(zoomLevel, 3.0, 15.0);
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
    auto* root = oldNode;
    if (root == nullptr) {
        root = new QSGNode;
    }
    // Rebuild the tile nodes each frame; textures come from the sources' caches.
    while (root->firstChild() != nullptr) {
        auto* child = root->firstChild();
        root->removeChildNode(child);
        delete child;
    }
    if (width() <= 0 || height() <= 0 || window() == nullptr) {
        return root;
    }

    const int z = std::clamp(static_cast<int>(std::floor(zoom_)), 3, 15);
    const double frac = std::pow(2.0, zoom_ - z); // 1..2 fractional scale
    const double tilePixels = kTileSize * frac;
    const int worldTiles = 1 << z;

    const double cx = lonToWorldX(centerLon_) * worldTiles; // in tiles
    const double cy = latToWorldY(centerLat_) * worldTiles;

    const int tilesX = static_cast<int>(std::ceil(width() / tilePixels)) + 2;
    const int tilesY = static_cast<int>(std::ceil(height() / tilePixels)) + 2;
    const int startX = static_cast<int>(std::floor(cx - tilesX / 2.0));
    const int startY = static_cast<int>(std::floor(cy - tilesY / 2.0));

    for (int ty = startY; ty < startY + tilesY; ++ty) {
        if (ty < 0 || ty >= worldTiles) {
            continue;
        }
        for (int tx = startX; tx < startX + tilesX; ++tx) {
            const int wrappedX = ((tx % worldTiles) + worldTiles) % worldTiles;
            const TileKey key{z, wrappedX, ty};

            QImage image;
            if (basemap_ && basemap_->isOpen() && z >= basemap_->minZoom() && z <= basemap_->maxZoom()) {
                image = basemap_->tile(key);
            }
            if (image.isNull() && hillshade_) {
                image = hillshade_->tile(key, darkTheme_);
            }
            if (image.isNull() || image.width() <= 1) {
                continue;
            }
            auto* node = new QSGSimpleTextureNode;
            QSGTexture* texture = window()->createTextureFromImage(image);
            node->setOwnsTexture(true);
            node->setTexture(texture);
            node->setFiltering(QSGTexture::Linear);
            const double px = width() / 2.0 + (tx - cx) * tilePixels;
            const double py = height() / 2.0 + (ty - cy) * tilePixels;
            node->setRect(QRectF(px, py, tilePixels + 0.5, tilePixels + 0.5));
            root->appendChildNode(node);
        }
    }
    return root;
}
