#pragma once

// Deterministic FNV-1a 64-bit hash. Used for report-content hashes and regression
// auditing: identical inputs must yield an identical, platform-independent hash
// (std::hash makes no such guarantee).

#include <cstdint>
#include <string_view>

namespace tl {

class Fnv1a {
public:
    void update(std::string_view text) {
        for (const char c : text) {
            state_ ^= static_cast<std::uint8_t>(c);
            state_ *= 0x100000001b3ULL;
        }
    }
    [[nodiscard]] std::uint64_t digest() const { return state_; }

private:
    std::uint64_t state_ = 0xcbf29ce484222325ULL;
};

[[nodiscard]] inline std::uint64_t fnv1a(std::string_view text) {
    Fnv1a h;
    h.update(text);
    return h.digest();
}

} // namespace tl
