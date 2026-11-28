#pragma once

// Выбор шрифта текста чертежа и общий интерфейс метрик (ширина строки, наличие знака).
// Реализации: GostStrokeFont (однолинейный шрифт типа Б ГОСТ 2.304, stroke_font.hpp) и
// TrueTypeFont (DejaVu Sans и др., truetype_font.hpp). Документация — docs/drawing/EXPORT.md,
// раздел «Шрифт ГОСТ 2.304».

#include <optional>
#include <string_view>

namespace bendcore::drawing_v2 {

// Как выводится текст листа.
enum class TextFont {
    TrueType,  // контурный шрифт TrueType: TEXT в DXF, <text> в SVG, встроенный шрифт в PDF
    Gost2304,  // однолинейный шрифт типа Б ГОСТ 2.304: текст выводится линиями (DXF, SVG, PDF)
};

const char* toString(TextFont font) noexcept;  // "truetype", "gost2304"
std::optional<TextFont> textFontFromString(std::string_view name) noexcept;

// Метрики шрифта для раскладки текста (основная надпись, размеры, полки, таблицы).
class TextMetrics {
public:
    virtual ~TextMetrics() = default;
    // Ширина строки вдоль основания, мм, при высоте прописных capHeightMm (без кернинга,
    // коэффициент ширины 1).
    virtual double textWidthMm(std::u32string_view text, double capHeightMm) const = 0;
    // Знак есть в шрифте.
    virtual bool hasGlyph(char32_t cp) const = 0;
    // Наименьший коэффициент ширины при подгонке текста в графу (основная надпись, таблицы):
    // контурный шрифт сжимается до 0,6; шрифт ГОСТ 2.304 не сжимается (1,0) — вместо сжатия
    // уменьшается высота по ряду ГОСТ 2.304.
    virtual double minWidthFactor() const { return 0.6; }
};

}  // namespace bendcore::drawing_v2
