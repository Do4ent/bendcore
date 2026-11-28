#pragma once

// Форматы листов: ГОСТ 2.301-68 (основные А0–А4 и дополнительные кратные, таблицы 1 и 2)
// и ISO 216 (ряд A0–A10).

#include <bendcore/drawing_v2/common.hpp>

#include <string>
#include <vector>

namespace bendcore::drawing_v2 {

enum class Orientation { Portrait, Landscape };

// Основные форматы ГОСТ 2.301 (табл. 1).
enum class GostFormat { A0, A1, A2, A3, A4 };

enum class SheetStandard { Gost2301, Iso216 };

struct Sheet {
    std::string name;      // "А4", "А3x3", "ISO A5"
    double widthMm = 0.0;  // по горизонтали с учётом ориентации
    double heightMm = 0.0;
    Orientation orientation = Orientation::Portrait;
    SheetStandard standard = SheetStandard::Gost2301;
    GostFormat gostBase = GostFormat::A4;  // для ГОСТ: основной формат
    int multiplier = 1;                    // для ГОСТ: кратность (1 — основной формат)
    int isoIndex = -1;                     // для ISO 216: номер n в An
};

// Формат ГОСТ 2.301. multiplier = 1 — основной формат (табл. 1); 2..9 — дополнительный
// по табл. 2 (допустимые кратности: А0 — 2, 3; А1 — 3, 4; А2 — 3..5; А3 — 3..7; А4 — 3..9).
// Ориентация Portrait — короткая сторона горизонтально.
Result<Sheet> gostSheet(GostFormat base, int multiplier = 1,
                        Orientation orientation = Orientation::Portrait);

// Формат ISO 216 ряда A: n = 0..10.
Result<Sheet> iso216Sheet(int n, Orientation orientation = Orientation::Portrait);

// Формат ISO 5457 (разд. 2): multiplier = 1 — основной формат ряда A ISO 216 (n = 0..4);
// иначе удлинённый — «специальный» (табл. 2: A3×3, A3×4, A4×3…A4×5) или «исключительный»
// (табл. 3: A0×2, A0×3, A1×3, A1×4, A2×3…A2×5, A3×5…A3×7, A4×6…A4×9). Размеры удлинённых
// форматов ISO 5457 совпадают с дополнительными форматами ГОСТ 2.301 (табл. 2). Имя — «ISO A3x3».
Result<Sheet> iso5457Sheet(int n, int multiplier = 1, Orientation orientation = Orientation::Portrait);

// Допустимые кратности основного формата: {1} и кратности табл. 2 ГОСТ 2.301 (= ISO 5457).
std::vector<int> sheetMultipliers(GostFormat base);

// Название основного формата: "А0".."А4" (кириллическая «А», UTF-8).
const char* gostFormatName(GostFormat base);

}  // namespace bendcore::drawing_v2
