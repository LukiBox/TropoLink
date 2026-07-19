#pragma once

// Vector overlay above the tile map: geodesic link line, radio-horizon fans, the
// common-volume marker at its true position, MGRS grid, measurement line and the
// profile-hover marker. Painted (antialiased) into a GPU-composited layer;
// repainted on view or result changes only.

#include "ui/map/TileMapItem.h" // complete types: moc force-completes property types
#include "ui/models/AppController.h"

#include <QQuickPaintedItem>
#include <QVariantList>

class MapOverlayItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(TileMapItem* map READ map WRITE setMap NOTIFY attachedChanged)
    Q_PROPERTY(AppController* controller READ controller WRITE setController NOTIFY attachedChanged)
    Q_PROPERTY(bool showMgrsGrid READ showMgrsGrid WRITE setShowMgrsGrid NOTIFY optionsChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY optionsChanged)
    Q_PROPERTY(double hoverDistanceM READ hoverDistanceM WRITE setHoverDistanceM NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList measurePoints READ measurePoints WRITE setMeasurePoints NOTIFY optionsChanged)
    QML_ELEMENT

public:
    explicit MapOverlayItem(QQuickItem* parent = nullptr);

    TileMapItem* map() const { return map_; }
    void setMap(TileMapItem* m);
    AppController* controller() const { return controller_; }
    void setController(AppController* c);
    bool showMgrsGrid() const { return showMgrsGrid_; }
    void setShowMgrsGrid(bool v);
    bool darkTheme() const { return darkTheme_; }
    void setDarkTheme(bool v);
    double hoverDistanceM() const { return hoverDistanceM_; }
    void setHoverDistanceM(double v);
    QVariantList measurePoints() const { return measurePoints_; }
    void setMeasurePoints(const QVariantList& v);

    void paint(QPainter* painter) override;

signals:
    void attachedChanged();
    void optionsChanged();

private:
    void paintMgrsGrid(QPainter* painter);

    TileMapItem* map_ = nullptr;
    AppController* controller_ = nullptr;
    bool showMgrsGrid_ = true;
    bool darkTheme_ = true;
    double hoverDistanceM_ = -1.0;
    QVariantList measurePoints_;
};
