#pragma once

// Private helper of bendcore_kernel: text of an OCCT exception for Error::message.

#include <string>

#include <Standard_Failure.hxx>

namespace bendcore::kernel::detail {

inline std::string failureText(const Standard_Failure& failure) {
    std::string text = failure.ExceptionType();
    const char* message = failure.what();
    if (message != nullptr && *message != '\0') {
        text += ": ";
        text += message;
    }
    return text;
}

}  // namespace bendcore::kernel::detail
