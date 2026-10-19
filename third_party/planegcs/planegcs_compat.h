// SPDX-License-Identifier: LGPL-2.1-or-later
// Замена зависимостей FreeCAD (Base::Console, Base::TimeElapsed) для автономной сборки PlaneGCS.
// Журнал отключён, пока приложение не передаст приёмник через setLogSink.
// Форматирование — подстановка «{}» по порядку (C++17, без <format>): PlaneGCS собирается по C++17,
// потому что Eigen 3.4.90 (ветка master) не компилируется в режиме C++20 с GCC 13 (LDLT.h).
#pragma once

#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace planegcs {

using LogSink = std::function<void(const std::string&)>;

inline LogSink& logSink() {
    static LogSink sink;
    return sink;
}

inline void setLogSink(LogSink sink) {
    logSink() = std::move(sink);
}

namespace detail {

inline void formatTo(std::ostream& os, const char* fmt) {
    os << fmt;
}

template <class T, class... Rest>
void formatTo(std::ostream& os, const char* fmt, const T& value, const Rest&... rest) {
    for (; *fmt != '\0'; ++fmt) {
        if (fmt[0] == '{' && fmt[1] == '}') {
            os << value;
            formatTo(os, fmt + 2, rest...);
            return;
        }
        os << *fmt;
    }
}

}  // namespace detail

template <class... Args>
void log(const char* fmt, const Args&... args) {
    if (const auto& sink = logSink(); sink) {
        std::ostringstream os;
        detail::formatTo(os, fmt, args...);
        sink(os.str());
    }
}

template <class... Args>
void warning(const char* fmt, const Args&... args) {
    log(fmt, args...);
}

}  // namespace planegcs
