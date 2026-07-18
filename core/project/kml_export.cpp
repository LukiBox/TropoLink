#include "core/project/kml_export.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tl::project {

namespace {
std::string escapeXml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

std::string coordinate(const geo::GeoPoint& p, double altitudeM = 0.0) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%.8f,%.8f,%.1f", p.longitude.value(), p.latitude.value(), altitudeM);
    return buf;
}
} // namespace

std::string buildKml(const Project& project, const LinkSpec& link, const tropo::SuiteResult& results) {
    const Site* siteA = project.findSite(link.siteAId);
    const Site* siteB = project.findSite(link.siteBId);
    std::ostringstream kml;
    kml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n<Document>\n"
        << "<name>" << escapeXml(project.name) << " - " << escapeXml(link.name) << "</name>\n"
        << "<Style id=\"path\"><LineStyle><color>ff00a5ff</color><width>3</width></LineStyle></Style>\n"
        << "<Style id=\"volume\"><IconStyle><scale>1.2</scale></IconStyle></Style>\n";

    auto placemark = [&kml](const std::string& name, const std::string& description,
                            const std::string& coords) {
        kml << "<Placemark><name>" << escapeXml(name) << "</name>";
        if (!description.empty()) {
            kml << "<description>" << escapeXml(description) << "</description>";
        }
        kml << "<Point><coordinates>" << coords << "</coordinates></Point></Placemark>\n";
    };

    if (siteA != nullptr) {
        placemark(siteA->name, "TropoLink site A", coordinate(siteA->position));
    }
    if (siteB != nullptr) {
        placemark(siteB->name, "TropoLink site B", coordinate(siteB->position));
    }

    if (siteA != nullptr && siteB != nullptr) {
        kml << "<Placemark><name>" << escapeXml(link.name)
            << "</name><styleUrl>#path</styleUrl><LineString><tessellate>1</tessellate><coordinates>\n";
        const auto samples = geo::Geodesy::sampleLine(siteA->position, siteB->position, 129);
        for (const auto& s : samples) {
            kml << coordinate(s.point) << "\n";
        }
        kml << "</coordinates></LineString></Placemark>\n";
    }

    {
        char desc[256];
        std::snprintf(desc, sizeof(desc),
                      "Common scattering volume. Base %.0f m AMSL, top %.0f m AMSL, scatter angle %.2f mrad.",
                      results.geometry.volumeBaseAmsl.value(), results.geometry.volumeTopAmsl.value(),
                      results.geometry.scatterAngle.milliradians());
        kml << "<Placemark><name>Common volume</name><styleUrl>#volume</styleUrl><description>"
            << escapeXml(desc) << "</description><Point><altitudeMode>absolute</altitudeMode><coordinates>"
            << coordinate(results.commonVolumePosition, results.geometry.volumeBaseAmsl.value())
            << "</coordinates></Point></Placemark>\n";
    }

    kml << "</Document>\n</kml>\n";
    return kml.str();
}

Status exportKml(const Project& project, const LinkSpec& link, const tropo::SuiteResult& results,
                 const std::string& utf8Path) {
    std::ofstream out(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())),
                      std::ios::trunc | std::ios::binary);
    if (!out) {
        return Error{"cannot write KML file: " + utf8Path};
    }
    out << buildKml(project, link, results);
    return Status::ok();
}

} // namespace tl::project
