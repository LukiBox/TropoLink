#pragma once

// Link budget: EIRP -> median received signal level -> noise floor (kTB + NF) ->
// median SNR -> fade margin. Every step is a named waterfall item for the cascade
// chart and the report table; the waterfall sums exactly by construction.

#include "core/budget/modulation.h"
#include "core/common/units.h"

#include <optional>
#include <string>
#include <vector>

namespace tl::budget {

struct RadioParams {
    Dbm txPower{57.0}; // 500 W
    Dbi antennaGainA{39.1};
    Dbi antennaGainB{39.1};
    Decibels lineLossA{0.5};
    Decibels lineLossB{0.5};
    Decibels noiseFigure{4.0};
    Hertz bandwidth = Hertz::fromMegahertz(2.0);
    // Required SNR either direct or from modulation + data rate.
    std::optional<Decibels> requiredSnrOverride;
    Modulation modulation{"QPSK", 2.0, Decibels(10.5), 0.35};
    BitsPerSecond dataRate = BitsPerSecond::fromMegabits(2.0);
    bool bandwidthFromModulation = true; // derive B from modulation + rate

    [[nodiscard]] Hertz effectiveBandwidth() const {
        return bandwidthFromModulation ? modulation.bandwidthFor(dataRate) : bandwidth;
    }
    [[nodiscard]] Decibels requiredSnr() const {
        return requiredSnrOverride ? *requiredSnrOverride : modulation.requiredSnr();
    }
};

struct WaterfallItem {
    std::string label; // stable identifier, localized at display time
    double valueDb;    // the step (+gain / -loss), or the running level for markers
    bool isLevel;      // true: absolute level marker (dBm); false: gain/loss step (dB)
};

struct LinkBudget {
    Dbm eirp{0.0};
    Decibels pathLoss{0.0}; // median basic transmission loss from the primary model
    Dbm medianRsl{0.0};
    Dbm noiseFloor{0.0};
    Decibels medianSnr{0.0};
    Decibels requiredSnr{0.0};
    Decibels fadeMargin{0.0};
    std::vector<WaterfallItem> waterfall;
};

[[nodiscard]] LinkBudget computeLinkBudget(const RadioParams& radio, Decibels medianPathLoss);

// Thermal noise floor: -174 dBm/Hz + 10 log10(B) + NF  (kT at 290 K).
[[nodiscard]] Dbm noiseFloorDbm(Hertz bandwidth, Decibels noiseFigure);

} // namespace tl::budget
