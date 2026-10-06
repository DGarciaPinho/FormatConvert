#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace forgeconvert {
enum class ErrorCode { internal = 1, arguments = 2, unsupported = 3, invalid_input = 4, io = 5, limit = 6 };
struct Error {
    ErrorCode code;
    std::string message;
    std::optional<std::size_t> byte_offset = {};
    std::optional<std::size_t> page = {};
};
template<class T> class Result {
    std::variant<T, Error> data_;
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}
    explicit operator bool() const { return std::holds_alternative<T>(data_); }
    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }
    const Error& error() const { return std::get<Error>(data_); }
};
// Internal exception is caught at public Result boundaries; never exits the process.
class Failure : public std::exception {
public:
    Error error;
    explicit Failure(Error e) : error(std::move(e)) {}
    const char* what() const noexcept override { return error.message.c_str(); }
};
[[noreturn]] inline void fail(ErrorCode code, std::string message,
                            std::optional<std::size_t> offset = {}) {
    throw Failure({code, std::move(message), offset, {}});
}
}
