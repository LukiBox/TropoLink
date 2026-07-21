#include "ui/map/MapSources.h"

#include <sqlite3.h>

#ifndef TROPOLINK_AIRGAP
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QPainter>
#include <QStandardPaths>
#include <QThreadPool>
#include <QtMath>

#include <array>
#include <cmath>
#include <cstdint>

namespace {

// Web-mercator tile bounds in degrees.
double tileLon(int x, int z) {
    return x / std::pow(2.0, z) * 360.0 - 180.0;
}
double tileLat(int y, int z) {
    const double n = M_PI - 2.0 * M_PI * y / std::pow(2.0, z);
    return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

int imageCost(const QImage& img) {
    return static_cast<int>(std::min<qsizetype>(img.sizeInBytes(), 1 << 30)) + 64;
}

// ---- paper-topo cartography -------------------------------------------------

struct RampStop {
    double elevM;
    int r, g, b;
};

// Classic printed-topo hypsometric tints: woodland green in the lowlands, cream,
// then tan and brown with height, grey-white in high mountains.
constexpr std::array<RampStop, 10> kLightRamp{{{0, 200, 221, 167},
                                               {150, 209, 228, 178},
                                               {350, 223, 232, 188},
                                               {600, 235, 226, 175},
                                               {900, 231, 209, 152},
                                               {1300, 220, 187, 133},
                                               {1800, 206, 164, 115},
                                               {2400, 198, 166, 130},
                                               {3000, 226, 226, 226},
                                               {4200, 247, 247, 249}}};

// Night variant: same map, printed on dark slate — desaturated, readable.
constexpr std::array<RampStop, 10> kDarkRamp{{{0, 42, 54, 42},
                                              {150, 47, 58, 45},
                                              {350, 55, 62, 48},
                                              {600, 64, 66, 50},
                                              {900, 71, 66, 52},
                                              {1300, 77, 69, 56},
                                              {1800, 83, 74, 62},
                                              {2400, 88, 84, 80},
                                              {3000, 106, 108, 112},
                                              {4200, 130, 133, 138}}};

template <std::size_t N>
void rampColor(const std::array<RampStop, N>& ramp, double e, int& r, int& g, int& b) {
    if (e <= ramp.front().elevM) {
        r = ramp.front().r;
        g = ramp.front().g;
        b = ramp.front().b;
        return;
    }
    for (std::size_t i = 1; i < N; ++i) {
        if (e <= ramp[i].elevM) {
            const double f = (e - ramp[i - 1].elevM) / (ramp[i].elevM - ramp[i - 1].elevM);
            r = static_cast<int>(ramp[i - 1].r + f * (ramp[i].r - ramp[i - 1].r));
            g = static_cast<int>(ramp[i - 1].g + f * (ramp[i].g - ramp[i - 1].g));
            b = static_cast<int>(ramp[i - 1].b + f * (ramp[i].b - ramp[i - 1].b));
            return;
        }
    }
    r = ramp.back().r;
    g = ramp.back().g;
    b = ramp.back().b;
}

// Contour interval per zoom level (metres). Fixed per z so adjacent tiles agree.
// Chosen so lowland relief still draws readable line-work (Polish 1:50k sheets
// use 2.5-5 m contours; DEM posting limits us to 5 m at best).
double contourIntervalFor(int z) {
    if (z <= 6) {
        return 0.0; // none — too coarse to read
    }
    switch (z) {
    case 7:
        return 200.0;
    case 8:
    case 9:
        return 100.0;
    case 10:
        return 50.0;
    case 11:
    case 12:
        return 20.0;
    case 13:
    case 14:
        return 10.0;
    default:
        return 5.0; // z >= 15
    }
}

struct TopoPalette {
    QRgb water;
    int inkMinorR, inkMinorG, inkMinorB;
    double inkMinorA;
    int inkIndexR, inkIndexG, inkIndexB;
    double inkIndexA;
    QColor label;
    QColor labelHalo;
};

TopoPalette paletteFor(bool dark) {
    if (dark) {
        return {qRgb(30, 48, 62),  148, 156, 166, 0.42, 190, 198, 208, 0.68, QColor(205, 212, 220),
                QColor(35, 38, 43)};
    }
    return {qRgb(172, 205, 226),  130, 104, 66, 0.55, 96, 72, 40, 0.85, QColor(88, 66, 36),
            QColor(246, 243, 233)};
}

} // namespace

// The full paper-topo tile render: hypsometric tint + hillshade + contours + labels.
QImage renderTopoTile(const tl::terrain::TerrainStore* store, const TileKey& key, bool dark) {
    constexpr int kSize = 256;
    constexpr int kPad = 2; // ring for smoothing + gradients
    constexpr int kGrid = kSize + 2 * kPad;
    QImage image(kSize, kSize, QImage::Format_ARGB32);
    const double lon0 = tileLon(key.x, key.z);
    const double lon1 = tileLon(key.x + 1, key.z);
    const double lat0 = tileLat(key.y, key.z);     // north edge
    const double lat1 = tileLat(key.y + 1, key.z); // south edge

    // Elevation grid two texels wider than the tile (smoothing + gradients);
    // validity tracked so uncovered pixels stay transparent instead of pretending
    // to be sea level.
    std::vector<float> raw(static_cast<std::size_t>(kGrid) * kGrid);
    std::vector<std::uint8_t> valid(static_cast<std::size_t>(kGrid) * kGrid, 0);
    bool anyData = false;
    for (int gy = 0; gy < kGrid; ++gy) {
        const double lat = lat0 + (lat1 - lat0) * (gy - kPad + 0.5) / kSize;
        for (int gx = 0; gx < kGrid; ++gx) {
            const double lon = lon0 + (lon1 - lon0) * (gx - kPad + 0.5) / kSize;
            float v = 0.0f;
            const auto sample = store->elevationAt(tl::geo::GeoPoint{tl::Degrees(lat), tl::Degrees(lon)});
            const std::size_t idx = static_cast<std::size_t>(gy) * kGrid + gx;
            if (sample && !sample->isVoid) {
                v = static_cast<float>(sample->elevation.value());
                valid[idx] = 1;
                anyData = true;
            }
            raw[idx] = v;
        }
    }
    if (!anyData) {
        image.fill(Qt::transparent);
        return image;
    }

    // Two passes of 3x3 binomial smoothing (~gaussian sigma 1): dissolves
    // coarse-DEM posting blockiness and metre-level steps at dataset seams that
    // would otherwise etch false lines into shading and contours.
    std::vector<float> grid = raw;
    {
        std::vector<float> tmp(grid.size());
        for (int pass = 0; pass < 2; ++pass) {
            tmp = grid;
            for (int gy = 1; gy < kGrid - 1; ++gy) {
                for (int gx = 1; gx < kGrid - 1; ++gx) {
                    const std::size_t i = static_cast<std::size_t>(gy) * kGrid + gx;
                    bool ok = true;
                    for (int dy = -1; dy <= 1 && ok; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (valid[i + static_cast<std::size_t>(dy) * kGrid + dx] == 0) {
                                ok = false;
                                break;
                            }
                        }
                    }
                    if (ok) {
                        grid[i] = (tmp[i - kGrid - 1] + 2 * tmp[i - kGrid] + tmp[i - kGrid + 1] +
                                   2 * tmp[i - 1] + 4 * tmp[i] + 2 * tmp[i + 1] + tmp[i + kGrid - 1] +
                                   2 * tmp[i + kGrid] + tmp[i + kGrid + 1]) /
                                  16.0f;
                    }
                }
            }
        }
    }

