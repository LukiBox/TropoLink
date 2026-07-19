#pragma once

// The signature visualization: a GPU scene-graph item showing the vertical slice of
// the link — terrain with effective-earth curvature, masts, horizon rays, the shaded
// common-volume lens, and the first Fresnel zone of the (blocked) direct path.
//
// All input polygons are in display space (metres along path, metres AMSL plus the
// effective-earth bulge), in which the horizon rays are exactly straight lines.
// Text labels are QML overlays positioned through xToPixel/yToPixel.

#include <QPolygonF>
#include <QQuickItem>
#include <QVariantList>

class ProfileItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QPolygonF terrain READ terrain WRITE setTerrain NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF rayA READ rayA WRITE setRayA NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF rayB READ rayB WRITE setRayB NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF lens READ lens WRITE setLens NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF directRay READ directRay WRITE setDirectRay NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF fresnelLower READ fresnelLower WRITE setFresnelLower NOTIFY dataChanged)
    Q_PROPERTY(QPolygonF fresnelUpper READ fresnelUpper WRITE setFresnelUpper NOTIFY dataChanged)
    Q_PROPERTY(QVariantList voidSpans READ voidSpans WRITE setVoidSpans NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap meta READ meta WRITE setMeta NOTIFY dataChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY dataChanged)
    QML_ELEMENT

public:
    explicit ProfileItem(QQuickItem* parent = nullptr);

    QPolygonF terrain() const { return terrain_; }
    QPolygonF rayA() const { return rayA_; }
    QPolygonF rayB() const { return rayB_; }
    QPolygonF lens() const { return lens_; }
    QPolygonF directRay() const { return directRay_; }
    QPolygonF fresnelLower() const { return fresnelLower_; }
    QPolygonF fresnelUpper() const { return fresnelUpper_; }
    QVariantList voidSpans() const { return voidSpans_; }
    QVariantMap meta() const { return meta_; }
    bool darkTheme() const { return darkTheme_; }

    void setTerrain(const QPolygonF& v);
    void setRayA(const QPolygonF& v);
    void setRayB(const QPolygonF& v);
    void setLens(const QPolygonF& v);
    void setDirectRay(const QPolygonF& v);
    void setFresnelLower(const QPolygonF& v);
    void setFresnelUpper(const QPolygonF& v);
    void setVoidSpans(const QVariantList& v);
    void setMeta(const QVariantMap& v);
    void setDarkTheme(bool v);

    // Pixel mapping for QML label overlays and hover sync.
    Q_INVOKABLE double xToPixel(double distanceM) const;
    Q_INVOKABLE double yToPixel(double elevationM) const;
    Q_INVOKABLE double pixelToDistance(double px) const;
    Q_INVOKABLE QVariantList elevationTicks() const;
    Q_INVOKABLE QVariantList distanceTicks() const;

signals:
    void dataChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void computeViewport();

    QPolygonF terrain_;
    QPolygonF rayA_;
    QPolygonF rayB_;
    QPolygonF lens_;
    QPolygonF directRay_;
    QPolygonF fresnelLower_;
    QPolygonF fresnelUpper_;
    QVariantList voidSpans_;
    QVariantMap meta_;
    bool darkTheme_ = true;

    double x0_ = 0.0;
    double x1_ = 1.0;
    double y0_ = 0.0; // bottom elevation
    double y1_ = 1.0; // top elevation
    static constexpr double kMarginLeft = 52.0;
    static constexpr double kMarginRight = 14.0;
    static constexpr double kMarginTop = 12.0;
    static constexpr double kMarginBottom = 26.0;
};
