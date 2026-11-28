#pragma once

// Подгонка текста в графу таблицы (спецификация ГОСТ 2.106, групповая спецификация ГОСТ 2.113,
// перечень ISO 7573): общий код модуля аннотаций, не публичный интерфейс.

#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/drawing_v2/model.hpp>

#include <string>

namespace bendcore::drawing_v2::annotations::detail {

// Текст в ячейке: высота по ряду ГОСТ 2.304 от preferred вниз до 2,5 и сжатие до 0,6 (только
// TrueType). Ошибка InvalidInput с именем графы what — текст не помещается.
Result<Text> fitCellText(const Rect& cell, const std::string& value, double preferred, HAlign h, bool vertical,
                         const TextMetrics* font, const char* what);

}  // namespace bendcore::drawing_v2::annotations::detail