    const double metersPerPixelY = std::abs(lat1 - lat0) * 111320.0 / kSize;
    const double metersPerPixelX =
        std::abs(lon1 - lon0) * 111320.0 * std::cos((lat0 + lat1) * 0.5 * M_PI / 180.0) / kSize;
    const double azimuth = 315.0 * M_PI / 180.0;
    const double altitude = 45.0 * M_PI / 180.0;
    // Vertical exaggeration grows as the view zooms out, so relief stays legible at
    // small scales (a paper 1:250k sheet does the same with its shading).
    const double exaggeration = std::clamp(std::pow(2.0, (13.0 - key.z) * 0.5), 1.3, 5.0);
    const double interval = contourIntervalFor(key.z);
    const double flatFadeGpp = interval > 0.0 ? interval / 96.0 : 0.0;
    const TopoPalette pal = paletteFor(dark);

    for (int y = 0; y < kSize; ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < kSize; ++x) {
            const std::size_t center = static_cast<std::size_t>(y + kPad) * kGrid + (x + kPad);
            if (valid[center] == 0) {
                line[x] = qRgba(0, 0, 0, 0); // no DEM coverage: blank paper shows through
                continue;
            }
            auto at = [&](int dx, int dy) {
                return static_cast<double>(
                    grid[static_cast<std::size_t>(y + kPad + dy) * kGrid + (x + kPad + dx)]);
            };
            const double e = at(0, 0);

            if (raw[center] <= 0.0f) { // sea / large water bodies at datum
                line[x] = pal.water;
                continue;
            }

            const double dzdx =
                ((at(1, -1) + 2 * at(1, 0) + at(1, 1)) - (at(-1, -1) + 2 * at(-1, 0) + at(-1, 1))) /
                (8.0 * metersPerPixelX);
            const double dzdy =
                ((at(-1, 1) + 2 * at(0, 1) + at(1, 1)) - (at(-1, -1) + 2 * at(0, -1) + at(1, -1))) /
                (8.0 * metersPerPixelY);

            // Base hypsometric tint.
            int r = 0;
            int g = 0;
            int b = 0;
            if (dark) {
                rampColor(kDarkRamp, e, r, g, b);
            } else {
                rampColor(kLightRamp, e, r, g, b);
            }

            // Horn hillshade, multiplied like the grey shading of a printed sheet.
            const double slope = std::atan(exaggeration * std::hypot(dzdx, dzdy));
            const double aspect = std::atan2(dzdy, -dzdx);
            double shade = std::sin(altitude) * std::cos(slope) +
                           std::cos(altitude) * std::sin(slope) * std::cos(azimuth - aspect);
            shade = std::clamp(shade, 0.0, 1.0);
            const double factor = dark ? (0.46 + 0.66 * shade) : (0.50 + 0.58 * shade);
            double fr = r * factor;
            double fg = g * factor;
            double fb = b * factor;

            // Contour ink. Anti-aliased by distance-to-level measured in screen pixels
            // through the local gradient; faded out over flats where a level line
            // would smear into a blob, capped in metres so an elevation step at a
            // dataset seam cannot etch a false line, and faded where real contours
            // would merge (cliffs).
            if (interval > 0.0) {
                const double gpp = std::hypot(dzdx * metersPerPixelX, dzdy * metersPerPixelY);
                if (gpp > 1e-6) {
                    const double level = e / interval;
                    const long long k = std::llround(level);
                    const double distM = std::abs(e - k * interval);
                    const double distPx = distM / gpp;
                    const bool index = (k % 5) == 0;
                    const double halfWidth = index ? 1.15 : 0.7;
                    double alpha = std::clamp((halfWidth + 0.75 - distPx) / 0.75, 0.0, 1.0);
                    if (distM > interval * 0.4) {
                        alpha = 0.0; // seam guard: never mark pixels far from the level
                    }
                    if (gpp > interval / 2.5) {
                        alpha *= (interval / 2.5) / gpp; // adjacent contours < ~2.5 px apart
                    }
                    if (flatFadeGpp > 0.0 && gpp < flatFadeGpp) {
                        alpha *= gpp / flatFadeGpp;
                    }
                    if (alpha > 0.0) {
                        alpha *= index ? pal.inkIndexA : pal.inkMinorA;
                        const int ir = index ? pal.inkIndexR : pal.inkMinorR;
                        const int ig = index ? pal.inkIndexG : pal.inkMinorG;
                        const int ib = index ? pal.inkIndexB : pal.inkMinorB;
                        fr = fr * (1.0 - alpha) + ir * alpha;
                        fg = fg * (1.0 - alpha) + ig * alpha;
                        fb = fb * (1.0 - alpha) + ib * alpha;
                    }
                }
            }

            line[x] =
                qRgba(std::clamp(static_cast<int>(fr), 0, 255), std::clamp(static_cast<int>(fg), 0, 255),
                      std::clamp(static_cast<int>(fb), 0, 255), 255);
        }
    }

    // Elevation labels on index contours (z >= 11), one per 128-px cell at most,
    // rotated along the contour, haloed in paper — the printed-map convention.
    const double indexInterval = interval * 5.0;
    if (interval > 0.0 && key.z >= 11) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QFont font(QStringLiteral("Arial"));
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        const QFontMetricsF fm(font);

        constexpr int kCell = 128;
        for (int cy = 0; cy < kSize / kCell; ++cy) {
            for (int cx = 0; cx < kSize / kCell; ++cx) {
                int bestX = -1;
                int bestY = -1;
                long long bestK = 0;
                double bestGx = 0.0;
                double bestGy = 0.0;
                double bestScore = 1e18;
                const int centerX = cx * kCell + kCell / 2;
                const int centerY = cy * kCell + kCell / 2;
                for (int y = cy * kCell + 14; y < (cy + 1) * kCell - 14; y += 2) {
                    for (int x = cx * kCell + 16; x < (cx + 1) * kCell - 16; x += 2) {
                        if (valid[static_cast<std::size_t>(y + kPad) * kGrid + (x + kPad)] == 0) {
                            continue;
                        }
                        auto at = [&](int dx, int dy) {
                            return static_cast<double>(
                                grid[static_cast<std::size_t>(y + kPad + dy) * kGrid + (x + kPad + dx)]);
                        };
                        const double e = at(0, 0);
                        if (e <= 0.0) {
                            continue;
                        }
                        const long long k = std::llround(e / indexInterval);
                        if (k <= 0) {
                            continue;
                        }
                        const double gx = (at(1, 0) - at(-1, 0)) / 2.0; // m per pixel
                        const double gy = (at(0, 1) - at(0, -1)) / 2.0;
                        const double gpp = std::hypot(gx, gy);
                        if (gpp < 0.18 || gpp > 12.0) {
                            continue; // too flat (label would float) or cliff-like
                        }
                        const double distPx = std::abs(e - k * indexInterval) / gpp;
                        if (distPx > 1.2) {
                            continue; // not sitting on the index contour
                        }
                        const double score = std::abs(x - centerX) + std::abs(y - centerY);
                        if (score < bestScore) {
                            bestScore = score;
                            bestX = x;
                            bestY = y;
                            bestK = k;
                            bestGx = gx;
                            bestGy = gy;
                        }
                    }
                }
                if (bestX < 0) {
                    continue;
                }
                // Along-contour direction = perpendicular to the gradient; keep upright.
                double angle = std::atan2(bestGy, bestGx) * 180.0 / M_PI + 90.0;
                if (angle > 90.0) {
                    angle -= 180.0;
                }
                if (angle < -90.0) {
                    angle += 180.0;
                }
                const QString text = QString::number(bestK * static_cast<long long>(indexInterval));
                const QRectF rect(-fm.horizontalAdvance(text) / 2.0 - 2.0, -fm.height() / 2.0,
                                  fm.horizontalAdvance(text) + 4.0, fm.height());
                painter.save();
                painter.translate(bestX, bestY);
                painter.rotate(angle);
                painter.setPen(pal.labelHalo);
                for (const QPointF off : {QPointF(-1, 0), QPointF(1, 0), QPointF(0, -1), QPointF(0, 1),
                                          QPointF(-1, -1), QPointF(1, 1), QPointF(-1, 1), QPointF(1, -1)}) {
                    painter.drawText(rect.translated(off), Qt::AlignCenter, text);
                }
                painter.setPen(pal.label);
                painter.drawText(rect, Qt::AlignCenter, text);
                painter.restore();
            }
        }
    }
    return image;
}

