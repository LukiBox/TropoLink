#include "core/project/csv_export.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tl::project {

std::string profileCsv(const terrain::Profile& profile) {
    std::ostringstream csv;
    csv << "distance_m,latitude_deg,longitude_deg,elevation_m,void_interpolated\n";
    char line[160];
    for (const auto& p : profile.points) {
        std::snprintf(line, sizeof(line), "%.1f,%.8f,%.8f,%.2f,%d\n", p.distance.value(),
                      p.position.latitude.value(), p.position.longitude.value(), p.elevation.value(),
                      p.interpolatedVoid ? 1 : 0);
        csv << line;
    }
    return csv.str();
}

std::string budgetCsv(const budget::LinkBudget& budget, const tropo::SuiteResult& results) {
    std::ostringstream csv;
    csv << "section,item,value_db\n";
    for (const auto& item : budget.waterfall) {
        csv << "budget," << item.label << "," << item.valueDb << "\n";
    }
    csv << "geometry,distance_km," << results.inverse.distance.kilometers() << "\n";
    csv << "geometry,azimuth_ab_deg," << results.inverse.forwardAzimuth.value() << "\n";
    csv << "geometry,azimuth_ba_deg," << results.inverse.reverseAzimuth.value() << "\n";
    csv << "geometry,scatter_angle_mrad," << results.geometry.scatterAngle.milliradians() << "\n";
    csv << "geometry,common_volume_base_amsl_m," << results.geometry.volumeBaseAmsl.value() << "\n";
    for (const auto& row : results.rows) {
        csv << "model_median_db," << row.name << "," << (row.valid ? row.median.value() : 0.0) << "\n";
    }
    csv << "models,spread_db," << results.spread.value() << "\n";
    return csv.str();
}

Status exportCsv(const std::string& content, const std::string& utf8Path) {
    std::ofstream out(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())),
                      std::ios::trunc | std::ios::binary);
    if (!out) {
        return Error{"cannot write CSV file: " + utf8Path};
    }
    out << content;
    return Status::ok();
}

} // namespace tl::project
