#pragma once

// Однолинейный (stroke) чертёжный шрифт типа Б ГОСТ 2.304-81 — прямой и с наклоном 75°.
// Знаки построены на сетке с шагом d = h/10 (толщина линии шрифта типа Б, п. 2.2): высота
// прописных букв и цифр h = 10d, строчных c = 7d, выносные элементы строчных 3d, расстояние
// между буквами a = 2d, минимальное расстояние между словами e = 6d, шаг строк b = 17d
// (табл. 2). Ширины букв и цифр — по табл. 4 (кириллица) и табл. 6 (латиница) ГОСТ 2.304 —
// записаны по памяти и требуют сверки (docs/STANDARDS_VERIFICATION.md). Начертание —
// отрезки и дуги по оси линии; высота и ширина знака измеряются по оси линии.
// Документация — docs/drawing/EXPORT.md, раздел «Шрифт ГОСТ 2.304».

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/text_font.hpp>

#include <string_view>
#include <unordered_map>
#include <vector>

namespace bendcore::drawing_v2 {

// Знак шрифта в единицах d = h/10: основание строки y = 0, верх прописных y = 10.
struct StrokeGlyph {
    char32_t codePoint = 0;
    double width = 0.0;  // ширина знака по таблице ГОСТ 2.304 (d); у пробела — e − 2a
    std::vector<std::vector<Point2>> strokes;  // ломаные по оси линии (дуги — с шагом ≤ 10°)
};

// Строка, разложенная в линии на листе.
struct StrokeTextLayout {
    std::vector<std::vector<Point2>> strokes;  // мм листа
    double lineWidth = 0.0;  // толщина линии d = h/10, мм
    double width = 0.0;      // длина строки вдоль основания с учётом widthFactor, мм
};

class GostStrokeFont final : public TextMetrics {
public:
    // Параметры шрифта типа Б (ГОСТ 2.304, табл. 2), в долях d = h/10.
    static constexpr double kCapHeight = 10.0;      // h
    static constexpr double kLowerHeight = 7.0;     // c
    static constexpr double kDescender = 3.0;       // выносные элементы строчных
    static constexpr double kLetterSpacing = 2.0;   // a
    static constexpr double kWordSpacing = 6.0;     // e (минимальное)
    static constexpr double kLineSpacing = 17.0;    // b

    GostStrokeFont();

    // Знак; nullptr — знака в шрифте нет.
    const StrokeGlyph* glyph(char32_t cp) const;
    const std::vector<StrokeGlyph>& glyphs() const { return glyphs_; }

    double textWidthMm(std::u32string_view text, double capHeightMm) const override;
    bool hasGlyph(char32_t cp) const override { return glyph(cp) != nullptr; }
    double minWidthFactor() const override { return 1.0; }

    // Толщина линии шрифта типа Б, мм: d = h/10.
    static double strokeWidth(double capHeightMm) { return capHeightMm / 10.0; }

    // Линии строки на листе: масштаб h/10, коэффициент ширины, наклон 75° (сдвиг x += y·tg 15°
    // в системе строки), поворот, выравнивание — как у текста TrueType
    // (detail::textBaselineOrigin по ширине строки вдоль основания). Ошибки: InvalidInput —
    // некорректный UTF-8; Unsupported — знака нет в шрифте (код знака в сообщении).
    Result<StrokeTextLayout> layout(const Text& text) const;

private:
    std::vector<StrokeGlyph> glyphs_;
    std::unordered_map<char32_t, std::size_t> index_;
};

// Встроенный шрифт (строится один раз; потокобезопасная инициализация).
const GostStrokeFont& gostStrokeFont();

}  // namespace bendcore::drawing_v2
