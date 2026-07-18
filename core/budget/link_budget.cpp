#include "core/budget/link_budget.h"

#include <cmath>

namespace tl::budget {

Dbm noiseFloorDbm(Hertz bandwidth, Decibels noiseFigure) {
    return Dbm(-174.0 + 10.0 * std::log10(bandwidth.value())) + noiseFigure;
}

LinkBudget computeLinkBudget(const RadioParams& radio, Decibels medianPathLoss) {
    LinkBudget out;
    out.pathLoss = medianPathLoss;
    out.eirp = radio.txPower - radio.lineLossA + radio.antennaGainA;
    out.medianRsl = out.eirp - medianPathLoss + radio.antennaGainB - radio.lineLossB;
    out.noiseFloor = noiseFloorDbm(radio.effectiveBandwidth(), radio.noiseFigure);
    out.medianSnr = out.medianRsl - out.noiseFloor;
    out.requiredSnr = radio.requiredSnr();
    out.fadeMargin = out.medianSnr - out.requiredSnr;

    out.waterfall = {
        {"tx_power", radio.txPower.value(), true},
        {"line_loss_tx", -radio.lineLossA.value(), false},
        {"antenna_gain_tx", radio.antennaGainA.value(), false},
        {"eirp", out.eirp.value(), true},
        {"path_loss", -medianPathLoss.value(), false},
        {"antenna_gain_rx", radio.antennaGainB.value(), false},
        {"line_loss_rx", -radio.lineLossB.value(), false},
        {"rsl", out.medianRsl.value(), true},
        {"noise_floor", out.noiseFloor.value(), true},
        {"median_snr", out.medianSnr.value(), false},
        {"required_snr", out.requiredSnr.value(), false},
        {"fade_margin", out.fadeMargin.value(), false},
    };
    return out;
}

} // namespace tl::budget
