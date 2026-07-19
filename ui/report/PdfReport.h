#pragma once

// PDF rendering of the core-built report content via QPdfWriter. The renderer draws
// the cover with the red/yellow/green feasibility rating, every section table, the
// map snapshot, the profile figure with the common-volume lens (vector, drawn with
// the same geometry the live view uses) and the availability-vs-margin curve.

#include "core/budget/availability.h"
#include "core/project/project.h"
#include "core/report/report_content.h"
#include "ui/models/AppController.h"

#include <QImage>
#include <QPolygonF>
#include <QString>
#include <QVariantList>

struct ReportRenderInputs {
    const tl::project::Project* project = nullptr;
    const ComputeOutcome* outcome = nullptr;
    tl::budget::DiversityMode diversity = tl::budget::DiversityMode::Quad;
    tl::report::Language language = tl::report::Language::Polish;
    std::vector<tl::terrain::StoreEntry> terrainSources;
    QImage mapSnapshot;
    // Profile figure data in display space (as built by AppController).
    QPolygonF terrain;
    QPolygonF rayA;
    QPolygonF rayB;
    QPolygonF lens;
    QPolygonF directRay;
    QVariantMap profileMeta;
    QString aiCommentary;
};

struct ReportRenderResult {
    bool ok = false;
    QString error;
    QString contentSha256;
};

[[nodiscard]] ReportRenderResult renderPdfReport(const ReportRenderInputs& inputs, const QString& path);