// --- MBTilesSource -----------------------------------------------------------

MBTilesSource::MBTilesSource(QObject* parent) : QObject(parent) {}

MBTilesSource::~MBTilesSource() {
    alive_->store(false);
    close();
}

bool MBTilesSource::open(const QString& path) {
    close();
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.toUtf8().constData(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        return false;
    }
    db_ = db;
    path_ = path;
    cache_.clear();
    // Zoom range and attribution from metadata when present.
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT name, value FROM metadata", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (name == nullptr || value == nullptr) {
                continue;
            }
            if (qstrcmp(name, "minzoom") == 0) {
                minZoom_ = QByteArray(value).toInt();
            } else if (qstrcmp(name, "maxzoom") == 0) {
                maxZoom_ = QByteArray(value).toInt();
            } else if (qstrcmp(name, "attribution") == 0) {
                attribution_ = QString::fromUtf8(value);
            }
        }
        sqlite3_finalize(stmt);
    }
    return true;
}

void MBTilesSource::close() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    cache_.clear();
    pending_.clear();
}

QImage MBTilesSource::tile(const TileKey& key) {
    if (db_ == nullptr) {
        return {};
    }
    if (QImage* cached = cache_.object(key)) {
        return *cached;
    }
    if (pending_.contains(key)) {
        return {};
    }
    pending_.insert(key);
    const QString path = path_;
    auto alive = alive_;
    QThreadPool::globalInstance()->start([this, alive, key, path] {
        // Independent connection per task: SQLite handles are not shared across threads.
        QImage image;
        sqlite3* db = nullptr;
        if (sqlite3_open_v2(path.toUtf8().constData(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db,
                                   "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? "
                                   "AND tile_row=?",
                                   -1, &stmt, nullptr) == SQLITE_OK) {
                const int tmsRow = (1 << key.z) - 1 - key.y; // MBTiles stores TMS rows
                sqlite3_bind_int(stmt, 1, key.z);
                sqlite3_bind_int(stmt, 2, key.x);
                sqlite3_bind_int(stmt, 3, tmsRow);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const void* blob = sqlite3_column_blob(stmt, 0);
                    const int size = sqlite3_column_bytes(stmt, 0);
                    image = QImage::fromData(static_cast<const uchar*>(blob), size);
                }
                sqlite3_finalize(stmt);
            }
            sqlite3_close(db);
        }
        if (!alive->load()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, key, image] {
                pending_.remove(key);
                const QImage stored = image.isNull() ? QImage(1, 1, QImage::Format_ARGB32) : image;
                cache_.insert(key, new QImage(stored), imageCost(stored));
                emit tileReady();
            },
            Qt::QueuedConnection);
    });
    return {};
}

