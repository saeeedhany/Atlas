#pragma once

#include <optional>
#include <utility>
#include <variant>

namespace atlas::core {

// Minimal Result<T, E> for fallible operations where exceptions are
// undesirable (e.g. across a future plugin/ABI boundary — see M0
// design notes). Deliberately small: only what M0 actually needs.
// No monadic map/and_then yet; add only when a real use case appears.
template <typename T, typename E>
class Result {
public:
    static Result ok(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result err(E error) { return Result(std::in_place_index<1>, std::move(error)); }

    bool hasValue() const { return storage_.index() == 0; }
    explicit operator bool() const { return hasValue(); }

    // All three ref-qualified overloads exist deliberately, mirroring
    // std::optional/std::variant — not just the const& and && pair.
    // Without the plain `&` overload, calling .value() on a named
    // non-const Result has no matching non-const candidate, so it
    // silently falls back to the const& overload. That's not just a
    // style issue: it means `std::move(namedResult.value())` doesn't
    // actually move anything (you can't move out of a const
    // reference), silently downgrading to a copy. Found via
    // -Wredundant-move on a real call site in atlas-ui's load(), which
    // was copying every loaded object instead of moving it.
    const T& value() const& { return std::get<0>(storage_); }
    T& value() & { return std::get<0>(storage_); }
    T&& value() && { return std::get<0>(std::move(storage_)); }

    const E& error() const& { return std::get<1>(storage_); }
    E& error() & { return std::get<1>(storage_); }
    E&& error() && { return std::get<1>(std::move(storage_)); }

private:
    Result(std::in_place_index_t<0> tag, T value) : storage_(tag, std::move(value)) {}
    Result(std::in_place_index_t<1> tag, E error) : storage_(tag, std::move(error)) {}

    std::variant<T, E> storage_;
};

// Specialization for operations that can fail but produce no value on
// success (e.g. KnowledgeObject::renameTo).
template <typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(); }
    static Result err(E error) { return Result(std::move(error)); }

    bool hasValue() const { return !error_.has_value(); }
    explicit operator bool() const { return hasValue(); }

    const E& error() const { return *error_; }

private:
    Result() = default;
    explicit Result(E error) : error_(std::move(error)) {}

    std::optional<E> error_;
};

}  // namespace atlas::core
