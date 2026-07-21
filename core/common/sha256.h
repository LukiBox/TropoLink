#pragma once

// SHA-256 (FIPS 180-4) — self-contained, deterministic, dependency-free.
// Used for report digital fingerprints and .tlk audit digests.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace tl {

class Sha256 {
  public:
    Sha256();
    void update(const void* data, std::size_t length);
    void update(std::string_view text) { update(text.data(), text.size()); }
    // Finalizes and returns the digest; the object must not be reused afterwards.
    [[nodiscard]] std::array<std::uint8_t, 32> digest();
    [[nodiscard]] std::string hexDigest();

    [[nodiscard]] static std::string hex(std::string_view text);

  private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t bitLength_ = 0;
    std::size_t bufferUsed_ = 0;
};

} // namespace tl
