#pragma once

// Editable modulation library. Required Eb/N0 values are uncoded theoretical
// references at BER 1e-6 (Proakis, Digital Communications); operators edit the JSON
// (resources/data/modulations.json) to match their actual modem data sheets.

#include "core/common/expected.h"
#include "core/common/units.h"

#include <string>
#include <vector>

namespace tl::budget {

struct Modulation {
    std::string name;
    double bitsPerSymbol = 2.0;
    Decibels requiredEbN0{10.5}; // at reference BER
    double rolloff = 0.35;       // occupied bandwidth = symbol rate * (1 + rolloff)

    // Occupied bandwidth for a given data rate.
    [[nodiscard]] Hertz bandwidthFor(BitsPerSecond rate) const {
        return Hertz(rate.value() / bitsPerSymbol * (1.0 + rolloff));
    }
    // Required SNR in the occupied bandwidth:
    //   SNR = Eb/N0 + 10 log10(Rb / B) = Eb/N0 + 10 log10(bitsPerSymbol / (1 + rolloff))
    [[nodiscard]] Decibels requiredSnr() const;
};

class ModulationLibrary {
public:
    // Built-in defaults (BPSK, QPSK, 8PSK, 16QAM, 64QAM).
    [[nodiscard]] static ModulationLibrary builtIn();
    // Load from JSON; falls back to built-in entries on error.
    [[nodiscard]] static Expected<ModulationLibrary> load(const std::string& utf8Path);

    [[nodiscard]] const std::vector<Modulation>& entries() const { return entries_; }
    [[nodiscard]] const Modulation* find(const std::string& name) const;

private:
    std::vector<Modulation> entries_;
};

} // namespace tl::budget
