#pragma once

// Availability-vs-margin curve (used in the availability block and the report).

#include <QQuickPaintedItem>
#include <QVariantList>

class CurveItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY dataChanged)
    Q_PROPERTY(double currentMargin READ currentMargin WRITE setCurrentMargin NOTIFY dataChanged)
    Q_PROPERTY(double targetAvailability READ targetAvailability WRITE setTargetAvailability NOTIFY dataChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY dataChanged)
    QML_ELEMENT

public:
    explicit CurveItem(QQuickItem* parent = nullptr);

    QVariantList points() const { return points_; }
    double currentMargin() const { return currentMargin_; }
    double targetAvailability() const { return targetAvailability_; }
    bool darkTheme() const { return darkTheme_; }
    void setPoints(const QVariantList& v);
    void setCurrentMargin(double v);
    void setTargetAvailability(double v);
    void setDarkTheme(bool v);

    void paint(QPainter* painter) override;

signals:
    void dataChanged();

private:
    QVariantList points_;
    double currentMargin_ = 0.0;
    double targetAvailability_ = 99.9;
    bool darkTheme_ = true;
};
