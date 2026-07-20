// Offscreen preview of the paper-topo tile renderer: composites a grid of tiles
// around a coordinate into one PNG. Development aid for tuning the cartography
// without driving the interactive map (run with QT_QPA_PLATFORM=offscreen).
//
//   render_topo_preview <lat> <lon> <zoom> <out.png> [dark] [cols rows] [terrainDir]

#include "core/terrain/terrain_store.h"
#include "ui/map/MapSources.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>

#include <cmath>
#include <cstdio>

int main(int argc, char** argv) {
    QGuiApplication::setOrganizationName(QStringLiteral("TropoLink"));
    QGuiApplication::setApplicationName(QStringLiteral("TropoLink"));
    QGuiApplication app(argc, argv);

    if (argc < 5) {
        std::fprintf(stderr, "usage: render_topo_preview lat lon zoom out.png [dark] [cols rows] [dir]\n");
        return 2;
    }
    const double lat = std::atof(argv[1]);
    const double lon = std::atof(argv[2]);
    const int z = std::atoi(argv[3]);
    const QString outPath = QString::fromLocal8Bit(argv[4]);
    const bool dark = argc > 5 && std::atoi(argv[5]) != 0;
    const int cols = argc > 7 ? std::atoi(argv[6]) : 5;
    const int rows = argc > 7 ? std::atoi(argv[7]) : 4;
    const QString terrainDir =
        argc > 8 ? QString::fromLocal8Bit(argv[8])
                 : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                       QStringLiteral("/terrain");

    auto store = tl::terrain::TerrainStore::open(terrainDir.toStdString());
    if (!store) {
        std::fprintf(stderr, "cannot open terrain store: %s\n", terrainDir.toUtf8().constData());
        return 1;
    }

    const int n = 1 << z;
    const int cx = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));
    const double latRad = lat * M_PI / 180.0;
    const int cy = static_cast<int>(
        std::floor((1.0 - std::asinh(std::tan(latRad)) / M_PI) / 2.0 * n));

    QImage sheet(cols * 256, rows * 256, QImage::Format_ARGB32);
    sheet.fill(dark ? qRgb(0x23, 0x26, 0x2b) : qRgb(0xf4, 0xf1, 0xe7));
    QPainter painter(&sheet);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const TileKey key{z, cx - cols / 2 + c, cy - rows / 2 + r};
            const QImage tile = renderTopoTile(store.value().get(), key, dark);
            painter.drawImage(c * 256, r * 256, tile);
        }
    }
    painter.end();
    if (!sheet.save(outPath)) {
        std::fprintf(stderr, "cannot save %s\n", outPath.toUtf8().constData());
        return 1;
    }
    std::printf("wrote %s (z=%d around %d/%d)\n", outPath.toUtf8().constData(), z, cx, cy);
    return 0;
}
