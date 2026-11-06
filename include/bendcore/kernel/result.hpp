#pragma once

#include <string>
#include <utility>
#include <variant>

namespace bendcore::kernel {

enum class ErrorCode {
    InvalidInput,
    InvalidShape,
    OperationFailed,
    ParseError,
    IoError,
    Unsupported,
};

const char* toString(ErrorCode code) noexcept;

struct Error {
    ErrorCode code;
    std::string message;
};

// Value or Error; kernel functions never throw across the module boundary.
template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    static Result failure(ErrorCode code, std::string message) {
        return Result(Error{code, std::move(message)});
    }

    bool ok() const noexcept { return std::holds_alternative<T>(data_); }
    explicit operator bool() const noexcept { return ok(); }

    const T& value() const& { return std::get<T>(data_); }
    T& value() & { return std::get<T>(data_); }
    T&& value() && { return std::get<T>(std::move(data_)); }

    const Error& error() const& { return std::get<Error>(data_); }

private:
    std::variant<T, Error> data_;
};

inline const char* toString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::InvalidInput: return "InvalidInput";
        case ErrorCode::InvalidShape: return "InvalidShape";
        case ErrorCode::OperationFailed: return "OperationFailed";
        case ErrorCode::ParseError: return "ParseError";
        case ErrorCode::IoError: return "IoError";
        case ErrorCode::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

}  // namespace bendcore::kernel
