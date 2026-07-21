#pragma once

// GPU tile map: a QQuickItem rendering web-mercator tiles as scene-graph texture
// nodes. Layer priority per tile: imported MBTiles basemap pack, then the optional
// online source (standard flavor), then the offline paper-topo rendering from the
// loaded DEMs — so something meaningful is always on screen, fully offline.
//
// Textures are cached per tile inside the scene-graph root node (created once,
// reused every frame); the cache is epoch-tagged and dropped wholesale when a
// source, the theme, or the terrain store changes.

#include "ui/map/MapSources.h"
#include "ui/models/AppController.h" // complete type: moc force-completes property types

#include <QImage>
#include <QQuickItem>

#include <memory>

class TileMapItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double centerLat READ centerLat WRITE setCenterLat NOTIFY viewChanged)
    Q_PROPERTY(double centerLon READ centerLon WRITE setCenterLon NOTIFY viewChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(AppController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY viewChanged)
    Q_PROPERTY(QString basemapPath READ basemapPath WRITE setBasemapPath NOTIFY basemapChanged)
    Q_PROPERTY(bool hasBasemap READ hasBasemap NOTIFY basemapChanged)
    // "" (offline paper-topo only), "opentopomap" or "osm".
    Q_PROPERTY(QString onlineSource READ onlineSource WRITE setOnlineSource NOTIFY basemapChanged)
    Q_PROPERTY(QString attribution READ attribution NOTIFY basemapChanged)
    QML_ELEMENT

  public:
    explicit TileMapItem(QQuickItem* parent = nullptr);

    double centerLat() const { return centerLat_; }
    double centerLon() const { return centerLon_; }
    double zoom() const { return zoom_; }
    void setCenterLat(double v);
    void setCenterLon(double v);
    void setZoom(double v);
    AppController* controller() const { return controller_; }
    void setController(AppController* c);
    bool darkTheme() const { return darkTheme_; }
    void setDarkTheme(bool v);
    QString basemapPath() const { return basemapPath_; }
    void setBasemapPath(const QString& path);
    bool hasBasemap() const { return basemap_ && basemap_->isOpen(); }
    QString onlineSource() const { return onlineSourceId_; }
    void setOnlineSource(const QString& id);
    QString attribution() const;

    // Coordinate <-> item pixel mapping for QML overlays.
    Q_INVOKABLE QPointF fromCoordinate(double lat, double lon) const;
    Q_INVOKABLE QVariantMap toCoordinate(QPointF point) const;
    Q_INVOKABLE void pan(double dxPixels, double dyPixels);
    Q_INVOKABLE void zoomAround(QPointF pivot, double zoomDelta);
    Q_INVOKABLE void centerOn(double lat, double lon, double zoomLevel = -1.0);
    Q_INVOKABLE double metersPerPixel() const;

  signals:
    void viewChanged();
    void controllerChanged();
    void basemapChanged();

  protected:
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

  private:
    static constexpr double kMinZoom = 3.0;
    static constexpr double kMaxZoom = 17.0;

    void bumpEpoch(); // drop cached textures on next frame

    double centerLat_ = 51.97;
    double centerLon_ = 15.27;
    double zoom_ = 8.0;
    bool darkTheme_ = true;
    AppController* controller_ = nullptr;
    QString basemapPath_;
    QString onlineSourceId_;
    quint64 epoch_ = 0;
    std::unique_ptr<MBTilesSource> basemap_;
    std::unique_ptr<TopoTileSource> topo_;
    std::unique_ptr<HttpTileSource> online_;
};
