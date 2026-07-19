#include "ui/report/PdfReport.h"

#include <gdal.h>
#include <GeographicLib/Constants.hpp>

#include <QDateTime>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>

#include <cmath>

using tl::report::Feasibility;
using tl::report::Language;

namespace {

// Layout in QPdfWriter device units (1/1200 inch at default resolution -> use
// painter window of 1000 x 1414 logical units for A4 proportions).
constexpr double kPageW = 1000.0;
constexpr double kPageH = 1414.0;
constexpr double kMargin = 70.0;

// Pixel-sized fonts scale with the painter window transform; point-sized fonts
// resolve against the 1200 dpi device and blow up ~10x under the window mapping.
QFont pdfFont(double pixelSize, bool bold = false) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(std::max(1, static_cast<int>(pixelSize)));
    f.setBold(bold);
    return f;
}

struct Cursor {
    QPainter* p = nullptr;
    QPdfWriter* writer = nullptr;
    double y = kMargin;
    int page = 1;
    QString footerLeft;
    QString footerRight;

    void footer() {
        QFont f = pdfFont(10.0);
        p->setFont(f);
        p->setPen(QColor(120, 120, 120));
        p->drawText(QRectF(kMargin, kPageH - 44, kPageW - 2 * kMargin, 20),
                    Qt::AlignLeft | Qt::AlignVCenter, footerLeft);
        p->drawText(QRectF(kMargin, kPageH - 44, kPageW - 2 * kMargin, 20),
                    Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1").arg(page));
    }

    void newPage() {
        footer();
        writer->newPage();
        ++page;
        y = kMargin;
    }

    void need(double h) {
        if (y + h > kPageH - 60.0) {
            newPage();
        }
    }
};

void drawParagraph(Cursor& c, const QString& text, double pointSize = 9.0, bool bold = false) {
    QFont f = pdfFont(pointSize * 1.4, bold);
    c.p->setFont(f);
    c.p->setPen(QColor(20, 20, 20));
    const double w = kPageW - 2 * kMargin;
    const QRectF bound =
        c.p->boundingRect(QRectF(kMargin, 0, w, 10000), Qt::TextWordWrap, text);
    c.need(bound.height() + 8);
    c.p->drawText(QRectF(kMargin, c.y, w, bound.height() + 4), Qt::TextWordWrap, text);
    c.y += bound.height() + 8;
}

void drawHeading(Cursor& c, const QString& text) {
    c.need(46);
    QFont f = pdfFont(17.0, true);
    c.p->setFont(f);
    c.p->setPen(QColor(20, 40, 70));
    c.p->drawText(QRectF(kMargin, c.y, kPageW - 2 * kMargin, 26), Qt::AlignLeft | Qt::AlignVCenter,
                  text);
    c.y += 28;
    c.p->setPen(QPen(QColor(20, 40, 70), 1.2));
    c.p->drawLine(QPointF(kMargin, c.y), QPointF(kPageW - kMargin, c.y));
    c.y += 10;
}

void drawTable(Cursor& c, const tl::report::Table& table) {
    if (!table.title.empty()) {
        drawParagraph(c, QString::fromStdString(table.title), 9.5, true);
    }
    const double w = kPageW - 2 * kMargin;
    const int cols = static_cast<int>(table.header.size());
    if (cols == 0) {
        return;
    }
    // First column wider for label-style tables.
    QVector<double> widths(cols, w / cols);
    if (cols == 2) {
        widths[0] = w * 0.46;
        widths[1] = w * 0.54;
    } else if (cols > 2) {
        widths[0] = w * 0.28;
        for (int i = 1; i < cols; ++i) {
            widths[i] = w * 0.72 / (cols - 1);
        }
    }

    QFont cell = pdfFont(11.0);
    QFont head = cell;
    head.setBold(true);

    auto rowHeight = [&](const std::vector<std::string>& row, const QFont& font) {
        double h = 16.0;
        c.p->setFont(font);
        for (int i = 0; i < cols && i < static_cast<int>(row.size()); ++i) {
            const QRectF b = c.p->boundingRect(QRectF(0, 0, widths[i] - 10, 10000), Qt::TextWordWrap,
                                               QString::fromStdString(row[i]));
            h = std::max(h, b.height() + 8.0);
        }
        return h;
    };

    auto drawRow = [&](const std::vector<std::string>& row, const QFont& font, bool header) {
        const double h = rowHeight(row, font);
        c.need(h);
        double x = kMargin;
        if (header) {
            c.p->fillRect(QRectF(kMargin, c.y, w, h), QColor(230, 236, 244));
        }
        c.p->setFont(font);
        c.p->setPen(QColor(25, 25, 25));
        for (int i = 0; i < cols && i < static_cast<int>(row.size()); ++i) {
            c.p->drawText(QRectF(x + 5, c.y + 3, widths[i] - 10, h - 6), Qt::TextWordWrap,
                          QString::fromStdString(row[i]));
            x += widths[i];
        }
        c.p->setPen(QPen(QColor(190, 190, 190), 0.6));
        c.p->drawLine(QPointF(kMargin, c.y + h), QPointF(kMargin + w, c.y + h));
        c.y += h;
    };

    drawRow(table.header, head, true);
    for (const auto& row : table.rows) {
        drawRow(row, cell, false);
    }
    c.y += 10;
}

void drawProfileFigure(Cursor& c, const ReportRenderInputs& in, const QString& caption) {
    if (in.terrain.size() < 2) {
        return;
    }
    const double w = kPageW - 2 * kMargin;
    const double h = 300.0;
    c.need(h + 30);
    const QRectF rect(kMargin, c.y, w, h);
    c.p->save();
    c.p->setClipRect(rect);
    c.p->fillRect(rect, QColor(248, 250, 252));

    const double d = in.profileMeta.value("distanceM").toDouble();
    const double minY = in.profileMeta.value("minY").toDouble();
    const double maxY = std::max(in.profileMeta.value("maxY").toDouble(),
                                 in.profileMeta.value("lensTopY").toDouble());
    const double span = std::max(50.0, maxY - minY);
    const double yLo = minY - span * 0.06;
    const double yHi = maxY + span * 0.10;
    auto X = [&](double xm) { return rect.left() + xm / std::max(1.0, d) * rect.width(); };
    auto Y = [&](double ym) { return rect.bottom() - (ym - yLo) / (yHi - yLo) * rect.height(); };

    // Terrain.
    QPainterPath terrain;
    terrain.moveTo(X(in.terrain.first().x()), rect.bottom());
    for (const auto& p : in.terrain) {
        terrain.lineTo(X(p.x()), Y(p.y()));
    }
    terrain.lineTo(X(in.terrain.last().x()), rect.bottom());
    terrain.closeSubpath();
    c.p->fillPath(terrain, QColor(196, 208, 186));
    c.p->setPen(QPen(QColor(96, 120, 88), 1.2));
    c.p->drawPath(terrain);

    // Lens.
    if (in.lens.size() >= 4) {
        QPainterPath lens;
        lens.moveTo(X(in.lens.first().x()), Y(in.lens.first().y()));
        for (const auto& p : in.lens) {
            lens.lineTo(X(p.x()), Y(p.y()));
        }
        lens.closeSubpath();
        c.p->fillPath(lens, QColor(30, 140, 60, 70));
        c.p->setPen(QPen(QColor(30, 140, 60, 180), 1.0));
        c.p->drawPath(lens);
    }
    // Rays.
    c.p->setPen(QPen(QColor(30, 90, 190), 1.4));
    for (const auto* ray : {&in.rayA, &in.rayB}) {
        if (ray->size() >= 2) {
            c.p->drawLine(QPointF(X(ray->first().x()), Y(ray->first().y())),
                          QPointF(X(ray->last().x()), Y(ray->last().y())));
        }
    }
    // Direct (blocked) ray.
    if (in.directRay.size() >= 2) {
        c.p->setPen(QPen(QColor(190, 40, 40), 1.0, Qt::DashLine));
        c.p->drawLine(QPointF(X(in.directRay.first().x()), Y(in.directRay.first().y())),
                      QPointF(X(in.directRay.last().x()), Y(in.directRay.last().y())));
    }
    // Masts.
    const double hA = in.profileMeta.value("antennaA").toDouble();
    const double hB = in.profileMeta.value("antennaB").toDouble();
    const double aglA = in.profileMeta.value("aglA").toDouble();
    const double aglB = in.profileMeta.value("aglB").toDouble();
    c.p->setPen(QPen(QColor(20, 20, 20), 2.4));
    c.p->drawLine(QPointF(X(0), Y(hA - aglA)), QPointF(X(0), Y(hA)));
    c.p->drawLine(QPointF(X(d), Y(hB - aglB)), QPointF(X(d), Y(hB)));

    // Scatter-angle annotation at the lens.
    const double lensX = in.profileMeta.value("lensX").toDouble();
    const double lensBaseY = in.profileMeta.value("lensBaseY").toDouble();
    QFont small = pdfFont(10.0);
    c.p->setFont(small);
    c.p->setPen(QColor(20, 20, 20));
    c.p->drawText(QPointF(X(lensX) + 6, Y(lensBaseY) - 4),
                  QStringLiteral("\xCE\xB8 = %1 mrad")
                      .arg(in.profileMeta.value("thetaMrad").toDouble(), 0, 'f', 2));
    c.p->restore();
    c.p->setPen(QPen(QColor(150, 150, 150), 0.8));
    c.p->drawRect(rect);
    c.y += h + 6;
    drawParagraph(c, caption, 7.5);
}

void drawCurveFigure(Cursor& c, const ComputeOutcome* outcome, tl::budget::DiversityMode diversity,
                     const QString& caption) {
    if (outcome == nullptr || !outcome->engine) {
        return;
    }
    const double w = kPageW - 2 * kMargin;
    const double h = 260.0;
    c.need(h + 30);
    const QRectF rect(kMargin, c.y, w, h);
    c.p->fillRect(rect, QColor(248, 250, 252));
    c.p->setPen(QPen(QColor(150, 150, 150), 0.8));
    c.p->drawRect(rect);

    const auto curve = outcome->engine->curve(diversity, false);
    auto nines = [](double a) { return -std::log10(std::clamp(100.0 - a, 1e-5, 100.0)); };
    const double mMin = curve.front().marginDb;
    const double mMax = curve.back().marginDb;
    auto X = [&](double m) { return rect.left() + 40 + (m - mMin) / (mMax - mMin) * (rect.width() - 55); };
    auto Y = [&](double a) {
        return rect.bottom() - 24 - (std::clamp(nines(a), -2.0, 4.2) + 2.0) / 6.2 * (rect.height() - 40);
    };
    QFont small = pdfFont(10.0);
    c.p->setFont(small);
    for (const double a : {90.0, 99.0, 99.9, 99.99}) {
        c.p->setPen(QPen(QColor(200, 200, 200), 0.6, Qt::DotLine));
        c.p->drawLine(QPointF(rect.left() + 40, Y(a)), QPointF(rect.right() - 15, Y(a)));
        c.p->setPen(QColor(90, 90, 90));
        c.p->drawText(QRectF(rect.left(), Y(a) - 8, 36, 16), Qt::AlignRight | Qt::AlignVCenter,
                      QString::number(a));
    }
    QPainterPath path;
    for (std::size_t i = 0; i < curve.size(); ++i) {
        const QPointF pt(X(curve[i].marginDb), Y(curve[i].availabilityPercent));
        if (i == 0) {
            path.moveTo(pt);
        } else {
            path.lineTo(pt);
        }
    }
    c.p->setPen(QPen(QColor(30, 90, 190), 1.6));
    c.p->drawPath(path);
    // Current margin marker.
    c.p->setPen(QPen(QColor(30, 140, 60), 1.2, Qt::DashLine));
    const double margin = outcome->budget.fadeMargin.value();
    if (margin >= mMin && margin <= mMax) {
        c.p->drawLine(QPointF(X(margin), rect.top() + 8), QPointF(X(margin), rect.bottom() - 24));
    }
    c.y += h + 6;
    drawParagraph(c, caption, 7.5);
}

} // namespace

