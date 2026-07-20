#pragma once

// In-app help: detailed bilingual (PL/EN) guides for every panel and workflow,
// served as Qt rich text. Content lives in code (like the report vocabulary) so
// both languages ship complete and the Air-Gap flavor needs no external files.

#include <QString>
#include <QVariantList>

// Ordered topic list: [{id, title}] in the requested language.
QVariantList tropolinkHelpTopics(bool polish);

// Rich-text body for one topic id; empty string for unknown ids.
QString tropolinkHelpHtml(const QString& topic, bool polish);
