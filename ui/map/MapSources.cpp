#include "ui/map/MapSources.h"

#include <sqlite3.h>

#include <QThreadPool>
#include <QtMath>

#include <cmath>
#include <cstdint>

namespace {

// Web-mercator tile bounds in degrees.
double tileLon(int x, int z) { return x / std::pow(2.0, z) * 360.0 - 180.0; }
double tileLat(int y, int z) {
    const double n = M_PI - 2.0 * M_PI * y / std::pow(2.0, z);
    return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

} // namespace

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
    // Zoom range from metadata when present.
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
                cache_.insert(key, new QImage(image.isNull() ? QImage(1, 1, QImage::Format_ARGB32)
                                                             : image));
                emit tileReady();
            },
            Qt::QueuedConnection);
    });
    return {};
}

// --- HillshadeSource ---------------------------------------------------------

HillshadeSource::HillshadeSource(tl::terrain::TerrainStore* store, QObject* parent)
    : QObject(parent), store_(store) {}

HillshadeSource::~HillshadeSource() { alive_->store(false); }

void HillshadeSource::invalidate() {
    cache_.clear();
    pending_.clear();
}

QImage HillshadeSource::tile(const TileKey& key, bool darkTheme) {
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
        constexpr int kSize = 256;
        QImage image(kSize, kSize, QImage::Format_ARGB32);
        const double lon0 = tileLon(key.x, key.z);
        const double lon1 = tileLon(key.x + 1, key.z);
        const double lat0 = tileLat(key.y, key.z);     // north edge
        const double lat1 = tileLat(key.y + 1, key.z); // south edge

        // Sample an elevation grid one texel wider for the gradient; track validity so
        // uncovered pixels stay transparent instead of pretending to be sea level.
        std::vector<float> grid(static_cast<std::size_t>((kSize + 2) * (kSize + 2)));
        std::vector<std::uint8_t> valid(static_cast<std::size_t>((kSize + 2) * (kSize + 2)), 0);
        bool anyData = false;
        for (int gy = 0; gy < kSize + 2; ++gy) {
            const double lat = lat0 + (lat1 - lat0) * (gy - 0.5) / kSize;
            for (int gx = 0; gx < kSize + 2; ++gx) {
                const double lon = lon0 + (lon1 - lon0) * (gx - 0.5) / kSize;
                float v = 0.0f;
                const auto sample =
                    store->elevationAt(tl::geo::GeoPoint{tl::Degrees(lat), tl::Degrees(lon)});
                const std::size_t idx = static_cast<std::size_t>(gy) * (kSize + 2) + gx;
                if (sample && !sample->isVoid) {
                    v = static_cast<float>(sample->elevation.value());
                    valid[idx] = 1;
                    anyData = true;
                }
                grid[idx] = v;
            }
        }

        if (!anyData) {
            image.fill(Qt::transparent);
        } else {
            // Horn's method; sun from 315 deg azimuth, 45 deg elevation.
            const double metersPerPixelY =
                std::abs(lat1 - lat0) * 111320.0 / kSize;
            const double metersPerPixelX = std::abs(lon1 - lon0) * 111320.0 *
                                           std::cos((lat0 + lat1) * 0.5 * M_PI / 180.0) / kSize;
            const double azimuth = 315.0 * M_PI / 180.0;
            const double altitude = 45.0 * M_PI / 180.0;
            for (int y = 0; y < kSize; ++y) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < kSize; ++x) {
                    if (valid[static_cast<std::size_t>(y + 1) * (kSize + 2) + (x + 1)] == 0) {
                        line[x] = qRgba(0, 0, 0, 0); // no coverage: transparent
                        continue;
                    }
                    auto at = [&](int dx, int dy) {
                        return static_cast<double>(
                            grid[static_cast<std::size_t>(y + 1 + dy) * (kSize + 2) + (x + 1 + dx)]);
                    };
                    const double dzdx = ((at(1, -1) + 2 * at(1, 0) + at(1, 1)) -
                                         (at(-1, -1) + 2 * at(-1, 0) + at(-1, 1))) /
                                        (8.0 * metersPerPixelX);
                    const double dzdy = ((at(-1, 1) + 2 * at(0, 1) + at(1, 1)) -
                                         (at(-1, -1) + 2 * at(0, -1) + at(1, -1))) /
                                        (8.0 * metersPerPixelY);
                    const double slope = std::atan(1.3 * std::hypot(dzdx, dzdy));
                    const double aspect = std::atan2(dzdy, -dzdx);
                    double shade = std::sin(altitude) * std::cos(slope) +
                                   std::cos(altitude) * std::sin(slope) * std::cos(azimuth - aspect);
                    shade = std::clamp(shade, 0.0, 1.0);
                    // Restrained, NATO-ish relief: near-neutral ramp tinted by theme.
                    int base = dark ? 38 + static_cast<int>(shade * 96)
                                    : 140 + static_cast<int>(shade * 100);
                    const int elevTint =
                        std::clamp(static_cast<int>(at(0, 0) / 40.0), 0, 24);
                    const int r = std::clamp(base + elevTint, 0, 255);
                    const int g = std::clamp(base + elevTint / 2, 0, 255);
                    const int b = std::clamp(base, 0, 255);
                    line[x] = qRgba(r, g, b, 255);
                }
            }
        }
        if (!alive->load()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, key, image] {
                pending_.remove(key);
                cache_.insert(key, new QImage(image));
                emit tileReady();
            },
            Qt::QueuedConnection);
    });
    return {};
}
