#include "ui/map/MapDownloader.h"

#include "ui/map/MapSources.h"

#include <sqlite3.h>

#ifndef TROPOLINK_AIRGAP
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

// Hard ceiling on one download job. ~30k tiles ≈ 600 MB — enough for a mission
// area at z12-14; anything larger should come as a prepared MBTiles pack.
constexpr int kMaxTiles = 30000;
constexpr int kConcurrent = 4;
constexpr int kPaceMs = 40; // >= 40 ms between dispatches: max ~25 req/s

int lonToTileX(double lon, int z) {
    return static_cast<int>(std::floor((lon + 180.0) / 360.0 * (1 << z)));
}
int latToTileY(double lat, int z) {
    const double rad = lat * M_PI / 180.0;
    const double y = (1.0 - std::asinh(std::tan(rad)) / M_PI) / 2.0;
    return static_cast<int>(std::floor(y * (1 << z)));
}

struct TileRange {
    int x0, x1, y0, y1; // inclusive
};

TileRange rangeFor(double minLat, double maxLat, double minLon, double maxLon, int z) {
    const int n = (1 << z) - 1;
    TileRange r{};
    r.x0 = std::clamp(lonToTileX(std::min(minLon, maxLon), z), 0, n);
    r.x1 = std::clamp(lonToTileX(std::max(minLon, maxLon), z), 0, n);
    r.y0 = std::clamp(latToTileY(std::max(minLat, maxLat), z), 0, n); // north = smaller y
    r.y1 = std::clamp(latToTileY(std::min(minLat, maxLat), z), 0, n);
    return r;
}

} // namespace

MapDownloader::MapDownloader(QObject* parent) : QObject(parent) {
    paceTimer_.setInterval(kPaceMs);
#ifndef TROPOLINK_AIRGAP
    connect(&paceTimer_, &QTimer::timeout, this, [this] { dispatch(); });
#endif
}

