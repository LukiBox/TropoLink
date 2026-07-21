#include "core/terrain/profile.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

namespace tl::terrain {

Meters Profile::elevationAt(Meters distance) const {
    if (points.empty()) {
        return Meters(0.0);
    }
    if (distance <= points.front().distance) {
        return points.front().elevation;
    }
    if (distance >= points.back().distance) {
        return points.back().elevation;
    }
    const auto it = std::lower_bound(points.begin(), points.end(), distance,
                                     [](const ProfilePoint& p, Meters d) { return p.distance < d; });
    const auto& hi = *it;
    const auto& lo = *(it - 1);
    const double span = (hi.distance - lo.distance).value();
    if (span <= 0.0) {
        return lo.elevation;
    }
    const double f = (distance - lo.distance).value() / span;
    return Meters(lo.elevation.value() * (1.0 - f) + hi.elevation.value() * f);
}

Meters Profile::meanElevation() const {
    if (points.empty()) {
        return Meters(0.0);
    }
    double sum = 0.0;
    for (const auto& p : points) {
        sum += p.elevation.value();
    }
    return Meters(sum / static_cast<double>(points.size()));
}

Expected<Profile> extractProfile(const TerrainStore& store, const ProfileRequest& request,
                                 std::stop_token stopToken, const ProgressCallback& progress) {
    const auto inverse = geo::Geodesy::inverse(request.siteA, request.siteB);
    if (inverse.distance.value() < 1.0) {
        return Error{"sites are co-located; no path to profile"};
    }

    // Step matched to the data: finest dataset covering either end, clamped to 30-90 m.
    Meters step = request.step;
    if (step.value() <= 0.0) {
        double finest = 90.0;
        for (const auto& e : store.entries()) {
            if (e.bounds.contains(request.siteA) || e.bounds.contains(request.siteB)) {
                finest = std::min(finest, e.resolutionM);
            }
        }
        step = Meters(std::clamp(finest, 30.0, 90.0));
    }

    // Hard cap on the sample count: beyond ~16k samples a finer step adds nothing to
    // horizon/mean-elevation accuracy but scales memory and time without bound (an
    // interactively dragged 2000 km path must not allocate 60k-sample profiles at
    // 120 ms cadence). The step widens instead; the cap still leaves <70 m posts at
    // 1000 km, finer than any global DEM's information content over such paths.
    constexpr int kMaxSamples = 16384;
    int sampleCount = std::max(2, static_cast<int>(std::ceil(inverse.distance.value() / step.value())) + 1);
    if (sampleCount > kMaxSamples) {
        sampleCount = kMaxSamples;
        step = Meters(inverse.distance.value() / (kMaxSamples - 1));
    }
    const auto line = geo::Geodesy::sampleLine(request.siteA, request.siteB, sampleCount);

    Profile profile;
    profile.step = Meters(inverse.distance.value() / static_cast<double>(sampleCount - 1));
    profile.totalDistance = inverse.distance;
    profile.points.resize(line.size());

    // Parallel sampling: fixed slot per index -> deterministic output.
    const unsigned workerCount = std::min<unsigned>(std::max(1U, std::thread::hardware_concurrency()), 16U);
    std::atomic<int> nextChunk{0};
    std::atomic<int> done{0};
    std::atomic<bool> cancelled{false};
    constexpr int kChunk = 256;
    const int chunkCount = (sampleCount + kChunk - 1) / kChunk;

    {
        std::vector<std::jthread> workers;
        workers.reserve(workerCount);
        for (unsigned w = 0; w < workerCount; ++w) {
            workers.emplace_back([&] {
                for (;;) {
                    if (stopToken.stop_requested()) {
                        cancelled.store(true, std::memory_order_relaxed);
                        return;
                    }
                    const int chunk = nextChunk.fetch_add(1, std::memory_order_relaxed);
                    if (chunk >= chunkCount) {
                        return;
                    }
                    const int begin = chunk * kChunk;
                    const int end = std::min(begin + kChunk, sampleCount);
                    for (int i = begin; i < end; ++i) {
                        const auto& s = line[static_cast<std::size_t>(i)];
                        ProfilePoint& out = profile.points[static_cast<std::size_t>(i)];
                        out.distance = s.distanceFromStart;
                        out.position = s.point;
                        const auto sample = store.elevationAt(s.point);
                        if (sample && !sample->isVoid) {
                            out.elevation = sample->elevation;
                            out.interpolatedVoid = false;
                        } else {
                            out.elevation = Meters(0.0);
                            out.interpolatedVoid = true; // includes "no coverage" points
                        }
                    }
                    const int total = done.fetch_add(end - begin, std::memory_order_relaxed) + (end - begin);
                    if (progress) {
                        progress(static_cast<double>(total) / static_cast<double>(sampleCount));
                    }
                }
            });
        }
    } // join

    if (cancelled.load() || stopToken.stop_requested()) {
        return Error{"profile extraction cancelled"};
    }

    // Coverage check and void interpolation. A point is a true DEM void (or gap) when
    // flagged above; we interpolate linearly between the nearest valid neighbours and
    // keep the flag so the UI can mark it.
    const auto validCount = std::count_if(profile.points.begin(), profile.points.end(),
                                          [](const ProfilePoint& p) { return !p.interpolatedVoid; });
    profile.hasCoverage = validCount > 0;
    if (!profile.hasCoverage) {
        profile.hasVoids = true;
        return profile; // all zeros, flagged; caller decides how to present this
    }

    const int n = static_cast<int>(profile.points.size());
    int i = 0;
    while (i < n) {
        if (!profile.points[static_cast<std::size_t>(i)].interpolatedVoid) {
            ++i;
            continue;
        }
        profile.hasVoids = true;
        const int gapBegin = i;
        while (i < n && profile.points[static_cast<std::size_t>(i)].interpolatedVoid) {
            ++i;
        }
        const int gapEnd = i; // one past
        const int left = gapBegin - 1;
        const int right = gapEnd; // valid or n
        const double leftElev = left >= 0 ? profile.points[static_cast<std::size_t>(left)].elevation.value()
                                          : profile.points[static_cast<std::size_t>(right)].elevation.value();
        const double rightElev =
            right < n ? profile.points[static_cast<std::size_t>(right)].elevation.value() : leftElev;
        const double leftDist = left >= 0
                                    ? profile.points[static_cast<std::size_t>(left)].distance.value()
                                    : profile.points[static_cast<std::size_t>(gapBegin)].distance.value();
        const double rightDist = right < n
                                     ? profile.points[static_cast<std::size_t>(right)].distance.value()
                                     : profile.points[static_cast<std::size_t>(gapEnd - 1)].distance.value();
        for (int g = gapBegin; g < gapEnd; ++g) {
            auto& p = profile.points[static_cast<std::size_t>(g)];
            if (rightDist > leftDist) {
                const double f = (p.distance.value() - leftDist) / (rightDist - leftDist);
                p.elevation = Meters(leftElev * (1.0 - f) + rightElev * f);
            } else {
                p.elevation = Meters(leftElev);
            }
        }
    }
    return profile;
}

} // namespace tl::terrain
