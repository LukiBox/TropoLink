#pragma once

// CSV export: terrain profile table and link-budget/model tables.

#include "core/budget/link_budget.h"
#include "core/common/expected.h"
#include "core/terrain/profile.h"
#include "core/tropo/model_suite.h"

#include <string>

namespace tl::project {

[[nodiscard]] std::string profileCsv(const terrain::Profile& profile);
[[nodiscard]] std::string budgetCsv(const budget::LinkBudget& budget, const tropo::SuiteResult& results);

[[nodiscard]] Status exportCsv(const std::string& content, const std::string& utf8Path);

} // namespace tl::project