// --- TopoTileSource ----------------------------------------------------------

TopoTileSource::TopoTileSource(tl::terrain::TerrainStore* store, QObject* parent)
    : QObject(parent), store_(store) {}

TopoTileSource::~TopoTileSource() {
    alive_->store(false);
}

void TopoTileSource::invalidate() {
    cache_.clear();
    pending_.clear();
}

QImage TopoTileSource::tile(const TileKey& key, bool darkTheme) {
    if (store_ == nullptr) {
        return {};
    }
    if (darkTheme != darkTheme_ || store_->revision() != storeRevision_) {
        darkTheme_ = darkTheme;
        storeRevision_ = store_->revision();
        invalidate();
    }
    if (QImage* cached = cache_.object(key)) {
        return *cached;
    }
    if (pending_.contains(key)) {
        return {};
    }
    pending_.insert(key);
    auto* store = store_;
    const bool dark = darkTheme;
    auto alive = alive_;
    QThreadPool::globalInstance()->start([this, alive, key, store, dark] {
        const QImage image = renderTopoTile(store, key, dark);
        if (!alive->load()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, key, image] {
                pending_.remove(key);
                cache_.insert(key, new QImage(image), imageCost(image));
                emit tileReady();
            },
            Qt::QueuedConnection);
    });
    return {};
}

