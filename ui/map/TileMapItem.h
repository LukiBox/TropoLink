#pragma once

// GPU tile map: a QQuickItem rendering web-mercator tiles as scene-graph texture
// nodes. Sources: imported MBTiles basemap packs, with a hillshade layer rendered
// from the loaded DEMs (visible even with no basemap). Fully offline.

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
    // World coordinates: mercator [0..1] at the current fractional zoom.
    double worldX(double lon) const;
    double worldY(double lat) const;

    double centerLat_ = 51.97;
    double centerLon_ = 15.27;
    double zoom_ = 8.0;
    bool darkTheme_ = true;
    AppController* controller_ = nullptr;
    QString basemapPath_;
    std::unique_ptr<MBTilesSource> basemap_;
    std::unique_ptr<HillshadeSource> hillshade_;
};
