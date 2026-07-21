#pragma once

// Auto-design: given the path that the two pins define, search the equipment
// configuration that closes the link most comfortably.
//
// This is a search over REAL candidate hardware, scored with the same ITU-R
// P.617-5 model the rest of the application uses — not a closed-form guess. For
// every candidate (frequency, dish diameter) pair it builds the actual loss model
// for this geometry, derives the fade margin the target availability demands, and
// inverts the link budget for the transmit power that would be required:
//
//   Ptx = margin + Lline,tx + Lline,rx - Gt - Gr + L_median + N + SNR_req
//
// The winner is the configuration needing the least transmit power, with ties
// broken toward the smaller antenna and the lower frequency (both cheaper to field).
//
// The point of sweeping rather than solving analytically is the aperture-to-medium
// coupling loss (P.617-5 eq. 3): Lc grows exponentially with total gain, so a bigger
// dish stops helping and then starts hurting. The optimum is real, geometry- and
// frequency-dependent, and this search finds it instead of assuming "bigger is
// better" — the classic troposcatter beginner trap.

#include "core/budget/availability.h"
#include "core/budget/link_budget.h"
#include "core/budget/modulation.h"
#include "core/geo/atmosphere.h"

#include <string>
#include <vector>

namespace tl::budget {

struct AutoDesignConstraints {
    Percent targetAvailability{99.9};
    bool worstMonth = false;
    DiversityMode diversity = DiversityMode::Quad;
    BitsPerSecond dataRate = BitsPerSecond::fromMegabits(2.0);
    Dbm maxTxPower{60.0}; // 1 kW: a realistic tactical SSPA ceiling
    Meters maxAntennaDiameter{4.6};
    Decibels lineLossA{0.5};
    Decibels lineLossB{0.5};
    Decibels noiseFigure{4.0};
    double apertureEfficiency = 0.55; // typical prime-focus troposcatter dish
    // Headroom added on top of the availability requirement, so the delivered design
    // is not sitting exactly on its threshold.
    Decibels designHeadroom{3.0};
};

// One field the solver decided to change, with the reason as a stable key the UI
// and report translate (never English prose baked into the core).
struct AutoDesignChange {
    std::string field;    // "frequency" | "diameter" | "gain" | "txPower" | "modulation"
    std::string oldValue; // preformatted for display
    std::string newValue;
    std::string reasonKey; // "reason_freq_gain" | "reason_coupling_optimum" | ...
};

struct AutoDesignResult {
    bool feasible = false;
    std::string noteKey; // "auto_ok" | "auto_infeasible" | "auto_no_model"

    // The chosen configuration.
    Hertz frequency = Hertz::fromGigahertz(4.4);
    Meters antennaDiameter{3.0};
    Dbi antennaGain{39.1};
    Dbm txPower{57.0};
    int modulationIndex = 0;

    // What it achieves, for the summary.
    Decibels medianLoss{0.0};
    Decibels couplingLoss{0.0};
    Decibels fadeMargin{0.0};
    Percent availabilityAnnual{0.0};
    Percent availabilityWorstMonth{0.0};
    // Best alternative that was rejected, to justify the coupling-loss optimum.
    Meters rejectedLargerDiameter{0.0};
    Decibels rejectedLargerPenaltyDb{0.0};

    std::vector<AutoDesignChange> changes;
};

// The part of the path that does NOT depend on the radio choice: it is fixed by
// where the two pins are and what the terrain does between them.
struct AutoDesignGeometry {
    Meters pathLength{0.0};
    Radians takeoffA{0.0};
    Radians takeoffB{0.0};
    Meters surfaceHeightAmsl{0.0};
    Meters antennaAmslA{0.0};
    Meters antennaAmslB{0.0};
    bool terrainAvailable = false;
};

// Antenna gain of a circular aperture: G = eta (pi D f / c)^2.
[[nodiscard]] Dbi apertureGain(Meters diameter, Hertz frequency, double efficiency);

// Runs the search over candidate frequencies, dish diameters and modulations.
[[nodiscard]] AutoDesignResult autoDesign(const AutoDesignGeometry& geometry,
                                          const geo::Atmosphere& atmosphere,
                                          const ModulationLibrary& modulations, const RadioParams& current,
                                          Hertz currentFrequency, Meters currentDiameter,
                                          const AutoDesignConstraints& constraints);

} // namespace tl::budget
