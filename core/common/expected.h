#pragma once

// std::expected-style error propagation for the core. No exceptions cross module
// boundaries: every fallible core API returns Expected<T>.

#include <string>
#include <utility>
#include <variant>

namespace tl {

struct Error {
    std::string message;
};

template <typename T>
class [[nodiscard]] Expected {
public:
    Expected(T value) : storage_(std::move(value)) {}                 // NOLINT(google-explicit-constructor)
    Expected(Error error) : storage_(std::move(error)) {}             // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return hasValue(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T& value() & { return std::get<T>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(storage_)); }
    [[nodiscard]] const Error& error() const { return std::get<Error>(storage_); }

    [[nodiscard]] T valueOr(T fallback) const {
        return hasValue() ? std::get<T>(storage_) : std::move(fallback);
    }

private:
    std::variant<T, Error> storage_;
};

// Expected<void> analogue.
class [[nodiscard]] Status {
public:
    Status() = default;
    Status(Error error) : error_(std::move(error)), failed_(true) {}  // NOLINT(google-explicit-constructor)
    static Status ok() { return {}; }

    [[nodiscard]] bool hasValue() const { return !failed_; }
    explicit operator bool() const { return !failed_; }
    [[nodiscard]] const Error& error() const { return error_; }

private:
    Error error_;
    bool failed_ = false;
};

} // namespace tl