ReportRenderResult renderPdfReport(const ReportRenderInputs& in, const QString& path) {
    ReportRenderResult result;
    if (in.project == nullptr || in.outcome == nullptr || in.project->links.empty()) {
        result.error = QStringLiteral("nothing to report");
        return result;
    }
    const auto& link = in.project->links.front();

    // Build the (deterministic, hashed) content in core.
    tl::report::ReportInputs cin{*in.project,
                                 link,
                                 in.outcome->suite,
                                 in.outcome->budget,
                                 *in.outcome->engine,
                                 in.outcome->availabilityAnnual,
                                 in.outcome->availabilityWorstMonth,
                                 in.outcome->separation,
                                 in.terrainSources,
                                 TROPOLINK_VERSION,
                                 GDALVersionInfo("RELEASE_NAME"),
                                 GEOGRAPHICLIB_VERSION_STRING,
                                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString(),
                                 in.aiCommentary.toStdString()};
    const auto content = tl::report::buildReportContent(cin, in.language);
    result.contentSha256 = QString::fromStdString(content.contentSha256);

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(QString::fromStdString(content.title));
    writer.setCreator(QStringLiteral("TropoLink ") + QStringLiteral(TROPOLINK_VERSION));

    QPainter painter;
    if (!painter.begin(&writer)) {
        result.error = QStringLiteral("cannot open PDF for writing: %1").arg(path);
        return result;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setWindow(0, 0, static_cast<int>(kPageW), static_cast<int>(kPageH));

    Cursor c;
    c.p = &painter;
    c.writer = &writer;
    c.footerLeft = QStringLiteral("TropoLink %1 - SHA-256 %2")
                       .arg(QStringLiteral(TROPOLINK_VERSION), result.contentSha256.left(16));

    // --- Cover ---------------------------------------------------------------
    painter.fillRect(QRectF(0, 0, kPageW, 210), QColor(24, 38, 58));
    painter.setPen(Qt::white);
    QFont title = pdfFont(30.0, true);
    painter.setFont(title);
    painter.drawText(QRectF(kMargin, 60, kPageW - 2 * kMargin, 46), Qt::AlignLeft,
                     QString::fromStdString(content.title));
    painter.setFont(pdfFont(15.0));
    painter.drawText(QRectF(kMargin, 120, kPageW - 2 * kMargin, 30), Qt::AlignLeft,
                     QStringLiteral("%1  /  %2")
                         .arg(QString::fromStdString(content.projectName),
                              QString::fromStdString(content.linkName)));
    painter.drawText(QRectF(kMargin, 152, kPageW - 2 * kMargin, 26), Qt::AlignLeft,
                     QString::fromStdString(content.generatedStamp));

    const QColor feasColor = content.feasibility == Feasibility::Green    ? QColor(46, 160, 67)
                             : content.feasibility == Feasibility::Yellow ? QColor(212, 167, 44)
                                                                          : QColor(207, 60, 60);
    painter.fillRect(QRectF(0, 210, kPageW, 56), feasColor);
    painter.setPen(Qt::white);
    QFont feasFont = pdfFont(17.0, true);
    painter.setFont(feasFont);
    painter.drawText(QRectF(kMargin, 210, kPageW - 2 * kMargin, 56), Qt::AlignVCenter,
                     QString::fromStdString(content.feasibilityStatement));
    c.y = 300.0;

    // --- Sections ------------------------------------------------------------
    for (const auto& section : content.sections) {
        drawHeading(c, QString::fromStdString(section.title));
        for (const auto& para : section.paragraphs) {
            drawParagraph(c, QString::fromStdString(para));
        }
        for (const auto& table : section.tables) {
            drawTable(c, table);
        }
        for (const auto& figure : section.figureKeys) {
            const QString caption =
                QString::fromStdString(tl::report::reportString("fig_" + figure, in.language));
            if (figure == "map" && !in.mapSnapshot.isNull()) {
                const double w = kPageW - 2 * kMargin;
                const double h = w * in.mapSnapshot.height() / std::max(1, in.mapSnapshot.width());
                c.need(h + 30);
                painter.drawImage(QRectF(kMargin, c.y, w, h), in.mapSnapshot);
                painter.setPen(QPen(QColor(150, 150, 150), 0.8));
                painter.drawRect(QRectF(kMargin, c.y, w, h));
                c.y += h + 6;
                drawParagraph(c, caption, 7.5);
            } else if (figure == "profile") {
                drawProfileFigure(c, in, caption);
            } else if (figure == "curve") {
                drawCurveFigure(c, in.outcome, in.diversity, caption);
            }
        }
    }
    // Content hash on the audit trail.
    drawParagraph(c,
                  QStringLiteral("%1: %2").arg(
                      QString::fromStdString(tl::report::reportString("report_hash", in.language)),
                      result.contentSha256),
                  8.0, true);
    c.footer();
    painter.end();
    result.ok = true;
    return result;
}
