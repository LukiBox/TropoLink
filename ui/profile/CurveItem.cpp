#include "ui/profile/CurveItem.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

CurveItem::CurveItem(QQuickItem* parent) : QQuickPaintedItem(parent) { setAntialiasing(true); }

void CurveItem::setPoints(const QVariantList& v) {
    points_ = v;
    emit dataChanged();
    update();
}
void CurveItem::setCurrentMargin(double v) {
    currentMargin_ = v;
    emit dataChanged();
    update();
}
void CurveItem::setTargetAvailability(double v) {
    targetAvailability_ = v;
    emit dataChanged();
    update();
}
void CurveItem::setDarkTheme(bool v) {
    darkTheme_ = v;
    emit dataChanged();
    update();
}

void CurveItem::paint(QPainter* p) {
    if (points_.size() < 2 || width() < 40 || height() < 40) {
        return;
    }
    const double left = 46.0;
    const double bottom = height() - 20.0;
    const double top = 8.0;
    const double right = width() - 10.0;

    // Availability axis is displayed as "nines": y = -log10(100 - A).
    auto nines = [](double availability) {
        return -std::log10(std::clamp(100.0 - availability, 1e-5, 100.0));
    };
    double mMin = 1e9;
    double mMax = -1e9;
    for (const auto& v : points_) {
        const double m = v.toMap().value("margin").toDouble();
        mMin = std::min(mMin, m);
        mMax = std::max(mMax, m);
    }
    const double nMin = -2.0; // 0% .. handled by clamp
    const double nMax = 4.2;  // 99.99%+

    auto xPix = [&](double m) { return left + (m - mMin) / (mMax - mMin) * (right - left); };
    auto yPix = [&](double a) {
        return bottom - (std::clamp(nines(a), nMin, nMax) - nMin) / (nMax - nMin) * (bottom - top);
    };

    const QColor axis = darkTheme_ ? QColor(150, 150, 150) : QColor(90, 90, 90);
    const QColor curve = darkTheme_ ? QColor(90, 170, 255) : QColor(30, 90, 190);
    const QColor marker = darkTheme_ ? QColor(120, 220, 140) : QColor(30, 140, 60);
    const QColor targetC = darkTheme_ ? QColor(255, 170, 60) : QColor(200, 90, 0);

    p->setPen(QPen(axis, 1.0));
    p->drawLine(QPointF(left, top), QPointF(left, bottom));
    p->drawLine(QPointF(left, bottom), QPointF(right, bottom));
    QFont f = p->font();
    f.setPointSizeF(7.5);
    p->setFont(f);
    for (const double a : {90.0, 99.0, 99.9, 99.99}) {
        const double y = yPix(a);
        p->setPen(QPen(QColor(axis.red(), axis.green(), axis.blue(), 60), 0.8, Qt::DotLine));
        p->drawLine(QPointF(left, y), QPointF(right, y));
        p->setPen(axis);
        p->drawText(QRectF(0, y - 7, left - 4, 14), Qt::AlignRight | Qt::AlignVCenter,
                    QString::number(a));
    }
    for (double m = std::ceil(mMin / 10.0) * 10.0; m <= mMax; m += 10.0) {
        p->setPen(axis);
        p->drawText(QRectF(xPix(m) - 20, bottom + 2, 40, 14), Qt::AlignCenter,
                    QStringLiteral("%1").arg(m));
    }

    QPainterPath path;
    bool started = false;
    for (const auto& v : points_) {
        const auto m = v.toMap();
        const QPointF pt(xPix(m.value("margin").toDouble()), yPix(m.value("availability").toDouble()));
        if (!started) {
            path.moveTo(pt);
            started = true;
        } else {
            path.lineTo(pt);
        }
    }
    p->setPen(QPen(curve, 2.0));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);

    // Target availability line and current-margin marker.
    p->setPen(QPen(targetC, 1.2, Qt::DashLine));
    p->drawLine(QPointF(left, yPix(targetAvailability_)), QPointF(right, yPix(targetAvailability_)));
    if (currentMargin_ >= mMin && currentMargin_ <= mMax) {
        p->setPen(QPen(marker, 1.4));
        p->drawLine(QPointF(xPix(currentMargin_), top), QPointF(xPix(currentMargin_), bottom));
    }
}
