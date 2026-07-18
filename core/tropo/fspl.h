#pragma once

// Free-space path loss, the reference baseline:
//     FSPL = 32.45 + 20 log10(f_MHz) + 20 log10(d_km)   dB
// (ITU-R P.525-4, eq. for frequency in MHz and distance in km.)

#include "core/common/units.h"

#include <cmath>

namespace tl::tropo {

[[nodiscard]] inline Decibels freeSpacePathLoss(Hertz frequency, Meters distance) {
    return Decibels(32.45 + 20.0 * std::log10(frequency.megahertz()) +
                    20.0 * std::log10(distance.kilometers()));
}

} // namespace tl::tropo
