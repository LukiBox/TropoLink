#include "core/terrain/terrain_store.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace tl::terrain {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

fs::path utf8Path(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }

std::string toUtf8(const fs::path& p) {
    const std::u8string u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

// Per-thread cache of open GDAL handles, keyed by absolute path. GDAL handles are not
// thread-safe; a thread_local cache gives every worker its own handle set.
DemDataset* threadLocalDataset(const std::string& absolutePath) {
    thread_local std::unordered_map<std::string, std::unique_ptr<DemDataset>> cache;
    auto it = cache.find(absolutePath);
    if (it != cache.end()) {
        return it->second.get();
    }
    auto opened = DemDataset::open(absolutePath);
    if (!opened) {
        cache.emplace(absolutePath, nullptr);
        return nullptr;
    }
    auto [inserted, ok] = cache.emplace(absolutePath, std::move(opened).value());
    return inserted->second.get();
}

} // namespace

Expected<std::unique_ptr<TerrainStore>> TerrainStore::open(const std::string& directory) {
    std::error_code ec;
    fs::create_directories(utf8Path(directory), ec);
    if (ec) {
        return Error{"cannot create terrain store directory: " + directory};
    }
    auto store = std::unique_ptr<TerrainStore>(new TerrainStore(directory));
    if (auto s = store->loadIndex(); !s) {
        return s.error();
    }
    return store;
}

Status TerrainStore::loadIndex() {
    const fs::path indexPath = utf8Path(directory_) / "index.json";
    if (!fs::exists(indexPath)) {
        return Status::ok();
    }
    std::ifstream in(indexPath);
    if (!in) {
        return Error{"cannot read terrain index"};
    }
    json doc = json::parse(in, nullptr, false);
    if (doc.is_discarded()) {
        return Error{"terrain index is not valid JSON"};
    }
    std::unique_lock lock(mutex_);
    entries_.clear();
    for (const auto& e : doc.value("entries", json::array())) {
        StoreEntry entry;
        entry.fileName = e.value("file", "");
        entry.format = e.value("format", "");
        entry.provenance = e.value("provenance", "imported") == "downloaded" ? Provenance::Downloaded
                                                                             : Provenance::Imported;
        entry.bounds.minLat = e.value("minLat", 0.0);
        entry.bounds.maxLat = e.value("maxLat", 0.0);
        entry.bounds.minLon = e.value("minLon", 0.0);
        entry.bounds.maxLon = e.value("maxLon", 0.0);
        entry.resolutionM = e.value("resolutionM", 90.0);
        entry.fileSize = e.value("fileSize", std::uint64_t{0});
        if (!entry.fileName.empty()) {
            entries_.push_back(std::move(entry));
        }
    }
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const StoreEntry& a, const StoreEntry& b) { return a.resolutionM < b.resolutionM; });
    return Status::ok();
}

Status TerrainStore::saveIndex() const {
    json doc;
    doc["version"] = 1;
    json list = json::array();
    for (const auto& e : entries_) {
        json item;
        item["file"] = e.fileName;
        item["format"] = e.format;
        item["provenance"] = e.provenance == Provenance::Downloaded ? "downloaded" : "imported";
        item["minLat"] = e.bounds.minLat;
        item["maxLat"] = e.bounds.maxLat;
        item["minLon"] = e.bounds.minLon;
        item["maxLon"] = e.bounds.maxLon;
        item["resolutionM"] = e.resolutionM;
        item["fileSize"] = e.fileSize;
        list.push_back(std::move(item));
    }
    doc["entries"] = std::move(list);
    std::ofstream out(utf8Path(directory_) / "index.json", std::ios::trunc);
    if (!out) {
        return Error{"cannot write terrain index"};
    }
    out << doc.dump(2);
    return Status::ok();
}

Expected<StoreEntry> TerrainStore::importFile(const std::string& srcUtf8Path, Provenance provenance) {
    // Validate by opening before copying.
    auto probe = DemDataset::open(srcUtf8Path);
    if (!probe) {
        return probe.error();
    }
    const fs::path src = utf8Path(srcUtf8Path);
    const fs::path dst = utf8Path(directory_) / src.filename();
    std::error_code ec;
    if (!fs::equivalent(src, dst, ec)) {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return Error{"cannot copy into terrain store: " + ec.message()};
        }
    }

    StoreEntry entry;
    entry.fileName = toUtf8(src.filename());
    entry.format = probe.value()->driverName();
    entry.provenance = provenance;
    entry.bounds = probe.value()->bounds();
    entry.resolutionM = probe.value()->resolution().value();
    entry.fileSize = static_cast<std::uint64_t>(fs::file_size(dst, ec));

    {
        std::unique_lock lock(mutex_);
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&entry](const StoreEntry& e) { return e.fileName == entry.fileName; }),
                       entries_.end());
        entries_.push_back(entry);
        std::stable_sort(entries_.begin(), entries_.end(), [](const StoreEntry& a, const StoreEntry& b) {
            return a.resolutionM < b.resolutionM;
        });
        ++revision_;
        if (auto s = saveIndex(); !s) {
            return s.error();
        }
    }
    return entry;
}

Status TerrainStore::removeEntry(const std::string& fileName) {
    std::unique_lock lock(mutex_);
    const auto before = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&fileName](const StoreEntry& e) { return e.fileName == fileName; }),
                   entries_.end());
    if (entries_.size() == before) {
        return Error{"no such terrain entry: " + fileName};
    }
    std::error_code ec;
    fs::remove(utf8Path(directory_) / utf8Path(fileName), ec);
    ++revision_;
    return saveIndex();
}

std::vector<StoreEntry> TerrainStore::entries() const {
    std::shared_lock lock(mutex_);
    return entries_;
}

std::optional<ElevationSample> TerrainStore::elevationAt(const geo::GeoPoint& p) const {
    std::vector<std::string> candidates;
    {
        std::shared_lock lock(mutex_);
        for (const auto& e : entries_) {
            if (e.bounds.contains(p)) {
                candidates.push_back(toUtf8(utf8Path(directory_) / utf8Path(e.fileName)));
            }
        }
    }
    bool sawVoid = false;
    for (const auto& path : candidates) {
        DemDataset* ds = threadLocalDataset(path);
        if (ds == nullptr) {
            continue;
        }
        const auto sample = ds->sample(p);
        if (!sample) {
            continue;
        }
        if (!sample->isVoid) {
            return sample;
        }
        sawVoid = true;
    }
    if (sawVoid) {
        return ElevationSample{Meters(0.0), true};
    }
    return std::nullopt;
}

bool TerrainStore::covers(const geo::GeoPoint& p) const {
    std::shared_lock lock(mutex_);
    return std::any_of(entries_.begin(), entries_.end(),
                       [&p](const StoreEntry& e) { return e.bounds.contains(p); });
}

} // namespace tl::terrain
