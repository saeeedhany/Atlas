#pragma once

#include <optional>
#include <utility>
#include <variant>

namespace atlas::core {

template <typename T, typename E>
class Result {
public:
    static Result ok(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result err(E error) { return Result(std::in_place_index<1>, std::move(error)); }

    bool hasValue() const { return storage_.index() == 0; }
    explicit operator bool() const { return hasValue(); }

    const T& value() const& { return std::get<0>(storage_); }
    // Without this overload, value() on a named Result binds to const& and std::move copies.
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

}
