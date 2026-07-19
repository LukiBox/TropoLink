// make_dted: derive DTED tiles from any GDAL-readable DEM (SRTM HGT, GeoTIFF).
//
//   make_dted <level 0|1|2> <input> <output.dtN>
//
// DTED cell sizes per MIL-PRF-89020B: level 0 = 121x121 (30"), level 1 = 1201x1201
// (3"), level 2 = 3601x3601 (1"). The input must cover a whole 1x1 degree cell.
// Used to produce the bundled reference terrain (SRTM-derived DTED-0).

#include <gdal_priv.h>
#include <gdal_utils.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: make_dted <0|1|2> <input> <output.dtN>\n");
        return 2;
    }
    GDALAllRegister();
    const int level = std::atoi(argv[1]);
    const int size = level == 0 ? 121 : level == 1 ? 1201 : 3601;

    GDALDatasetH src = GDALOpen(argv[2], GA_ReadOnly);
    if (src == nullptr) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 1;
    }

    // Whole-degree cell bounds (SRTM is pixel-centred on the degree grid).
    double gt[6] = {};
    GDALGetGeoTransform(src, gt);
    const double lonW = std::floor(gt[0] + 0.5);
    const double latN = std::floor(gt[3] + 0.5);
    const double latS = latN - 1.0;

    // MIL-PRF-89020B: longitude posting widens with latitude.
    const double absLat = latS >= 0.0 ? latS : -latN;
    int lonDivisor = 1;
    if (absLat >= 80.0) {
        lonDivisor = 6;
    } else if (absLat >= 75.0) {
        lonDivisor = 4;
    } else if (absLat >= 70.0) {
        lonDivisor = 3;
    } else if (absLat >= 50.0) {
        lonDivisor = 2;
    }
    const int lonSize = (size - 1) / lonDivisor + 1;

    char latSizeStr[16];
    char lonSizeStr[16];
    char projwin[128];
    std::snprintf(latSizeStr, sizeof(latSizeStr), "%d", size);
    std::snprintf(lonSizeStr, sizeof(lonSizeStr), "%d", lonSize);
    std::snprintf(projwin, sizeof(projwin), "%.1f %.1f %.1f %.1f", lonW, latN, lonW + 1.0, latS);
    // -projwin ulx uly lrx lry snaps the cell to exact degree boundaries.
    std::string pw(projwin);
    const auto s1 = pw.find(' ');
    const auto s2 = pw.find(' ', s1 + 1);
    const auto s3 = pw.find(' ', s2 + 1);
    const std::string ulx = pw.substr(0, s1);
    const std::string uly = pw.substr(s1 + 1, s2 - s1 - 1);
    const std::string lrx = pw.substr(s2 + 1, s3 - s2 - 1);
    const std::string lry = pw.substr(s3 + 1);
    const char* args[] = {"-of",       "DTED",      "-outsize",  lonSizeStr, latSizeStr,
                          "-r",        "average",   "-projwin",  ulx.c_str(), uly.c_str(),
                          lrx.c_str(), lry.c_str(), nullptr};

    GDALTranslateOptions* options =
        GDALTranslateOptionsNew(const_cast<char**>(args), nullptr);
    if (options == nullptr) {
        std::fprintf(stderr, "bad translate options\n");
        return 1;
    }
    int usageError = 0;
    GDALDatasetH dst = GDALTranslate(argv[3], src, options, &usageError);
    GDALTranslateOptionsFree(options);
    GDALClose(src);
    if (dst == nullptr) {
        std::fprintf(stderr, "DTED translation failed (usage error %d)\n", usageError);
        return 1;
    }
    GDALClose(dst);
    std::printf("wrote %s (%dx%d, DTED level %d)\n", argv[3], lonSize, size, level);
    return 0;
}
