#pragma once

// Offline basemap downloader: fetches an area's XYZ raster tiles (OpenTopoMap or
// OSM) across a zoom range and packs them into a standard .mbtiles file that the
// map can open fully offline afterwards.
//
// Politeness: bounded tile count, few concurrent requests, paced dispatch, an
// identifying User-Agent, and reuse of the browsing disk cache (tiles already
// cached are packed without touching the network). The Air-Gap flavor compiles
// the network path out; start() then reports the feature absent.

#include <QObject>
#include <QQmlEngine>
#include <QQueue>
#include <QString>
#include <QTimer>

struct sqlite3;
class QNetworkAccessManager;

class MapDownloader : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int done READ done NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(int failed READ failed NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)

  public:
    explicit MapDownloader(QObject* parent = nullptr);
    ~MapDownloader() override;

    bool running() const { return running_; }
    int done() const { return done_; }
    int total() const { return total_; }
    int failed() const { return failed_; }
    QString statusText() const { return statusText_; }

    // Tile count for a bbox and zoom range (no side effects; QML binds the estimate).
    Q_INVOKABLE static double estimateTiles(double minLat, double maxLat, double minLon, double maxLon,
                                            int minZoom, int maxZoom);
    Q_INVOKABLE static QString defaultOutputDir();

    Q_INVOKABLE void start(const QString& sourceId, double minLat, double maxLat, double minLon,
                           double maxLon, int minZoom, int maxZoom, const QString& outputPath);
    Q_INVOKABLE void cancel();

  signals:
    void runningChanged();
    void progressChanged();
    void finished(const QString& path, bool ok);

  private:
    struct Job {
        int z = 0;
        int x = 0;
        int y = 0;
        int attempts = 0;
    };

    void setStatus(const QString& text);
    void finish(bool ok);
#ifndef TROPOLINK_AIRGAP
    void dispatch(); // start next fetch, paced
    void storeTile(const Job& job, const QByteArray& data);
#endif

    bool running_ = false;
    int done_ = 0;
    int total_ = 0;
    int failed_ = 0;
    QString statusText_;
    QString outputPath_;
    QString sourceId_;
    QString urlTemplate_;
    QString cacheDir_;
    QQueue<Job> queue_;
    int inFlight_ = 0;
    int sinceCommit_ = 0;
    bool cancelled_ = false;
    sqlite3* db_ = nullptr;
    QNetworkAccessManager* nam_ = nullptr;
    QTimer paceTimer_;
};