// --- HttpTileSource ----------------------------------------------------------

QString HttpTileSource::cacheDirFor(const QString& id) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tiles/") + id;
}

HttpTileSource::HttpTileSource(const QString& id, const QString& urlTemplate, int maxZoom, QObject* parent)
    : QObject(parent), id_(id), urlTemplate_(urlTemplate), cacheDir_(cacheDirFor(id)), maxZoom_(maxZoom) {}

HttpTileSource::~HttpTileSource() {
    alive_->store(false);
}

QImage HttpTileSource::tile(const TileKey& key) {
    if (key.z > maxZoom_) {
        return {};
    }
    if (QImage* cached = cache_.object(key)) {
        return *cached;
    }
    if (pending_.contains(key)) {
        return {};
    }
    pending_.insert(key);

    // Disk cache first (works fully offline once an area has been browsed).
    const QString filePath =
        QStringLiteral("%1/%2/%3/%4.png").arg(cacheDir_).arg(key.z).arg(key.x).arg(key.y);
    auto alive = alive_;
    QThreadPool::globalInstance()->start([this, alive, key, filePath] {
        QImage image;
        if (QFile::exists(filePath)) {
            image.load(filePath);
        }
        if (!alive->load()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, key, image] {
                if (!image.isNull()) {
                    pending_.remove(key);
                    cache_.insert(key, new QImage(image), imageCost(image));
                    emit tileReady();
                } else {
#ifndef TROPOLINK_AIRGAP
                    // Not on disk: fetch. Stays "pending" until the reply lands.
                    fetchQueue_.enqueue(key);
                    pump();
#else
                    // Air-Gap flavor: no network code exists; the tile stays absent.
                    pending_.remove(key);
#endif
                }
            },
            Qt::QueuedConnection);
    });
    return {};
}

