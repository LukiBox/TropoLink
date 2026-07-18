#pragma once

// KML export for external GIS: sites, the geodesic path and the common scattering
// volume at its true map position.

#include "core/common/expected.h"
#include "core/project/project.h"
#include "core/tropo/model_suite.h"

#include <string>

namespace tl::project {

[[nodiscard]] std::string buildKml(const Project& project, const LinkSpec& link,
                                   const tropo::SuiteResult& results);

[[nodiscard]] Status exportKml(const Project& project, const LinkSpec& link,
                               const tropo::SuiteResult& results, const std::string& utf8Path);

} // namespace tl::project
