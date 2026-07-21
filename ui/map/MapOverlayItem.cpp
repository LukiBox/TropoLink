#include "ui/map/MapOverlayItem.h"

#include "ui/map/TileMapItem.h"
#include "ui/models/AppController.h"

#include <GeographicLib/UTMUPS.hpp>

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {
QColor accent(bool dark) {
    return dark ? QColor(255, 170, 60) : QColor(200, 90, 0);
}
QColor gridColor(bool dark) {
    return dark ? QColor(120, 160, 200, 110) : QColor(60, 90, 140, 110);
}
} // namespace

MapOverlayItem::MapOverlayItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}

void MapOverlayItem::setMap(TileMapItem* m) {
    if (map_ == m) {
        return;
    }
    if (map_ != nullptr) {
        disconnect(map_, nullptr, this, nullptr);
    }
    map_ = m;
    if (map_ != nullptr) {
        connect(map_, &TileMapItem::viewChanged, this, [this] { update(); });
    }
    emit attachedChanged();
    update();
}

void MapOverlayItem::setController(AppController* c) {
    if (controller_ == c) {
        return;
    }
    if (controller_ != nullptr) {
        disconnect(controller_, nullptr, this, nullptr);
    }
    controller_ = c;
    if (controller_ != nullptr) {
        connect(controller_, &AppController::resultsChanged, this, [this] { update(); });
    }
    emit attachedChanged();
    update();
}

void MapOverlayItem::setShowMgrsGrid(bool v) {
    showMgrsGrid_ = v;
    emit optionsChanged();
    update();
}
void MapOverlayItem::setDarkTheme(bool v) {
    darkTheme_ = v;
    emit optionsChanged();
    update();
}
void MapOverlayItem::setHoverDistanceM(double v) {
    hoverDistanceM_ = v;
    emit optionsChanged();
    update();
}
void MapOverlayItem::setMeasurePoints(const QVariantList& v) {
    measurePoints_ = v;
    emit optionsChanged();
    update();
}

