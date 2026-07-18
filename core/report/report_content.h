#pragma once

// Link-design report content, built entirely in core (rendered to PDF by the UI
// layer via QPdfWriter). Bilingual: every string resolves through the PL/EN table at
// build time. The canonical text serialization is SHA-256 hashed — identical inputs
// produce an identical, auditable report fingerprint.

#include "core/budget/availability.h"
#include "core/budget/link_budget.h"
#include "core/budget/solver.h"
#include "core/project/project.h"
#include "core/terrain/terrain_store.h"
#include "core/tropo/model_suite.h"

#include <string>
#include <vector>

namespace tl::report {

enum class Language { Polish, English };
enum class Feasibility { Green, Yellow, Red };

struct Table {
    std::string title;
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
};

struct Section {
    std::string key;   // stable machine key ("geometry", "budget", ...)
    std::string title; // localized
    std::vector<std::string> paragraphs;
    std::vector<Table> tables;
    std::vector<std::string> figureKeys; // renderer inserts figures (map, profile, curve)
};

struct ReportContent {
    Language language = Language::Polish;
    std::string title;
    std::string projectName;
    std::string linkName;
    std::string generatedStamp; // excluded from the content hash (audit metadata)
    Feasibility feasibility = Feasibility::Red;
    std::string feasibilityStatement;
    std::vector<Section> sections;
    std::string contentSha256; // filled by finalizeHash()

    [[nodiscard]] std::string canonicalText() const; // deterministic serialization
    void finalizeHash();
};

struct ReportInputs {
    const project::Project& project;
    const project::LinkSpec& link;
    const tropo::SuiteResult& suite;
    const budget::LinkBudget& budget;
    const budget::AvailabilityEngine& availability;
    Percent achievedAvailabilityAnnual{0.0};
    Percent achievedAvailabilityWorstMonth{0.0};
    budget::DiversitySeparation separation;
    std::vector<terrain::StoreEntry> terrainSources;
    std::string appVersion;
    std::string gdalVersion;
    std::string geographicLibVersion;
    std::string generatedStamp;
    std::string aiCommentary; // optional Ollama paragraph, empty in air-gap builds
};

[[nodiscard]] ReportContent buildReportContent(const ReportInputs& inputs, Language language);

// Localized string lookup for the report vocabulary (exposed for the UI renderer).
[[nodiscard]] std::string reportString(const std::string& key, Language language);

} // namespace tl::report
