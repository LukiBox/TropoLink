#include "ui/profile/ProfileItem.h"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>

#include <algorithm>
#include <cmath>

namespace {

QSGGeometryNode* makeNode(const QColor& color, QSGGeometry* geometry) {
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

// Direct3D ignores glLineWidth-style widths: build thick polylines as triangle
// strips (a ribbon of quads along the segment normals).
QSGGeometry* lineStrip(const QVector<QPointF>& points, float widthPx) {
    if (widthPx <= 1.2f || points.size() < 2) {
        auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), points.size());
        g->setDrawingMode(QSGGeometry::DrawLineStrip);
        g->setLineWidth(1.0f);
        auto* v = g->vertexDataAsPoint2D();
        for (int i = 0; i < points.size(); ++i) {
            v[i].set(static_cast<float>(points[i].x()), static_cast<float>(points[i].y()));
        }
        return g;
    }
    auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), points.size() * 2);
    g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    auto* v = g->vertexDataAsPoint2D();
    const float half = widthPx * 0.5f;
    for (int i = 0; i < points.size(); ++i) {
        // Normal from the average of adjacent segment directions.
        const QPointF prev = points[std::max(0, i - 1)];
        const QPointF next = points[std::min(static_cast<int>(points.size()) - 1, i + 1)];
        double dx = next.x() - prev.x();
        double dy = next.y() - prev.y();
        const double len = std::hypot(dx, dy);
        if (len > 1e-9) {
            dx /= len;
            dy /= len;
        } else {
            dx = 1.0;
            dy = 0.0;
        }
        const auto nx = static_cast<float>(-dy * half);
        const auto ny = static_cast<float>(dx * half);
        v[i * 2].set(static_cast<float>(points[i].x()) + nx, static_cast<float>(points[i].y()) + ny);
        v[i * 2 + 1].set(static_cast<float>(points[i].x()) - nx, static_cast<float>(points[i].y()) - ny);
    }
    return g;
}

} // namespace

ProfileItem::ProfileItem(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

#define TL_SETTER(name, member)                                                                            \
    void ProfileItem::name(const QPolygonF& v) {                                                           \
        member = v;                                                                                        \
        computeViewport();                                                                                 \
        emit dataChanged();                                                                                \
        update();                                                                                          \
    }
TL_SETTER(setTerrain, terrain_)
TL_SETTER(setRayA, rayA_)
TL_SETTER(setRayB, rayB_)
TL_SETTER(setLens, lens_)
TL_SETTER(setDirectRay, directRay_)
TL_SETTER(setFresnelLower, fresnelLower_)
TL_SETTER(setFresnelUpper, fresnelUpper_)
#undef TL_SETTER

void ProfileItem::setVoidSpans(const QVariantList& v) {
    voidSpans_ = v;
    emit dataChanged();
    update();
}

void ProfileItem::setMeta(const QVariantMap& v) {
    meta_ = v;
    computeViewport();
    emit dataChanged();
    update();
}

void ProfileItem::setDarkTheme(bool v) {
    darkTheme_ = v;
    emit dataChanged();
    update();
}

void ProfileItem::computeViewport() {
    if (meta_.isEmpty()) {
        return;
    }
    x0_ = 0.0;
    x1_ = std::max(1.0, meta_.value("distanceM").toDouble());
    const double minY = meta_.value("minY").toDouble();
    const double maxY = std::max({meta_.value("maxY").toDouble(), meta_.value("lensTopY").toDouble()});
    const double span = std::max(50.0, maxY - minY);
    y0_ = minY - span * 0.06;
    y1_ = maxY + span * 0.10;
}

double ProfileItem::xToPixel(double distanceM) const {
    const double w = width() - kMarginLeft - kMarginRight;
    return kMarginLeft + (distanceM - x0_) / (x1_ - x0_) * w;
}

double ProfileItem::yToPixel(double elevationM) const {
    const double h = height() - kMarginTop - kMarginBottom;
    return kMarginTop + (1.0 - (elevationM - y0_) / (y1_ - y0_)) * h;
}

double ProfileItem::pixelToDistance(double px) const {
    const double w = width() - kMarginLeft - kMarginRight;
    return x0_ + std::clamp((px - kMarginLeft) / std::max(1.0, w), 0.0, 1.0) * (x1_ - x0_);
}

QVariantList ProfileItem::elevationTicks() const {
    QVariantList ticks;
    const double span = y1_ - y0_;
    const double step = span > 2500 ? 500.0 : span > 1200 ? 250.0 : span > 500 ? 100.0 : 50.0;
    for (double v = std::ceil(y0_ / step) * step; v <= y1_; v += step) {
        ticks << QVariantMap{{"value", v}, {"pixel", yToPixel(v)}};
    }
    return ticks;
}

QVariantList ProfileItem::distanceTicks() const {
    QVariantList ticks;
    const double spanKm = (x1_ - x0_) / 1000.0;
    const double stepKm = spanKm > 250 ? 50.0 : spanKm > 100 ? 20.0 : spanKm > 40 ? 10.0 : 5.0;
    for (double km = 0.0; km <= spanKm + 0.001; km += stepKm) {
        ticks << QVariantMap{{"value", km}, {"pixel", xToPixel(km * 1000.0)}};
    }
    return ticks;
}

void ProfileItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    emit dataChanged(); // labels reposition
    update();
}

