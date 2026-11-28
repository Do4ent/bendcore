#pragma once

// Общие типы модуля чертежей: результат с ошибкой (из ядра) и точка в мм.

#include <bendcore/kernel/result.hpp>

#include <string>
#include <string_view>
#include <variant>

namespace bendcore::drawing_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

// Результат операции без значения.
using Status = Result<std::monostate>;

inline Status okStatus() { return Status(std::monostate{}); }

// Точка листа. Единицы — миллиметры, начало — левый нижний угол листа, ось Y вверх.
struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

// Декодирование UTF-8 в кодовые точки Unicode. Ошибка — при неверной последовательности,
// суррогатах и «длинных» кодировках.
Result<std::u32string> decodeUtf8(std::string_view text);

// Кодирование кодовых точек в UTF-8.
std::string encodeUtf8(std::u32string_view text);

}  // namespace bendcore::drawing_v2
