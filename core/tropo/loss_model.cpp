#include "core/tropo/loss_model.h"

namespace tl::tropo {

Decibels LossModel::lossNotExceededWorstMonth(double percent) const {
    // "Loss not exceeded for pw% of the worst month" corresponds to an excess
    // percentage of (100 - pw)%. P.841 converts excess percentages.
    const double excessWm = 100.0 - percent;
    const auto params = WorstMonthParams::forTransHorizon(surfaceRefractivityNs());
    const double excessAnnual = worstMonthToAnnualExcess(excessWm, params);
    return lossNotExceededAnnual(100.0 - excessAnnual);
}

} // namespace tl::tropo
