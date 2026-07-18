#include "core/tropo/model_suite.h"

#include "core/tropo/fspl.h"

#include <algorithm>
#include <cmath>

namespace tl::tropo {

SuiteResult runSuite(const SuiteInput& input, const terrain::Profile& profile, ModelId primary) {
    SuiteResult out;
    out.primary = primary;
    out.inverse = geo::Geodesy::inverse(input.siteA, input.siteB);
    out.terrainAvailable = profile.hasCoverage;
    out.profileHasVoids = profile.hasVoids;

    terrain::HorizonRequest hreq;
    hreq.antennaHeightAglA = input.antennaAglA;
    hreq.antennaHeightAglB = input.antennaAglB;
    hreq.kFactor = input.atmosphere.kFactor;
    hreq.frequency = input.frequency;
    out.horizons = terrain::analyzeHorizons(profile, hreq);

    const Meters siteElevA = profile.points.empty() ? Meters(0.0) : profile.points.front().elevation;
    const Meters siteElevB = profile.points.empty() ? Meters(0.0) : profile.points.back().elevation;

    ScatterGeometryInput gin;
    gin.pathLength = out.inverse.distance;
    gin.takeoffA = out.horizons.takeoffAngleA;
    gin.takeoffB = out.horizons.takeoffAngleB;
    gin.antennaAmslA = siteElevA + input.antennaAglA;
    gin.antennaAmslB = siteElevB + input.antennaAglB;
    gin.kFactor = input.atmosphere.kFactor;
    gin.gainA = input.gainA;
    gin.gainB = input.gainB;
    out.geometry = computeScatterGeometry(gin);

    // Geographic position of the common volume and its height above local terrain.
    const auto samples = geo::Geodesy::sampleLine(input.siteA, input.siteB, 3);
    const double frac = out.geometry.distanceToVolumeA.value() /
                        std::max(1.0, out.inverse.distance.value());
    out.commonVolumePosition =
        geo::Geodesy::direct(input.siteA, out.inverse.forwardAzimuth,
                             Meters(out.inverse.distance.value() * frac));
    out.commonVolumeAboveTerrain =
        out.geometry.volumeBaseAmsl - profile.elevationAt(out.geometry.distanceToVolumeA);

    out.fspl = freeSpacePathLoss(input.frequency, out.inverse.distance);

    // --- FSPL row (reference baseline) ---------------------------------------
    {
        ModelRow row;
        row.id = ModelId::Fspl;
        row.name = "FSPL (reference)";
        row.citation = "ITU-R P.525-4";
        row.valid = true;
        row.median = out.fspl;
        row.annualDb.fill(out.fspl.value());
        row.worstMonthDb.fill(out.fspl.value());
        row.note = "free-space baseline, not a troposcatter prediction";
        out.rows.push_back(row);
    }

    const double meanNs =
        input.atmosphere.surfaceRefractivityAt(profile.hasCoverage ? profile.meanElevation() : Meters(0.0));

    // --- P.617 ----------------------------------------------------------------
    {
        P617Params p;
        p.frequency = input.frequency;
        p.pathLength = out.inverse.distance;
        p.takeoffA = out.horizons.takeoffAngleA;
        p.takeoffB = out.horizons.takeoffAngleB;
        p.gainA = input.gainA;
        p.gainB = input.gainB;
        p.seaLevelN0 = input.atmosphere.seaLevelN0;
        p.lapseRateDn = input.atmosphere.lapseRateDn;
        p.kFactor = input.atmosphere.kFactor;
        p.surfaceHeightAmsl = profile.hasCoverage ? profile.meanElevation() : Meters(0.0);
        p.antennaAmslA = gin.antennaAmslA;
        p.antennaAmslB = gin.antennaAmslB;
        p.terrainAvailable = profile.hasCoverage;
        out.models[ModelId::P617] = std::make_shared<P617Model>(p);
    }

    // --- TN101 ----------------------------------------------------------------
    {
        Tn101Params t;
        t.frequency = input.frequency;
        t.pathLength = out.inverse.distance;
        t.takeoffA = out.horizons.takeoffAngleA;
        t.takeoffB = out.horizons.takeoffAngleB;
        t.horizonDistanceA = out.horizons.horizonDistanceA;
        t.horizonDistanceB = out.horizons.horizonDistanceB;
        t.antennaHeightAglA = input.antennaAglA;
        t.antennaHeightAglB = input.antennaAglB;
        t.gainA = input.gainA;
        t.gainB = input.gainB;
        t.surfaceRefractivityNs = meanNs;
        t.kFactor = input.atmosphere.kFactor;
        t.climate = static_cast<int>(input.atmosphere.climate);
        t.terrainIrregularityDeltaH = interdecileDeltaH(profile);
        t.terrainAvailable = profile.hasCoverage;
        out.models[ModelId::Tn101] = std::make_shared<Tn101Model>(t);
    }

    // --- ITM ------------------------------------------------------------------
    {
        ItmParams i;
        i.frequency = input.frequency;
        i.antennaHeightAglA = input.antennaAglA;
        i.antennaHeightAglB = input.antennaAglB;
        i.gainA = input.gainA;
        i.gainB = input.gainB;
        i.seaLevelN0 = input.atmosphere.seaLevelN0;
        i.climate = static_cast<int>(input.atmosphere.climate);
        auto itm = std::make_shared<ItmModel>(profile, i);
        out.models[ModelId::Itm] = itm;
    }

    // --- comparison rows + spread --------------------------------------------
    double minMedian = 1e18;
    double maxMedian = -1e18;
    for (const auto id : {ModelId::P617, ModelId::Tn101, ModelId::Itm}) {
        const auto& model = out.models.at(id);
        ModelRow row;
        row.id = id;
        row.name = model->name();
        row.citation = model->citation();
        const auto validity = model->validity();
        row.valid = validity.valid;
        row.validityIssues = validity.issues;
        row.couplingLoss = model->couplingLoss();
        if (id == ModelId::Itm) {
            row.note = "mode: " + std::static_pointer_cast<const ItmModel>(model)->propagationMode();
        }
        if (row.valid) {
            row.median = model->medianLoss();
            for (std::size_t k = 0; k < kStandardPercentiles.size(); ++k) {
                row.annualDb[k] = model->lossNotExceededAnnual(kStandardPercentiles[k]).value();
                row.worstMonthDb[k] = model->lossNotExceededWorstMonth(kStandardPercentiles[k]).value();
            }
            minMedian = std::min(minMedian, row.median.value());
            maxMedian = std::max(maxMedian, row.median.value());
        }
        out.rows.push_back(std::move(row));
    }
    out.spread = Decibels(maxMedian >= minMedian ? maxMedian - minMedian : 0.0);
    return out;
}

} // namespace tl::tropo
