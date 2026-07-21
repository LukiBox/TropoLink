#include "core/budget/modulation.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>

namespace tl::budget {

Decibels Modulation::requiredSnr() const {
    return requiredEbN0 + Decibels(10.0 * std::log10(bitsPerSymbol / (1.0 + rolloff)));
}

ModulationLibrary ModulationLibrary::builtIn() {
    ModulationLibrary lib;
    lib.entries_ = {
        {"BPSK", 1.0, Decibels(10.5), 0.35},  {"QPSK", 2.0, Decibels(10.5), 0.35},
        {"8PSK", 3.0, Decibels(14.0), 0.35},  {"16QAM", 4.0, Decibels(14.4), 0.35},
        {"64QAM", 6.0, Decibels(18.8), 0.35},
    };
    return lib;
}

Expected<ModulationLibrary> ModulationLibrary::load(const std::string& utf8Path) {
    std::ifstream in(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())));
    if (!in) {
        return Error{"cannot open modulation library: " + utf8Path};
    }
    nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
    if (doc.is_discarded()) {
        return Error{"modulation library is not valid JSON: " + utf8Path};
    }
    ModulationLibrary lib;
    for (const auto& e : doc.value("modulations", nlohmann::json::array())) {
        Modulation m;
        m.name = e.value("name", "");
        m.bitsPerSymbol = e.value("bitsPerSymbol", 2.0);
        m.requiredEbN0 = Decibels(e.value("requiredEbN0Db", 10.5));
        m.rolloff = e.value("rolloff", 0.35);
        if (!m.name.empty() && m.bitsPerSymbol > 0.0) {
            lib.entries_.push_back(std::move(m));
        }
    }
    if (lib.entries_.empty()) {
        return Error{"modulation library holds no valid entries: " + utf8Path};
    }
    return lib;
}

const Modulation* ModulationLibrary::find(const std::string& name) const {
    for (const auto& m : entries_) {
        if (m.name == name) {
            return &m;
        }
    }
    return nullptr;
}

} // namespace tl::budget