#ifndef TROPOLINK_AIRGAP
void HttpTileSource::pump() {
    constexpr int kConcurrent = 6;
    if (nam_ == nullptr) {
        nam_ = new QNetworkAccessManager(this);
    }
    while (inFlight_ < kConcurrent && !fetchQueue_.isEmpty()) {
        const TileKey key = fetchQueue_.dequeue();
        QString url = urlTemplate_;
        url.replace(QLatin1String("{z}"), QString::number(key.z));
        url.replace(QLatin1String("{x}"), QString::number(key.x));
        url.replace(QLatin1String("{y}"), QString::number(key.y));
        if (url.contains(QLatin1String("{s}"))) {
            static const char subs[] = "abc";
            url.replace(QLatin1String("{s}"), QChar::fromLatin1(subs[(key.x + key.y) % 3]));
        }
        QNetworkRequest request{QUrl(url)};
        // Tile-usage policies require an identifying agent.
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("TropoLink/1.0 (troposcatter link planner; offline cache)"));
        ++inFlight_;
        QNetworkReply* reply = nam_->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
            reply->deleteLater();
            --inFlight_;
            pending_.remove(key);
            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray data = reply->readAll();
                QImage image = QImage::fromData(data);
                if (!image.isNull()) {
                    // Persist to the disk cache for offline reuse.
                    const QString dir = QStringLiteral("%1/%2/%3").arg(cacheDir_).arg(key.z).arg(key.x);
                    QDir().mkpath(dir);
                    QFile file(dir + QStringLiteral("/%1.png").arg(key.y));
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(data);
                    }
                    cache_.insert(key, new QImage(image), imageCost(image));
                    emit tileReady();
                }
            }
            pump();
        });
    }
}
#else
void HttpTileSource::pump() {}
#endif