QSGNode* ProfileItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* root = oldNode != nullptr ? oldNode : new QSGNode;
    while (root->firstChild() != nullptr) {
        auto* child = root->firstChild();
        root->removeChildNode(child);
        delete child;
    }
    if (terrain_.size() < 2 || width() <= 10 || height() <= 10) {
        return root;
    }

    auto px = [this](const QPointF& p) { return QPointF(xToPixel(p.x()), yToPixel(p.y())); };

    const QColor terrainFill = darkTheme_ ? QColor(58, 72, 58) : QColor(196, 208, 186);
    const QColor terrainLine = darkTheme_ ? QColor(140, 168, 140) : QColor(96, 120, 88);
    const QColor rayColor = darkTheme_ ? QColor(90, 170, 255) : QColor(30, 90, 190);
    const QColor lensFill = darkTheme_ ? QColor(120, 220, 140, 70) : QColor(30, 140, 60, 60);
    const QColor lensEdge = darkTheme_ ? QColor(120, 220, 140, 180) : QColor(30, 140, 60, 170);
    const QColor directColor = darkTheme_ ? QColor(235, 90, 90) : QColor(190, 40, 40);
    const QColor fresnelColor = darkTheme_ ? QColor(235, 90, 90, 110) : QColor(190, 40, 40, 110);
    const QColor mastColor = darkTheme_ ? QColor(240, 240, 240) : QColor(30, 30, 30);
    const QColor voidColor = QColor(255, 140, 0);

    // Terrain fill: triangle strip terrain point -> bottom baseline.
    {
        auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), terrain_.size() * 2);
        g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        auto* v = g->vertexDataAsPoint2D();
        const float bottom = static_cast<float>(height() - kMarginBottom);
        for (int i = 0; i < terrain_.size(); ++i) {
            const QPointF p = px(terrain_[i]);
            v[i * 2].set(static_cast<float>(p.x()), static_cast<float>(p.y()));
            v[i * 2 + 1].set(static_cast<float>(p.x()), bottom);
        }
        root->appendChildNode(makeNode(terrainFill, g));
    }
    // Terrain outline.
    {
        QVector<QPointF> pts;
        pts.reserve(terrain_.size());
        for (const auto& p : terrain_) {
            pts << px(p);
        }
        root->appendChildNode(makeNode(terrainLine, lineStrip(pts, 1.6f)));
    }
    // Void spans: markers along the bottom.
    for (const auto& spanV : voidSpans_) {
        const auto span = spanV.toMap();
        const double from = span.value("from").toDouble();
        const double to = span.value("to").toDouble();
        QVector<QPointF> pts{QPointF(xToPixel(from), height() - kMarginBottom + 5.0),
                             QPointF(xToPixel(to), height() - kMarginBottom + 5.0)};
        root->appendChildNode(makeNode(voidColor, lineStrip(pts, 3.5f)));
    }
    // Common-volume lens (drawn under the rays): strip between lower/upper edges.
    if (lens_.size() >= 4) {
        const int n = lens_.size() / 2;
        auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), n * 2);
        g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        auto* v = g->vertexDataAsPoint2D();
        for (int i = 0; i < n; ++i) {
            const QPointF lo = px(lens_[i]);
            const QPointF hi = px(lens_[lens_.size() - 1 - i]);
            v[i * 2].set(static_cast<float>(lo.x()), static_cast<float>(lo.y()));
            v[i * 2 + 1].set(static_cast<float>(hi.x()), static_cast<float>(hi.y()));
        }
        root->appendChildNode(makeNode(lensFill, g));
        // Lens outline.
        QVector<QPointF> outline;
        outline.reserve(lens_.size() + 1);
        for (const auto& p : lens_) {
            outline << px(p);
        }
        outline << px(lens_.first());
        root->appendChildNode(makeNode(lensEdge, lineStrip(outline, 1.4f)));
    }
    // Horizon rays.
    for (const auto* ray : {&rayA_, &rayB_}) {
        if (ray->size() >= 2) {
            QVector<QPointF> pts;
            for (const auto& p : *ray) {
                pts << px(p);
            }
            root->appendChildNode(makeNode(rayColor, lineStrip(pts, 1.8f)));
        }
    }
    // Direct ray and Fresnel envelope (why this must be troposcatter).
    if (directRay_.size() >= 2) {
        QVector<QPointF> pts;
        for (const auto& p : directRay_) {
            pts << px(p);
        }
        root->appendChildNode(makeNode(directColor, lineStrip(pts, 1.4f)));
    }
    for (const auto* env : {&fresnelLower_, &fresnelUpper_}) {
        if (env->size() >= 2) {
            QVector<QPointF> pts;
            for (const auto& p : *env) {
                pts << px(p);
            }
            root->appendChildNode(makeNode(fresnelColor, lineStrip(pts, 1.0f)));
        }
    }
    // Masts, drawn to scale at their AGL heights.
    {
        const double dist = meta_.value("distanceM").toDouble();
        const double hA = meta_.value("antennaA").toDouble();
        const double hB = meta_.value("antennaB").toDouble();
        const double aglA = meta_.value("aglA").toDouble();
        const double aglB = meta_.value("aglB").toDouble();
        QVector<QPointF> mastA{QPointF(xToPixel(0.0), yToPixel(hA - aglA)),
                               QPointF(xToPixel(0.0), yToPixel(hA))};
        QVector<QPointF> mastB{QPointF(xToPixel(dist), yToPixel(hB - aglB)),
                               QPointF(xToPixel(dist), yToPixel(hB))};
        root->appendChildNode(makeNode(mastColor, lineStrip(mastA, 3.0f)));
        root->appendChildNode(makeNode(mastColor, lineStrip(mastB, 3.0f)));
    }
    return root;
}