MapDownloader::~MapDownloader() {
    cancelled_ = true;
    if (db_ != nullptr) {
        sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

double MapDownloader::estimateTiles(double minLat, double maxLat, double minLon, double maxLon, int minZoom,
                                    int maxZoom) {
    double count = 0.0;
    for (int z = std::max(0, minZoom); z <= maxZoom; ++z) {
        const TileRange r = rangeFor(minLat, maxLat, minLon, maxLon, z);
        count += static_cast<double>(r.x1 - r.x0 + 1) * static_cast<double>(r.y1 - r.y0 + 1);
    }
    return count;
}

QString MapDownloader::defaultOutputDir() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/maps");
    QDir().mkpath(dir);
    return dir;
}

void MapDownloader::setStatus(const QString& text) {
    statusText_ = text;
    emit progressChanged();
}

void MapDownloader::cancel() {
    if (!running_) {
        return;
    }
    cancelled_ = true;
    setStatus(tr("Cancelling..."));
}

void MapDownloader::finish(bool ok) {
    paceTimer_.stop();
    if (db_ != nullptr) {
        sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
        sqlite3_close(db_);
        db_ = nullptr;
    }
    running_ = false;
    emit runningChanged();
    emit finished(outputPath_, ok);
}

#ifndef TROPOLINK_AIRGAP

void MapDownloader::start(const QString& sourceId, double minLat, double maxLat, double minLon, double maxLon,
                          int minZoom, int maxZoom, const QString& outputPath) {
    if (running_) {
        return;
    }
    sourceId_ = sourceId;
    if (sourceId == QLatin1String("osm")) {
        urlTemplate_ = QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png");
        maxZoom = std::min(maxZoom, 19);
    } else {
        sourceId_ = QStringLiteral("opentopomap");
        urlTemplate_ = QStringLiteral("https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png");
        maxZoom = std::min(maxZoom, 17);
    }
    cacheDir_ = HttpTileSource::cacheDirFor(sourceId_);

    queue_.clear();
    for (int z = std::max(0, minZoom); z <= maxZoom; ++z) {
        const TileRange r = rangeFor(minLat, maxLat, minLon, maxLon, z);
        for (int y = r.y0; y <= r.y1; ++y) {
            for (int x = r.x0; x <= r.x1; ++x) {
                queue_.enqueue(Job{z, x, y, 0});
            }
        }
    }
    total_ = queue_.size();
    done_ = 0;
    failed_ = 0;
    if (total_ == 0) {
        setStatus(tr("Nothing to download for this area"));
        return;
    }
    if (total_ > kMaxTiles) {
        setStatus(tr("%1 tiles requested; the limit is %2. Reduce the area or zoom range.")
                      .arg(total_)
                      .arg(kMaxTiles));
        queue_.clear();
        total_ = 0;
        emit progressChanged();
        return;
    }

    outputPath_ = outputPath;
    QDir().mkpath(QFileInfo(outputPath).absolutePath());
    QFile::remove(outputPath); // fresh pack
    if (sqlite3_open(outputPath.toUtf8().constData(), &db_) != SQLITE_OK) {
        setStatus(tr("Cannot create %1").arg(outputPath));
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return;
    }
    sqlite3_exec(db_,
                 "CREATE TABLE metadata (name TEXT, value TEXT);"
                 "CREATE TABLE tiles (zoom_level INTEGER, tile_column INTEGER, "
                 "tile_row INTEGER, tile_data BLOB);"
                 "CREATE UNIQUE INDEX tile_index ON tiles (zoom_level, tile_column, tile_row);",
                 nullptr, nullptr, nullptr);
    const auto meta = [this](const char* name, const QString& value) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "INSERT INTO metadata VALUES (?, ?)", -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
            const QByteArray utf8 = value.toUtf8();
            sqlite3_bind_text(stmt, 2, utf8.constData(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    };
    meta("name", QStringLiteral("TropoLink %1 pack").arg(sourceId_));
    meta("type", QStringLiteral("baselayer"));
    meta("format", QStringLiteral("png"));
    meta("minzoom", QString::number(minZoom));
    meta("maxzoom", QString::number(maxZoom));
    meta("bounds", QStringLiteral("%1,%2,%3,%4")
                       .arg(std::min(minLon, maxLon))
                       .arg(std::min(minLat, maxLat))
                       .arg(std::max(minLon, maxLon))
                       .arg(std::max(minLat, maxLat)));
    meta("attribution",
         sourceId_ == QLatin1String("osm")
             ? QStringLiteral("\xC2\xA9 OpenStreetMap contributors")
             : QStringLiteral("\xC2\xA9 OpenStreetMap contributors, SRTM | style \xC2\xA9 OpenTopoMap "
                              "(CC-BY-SA)"));
    sqlite3_exec(db_, "BEGIN", nullptr, nullptr, nullptr);
    sinceCommit_ = 0;

    if (nam_ == nullptr) {
        nam_ = new QNetworkAccessManager(this);
    }
    cancelled_ = false;
    running_ = true;
    inFlight_ = 0;
    emit runningChanged();
    setStatus(tr("Downloading %1 tiles...").arg(total_));
    paceTimer_.start();
    dispatch();
}

void MapDownloader::storeTile(const Job& job, const QByteArray& data) {
    if (db_ == nullptr) {
        return;
    }
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT OR REPLACE INTO tiles VALUES (?, ?, ?, ?)", -1, &stmt, nullptr) ==
        SQLITE_OK) {
        const int tmsRow = (1 << job.z) - 1 - job.y; // MBTiles stores TMS rows
        sqlite3_bind_int(stmt, 1, job.z);
        sqlite3_bind_int(stmt, 2, job.x);
        sqlite3_bind_int(stmt, 3, tmsRow);
        sqlite3_bind_blob(stmt, 4, data.constData(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (++sinceCommit_ >= 256) {
        sqlite3_exec(db_, "COMMIT; BEGIN", nullptr, nullptr, nullptr);
        sinceCommit_ = 0;
    }
}

void MapDownloader::dispatch() {
    if (!running_) {
        return;
    }
    if (cancelled_) {
        if (inFlight_ == 0) {
            setStatus(tr("Cancelled: %1 of %2 tiles saved").arg(done_).arg(total_));
            finish(false);
        }
        return;
    }
    if (queue_.isEmpty()) {
        if (inFlight_ == 0) {
            setStatus(failed_ == 0 ? tr("Done: %1 tiles").arg(done_)
                                   : tr("Done: %1 tiles, %2 failed").arg(done_).arg(failed_));
            finish(true);
        }
        return;
    }
    // One dispatch per pace tick keeps the request rate tile-server friendly.
    if (inFlight_ >= kConcurrent) {
        return;
    }
    Job job = queue_.dequeue();

    // Browsing cache first: already-fetched tiles are packed without network.
    const QString cachedPath =
        QStringLiteral("%1/%2/%3/%4.png").arg(cacheDir_).arg(job.z).arg(job.x).arg(job.y);
    if (QFile::exists(cachedPath)) {
        QFile file(cachedPath);
        if (file.open(QIODevice::ReadOnly)) {
            storeTile(job, file.readAll());
            ++done_;
            emit progressChanged();
            dispatch(); // cache hits don't need pacing
            return;
        }
    }

    QString url = urlTemplate_;
    url.replace(QLatin1String("{z}"), QString::number(job.z));
    url.replace(QLatin1String("{x}"), QString::number(job.x));
    url.replace(QLatin1String("{y}"), QString::number(job.y));
    if (url.contains(QLatin1String("{s}"))) {
        static const char subs[] = "abc";
        url.replace(QLatin1String("{s}"), QChar::fromLatin1(subs[(job.x + job.y) % 3]));
    }
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("TropoLink/1.0 (troposcatter link planner; offline cache)"));
    ++inFlight_;
    QNetworkReply* reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job]() mutable {
        reply->deleteLater();
        --inFlight_;
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                storeTile(job, data);
                // Feed the browsing cache too: the map benefits immediately.
                const QString dir = QStringLiteral("%1/%2/%3").arg(cacheDir_).arg(job.z).arg(job.x);
                QDir().mkpath(dir);
                QFile file(dir + QStringLiteral("/%1.png").arg(job.y));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                }
                ++done_;
            } else {
                ++failed_;
            }
        } else if (job.attempts < 1 && !cancelled_) {
            ++job.attempts;
            queue_.enqueue(job); // one retry, at the back
        } else {
            ++failed_;
        }
        if ((done_ + failed_) % 25 == 0 || queue_.isEmpty()) {
            setStatus(tr("Downloading: %1 / %2 (failed: %3)").arg(done_).arg(total_).arg(failed_));
        } else {
            emit progressChanged();
        }
        dispatch();
    });
}

#else // TROPOLINK_AIRGAP

void MapDownloader::start(const QString&, double, double, double, double, int, int, const QString&) {
    setStatus(tr("The map downloader is not present in the Air-Gap build"));
}

#endif