void MapOverlayItem::paint(QPainter* painter) {
    if (map_ == nullptr || controller_ == nullptr) {
        return;
    }
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (showMgrsGrid_) {
        paintMgrsGrid(painter);
    }

    auto toPoint = [this](const QVariant& v) {
        const auto m = v.toMap();
        return map_->fromCoordinate(m.value("lat").toDouble(), m.value("lon").toDouble());
    };

    // Radio-horizon fans.
    for (const auto& fan : {controller_->horizonFanA(), controller_->horizonFanB()}) {
        if (fan.size() < 3) {
            continue;
        }
        QPainterPath path;
        path.moveTo(toPoint(fan.first()));
        for (int i = 1; i < fan.size(); ++i) {
            path.lineTo(toPoint(fan.at(i)));
        }
        path.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(80, 160, 255, darkTheme_ ? 34 : 48));
        painter->drawPath(path);
    }

    // Geodesic link line.
    const auto polyline = controller_->pathPolyline();
    if (polyline.size() >= 2) {
        QPainterPath path;
        path.moveTo(toPoint(polyline.first()));
        for (int i = 1; i < polyline.size(); ++i) {
            path.lineTo(toPoint(polyline.at(i)));
        }
        QPen pen(accent(darkTheme_), 2.5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }

    // Common volume marker (diamond) at its true map position.
    {
        const auto cv = controller_->commonVolumeMap();
        if (cv.contains("lat")) {
            const QPointF p = map_->fromCoordinate(cv["lat"].toDouble(), cv["lon"].toDouble());
            QPainterPath diamond;
            const double r = 7.0;
            diamond.moveTo(p + QPointF(0, -r));
            diamond.lineTo(p + QPointF(r, 0));
            diamond.lineTo(p + QPointF(0, r));
            diamond.lineTo(p + QPointF(-r, 0));
            diamond.closeSubpath();
            painter->setPen(QPen(darkTheme_ ? Qt::white : Qt::black, 1.2));
            painter->setBrush(QColor(120, 220, 140, 220));
            painter->drawPath(diamond);
        }
    }

    // Profile-hover marker on the path.
    if (hoverDistanceM_ >= 0.0 && polyline.size() >= 2) {
        const double total = controller_->geometry().value("distanceKm").toDouble() * 1000.0;
        if (total > 0.0) {
            const double f = std::clamp(hoverDistanceM_ / total, 0.0, 1.0);
            const int idx = static_cast<int>(f * (polyline.size() - 1));
            const QPointF p = toPoint(polyline.at(idx));
            painter->setPen(QPen(QColor(120, 220, 140), 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(p, 6.0, 6.0);
        }
    }

    // Measurement line.
    if (measurePoints_.size() == 2) {
        const QPointF a = toPoint(measurePoints_.at(0));
        const QPointF b = toPoint(measurePoints_.at(1));
        QPen pen(QColor(230, 90, 90), 2.0, Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(a, b);
        painter->setBrush(QColor(230, 90, 90));
        painter->drawEllipse(a, 3.5, 3.5);
        painter->drawEllipse(b, 3.5, 3.5);
    }
}

void MapOverlayItem::paintMgrsGrid(QPainter* painter) {
    // Grid-zone designators (6 x 8 degrees) always; 100 km squares when zoomed in.
    const auto topLeft = map_->toCoordinate(QPointF(0, 0));
    const auto bottomRight = map_->toCoordinate(QPointF(width(), height()));
    const double latMax = topLeft["lat"].toDouble();
    const double latMin = bottomRight["lat"].toDouble();
    const double lonMin = topLeft["lon"].toDouble();
    const double lonMax = bottomRight["lon"].toDouble();

    QPen gzdPen(gridColor(darkTheme_), 1.4);
    painter->setPen(gzdPen);
    QFont font = painter->font();
    font.setPointSizeF(9.0);
    font.setBold(true);
    painter->setFont(font);

    for (double lon = std::floor(lonMin / 6.0) * 6.0; lon <= lonMax + 6.0; lon += 6.0) {
        const QPointF a = map_->fromCoordinate(std::clamp(latMin, -80.0, 84.0), lon);
        const QPointF b = map_->fromCoordinate(std::clamp(latMax, -80.0, 84.0), lon);
        painter->drawLine(a, b);
    }
    for (double lat = std::floor(latMin / 8.0) * 8.0; lat <= latMax + 8.0; lat += 8.0) {
        if (lat < -80.0 || lat > 84.0) {
            continue;
        }
        painter->drawLine(map_->fromCoordinate(lat, lonMin), map_->fromCoordinate(lat, lonMax));
    }
    // GZD labels.
    static const char bands[] = "CDEFGHJKLMNPQRSTUVWX";
    for (double lon = std::floor(lonMin / 6.0) * 6.0; lon < lonMax; lon += 6.0) {
        for (double lat = std::floor(latMin / 8.0) * 8.0; lat < latMax; lat += 8.0) {
            if (lat < -80.0 || lat >= 84.0) {
                continue;
            }
            const int zone = static_cast<int>(std::floor((lon + 180.0) / 6.0)) % 60 + 1;
            const int bandIdx = std::clamp(static_cast<int>((lat + 80.0) / 8.0), 0, 19);
            const QPointF p = map_->fromCoordinate(lat + 4.0, lon + 3.0);
            painter->drawText(QRectF(p.x() - 30, p.y() - 10, 60, 20), Qt::AlignCenter,
                              QStringLiteral("%1%2").arg(zone).arg(QLatin1Char(bands[bandIdx])));
        }
    }

    // 100 km squares once a pixel is finer than ~150 m.
    if (map_->metersPerPixel() > 150.0) {
        return;
    }
    QPen sqPen(gridColor(darkTheme_), 0.8);
    painter->setPen(sqPen);
    const double centerLon = (lonMin + lonMax) / 2.0;
    const double centerLat = (latMin + latMax) / 2.0;
    try {
        int zone = 0;
        bool northp = true;
        double cx = 0.0;
        double cy = 0.0;
        GeographicLib::UTMUPS::Forward(centerLat, centerLon, zone, northp, cx, cy);
        const double e0 = std::floor((cx - 400000.0) / 100000.0) * 100000.0 + 400000.0 - 400000.0;
        const double n0 = std::floor((cy - 400000.0) / 100000.0) * 100000.0;
        for (int i = -5; i <= 6; ++i) {
            // North-south lines (constant easting), drawn as short polylines.
            QPainterPath path;
            bool started = false;
            for (int j = -5; j <= 6; ++j) {
                double lat = 0.0;
                double lon = 0.0;
                try {
                    GeographicLib::UTMUPS::Reverse(zone, northp, e0 + i * 100000.0 + 400000.0,
                                                   n0 + j * 100000.0, lat, lon);
                } catch (const std::exception&) {
                    continue;
                }
                const QPointF p = map_->fromCoordinate(lat, lon);
                if (!started) {
                    path.moveTo(p);
                    started = true;
                } else {
                    path.lineTo(p);
                }
            }
            painter->drawPath(path);
            // East-west lines (constant northing).
            QPainterPath path2;
            started = false;
            for (int j = -5; j <= 6; ++j) {
                double lat = 0.0;
                double lon = 0.0;
                try {
                    GeographicLib::UTMUPS::Reverse(zone, northp, e0 + j * 100000.0 + 400000.0,
                                                   n0 + i * 100000.0, lat, lon);
                } catch (const std::exception&) {
                    continue;
                }
                const QPointF p = map_->fromCoordinate(lat, lon);
                if (!started) {
                    path2.moveTo(p);
                    started = true;
                } else {
                    path2.lineTo(p);
                }
            }
            painter->drawPath(path2);
        }
    } catch (const std::exception&) {
        // Out of UTM coverage: skip the fine grid.
    }
}
