#pragma once

// Чтение шрифта TrueType (контуры glyf) для метрик текста и встраивания в PDF.
// Читаются таблицы head, hhea, maxp, hmtx, cmap (форматы 4 и 12), loca, glyf, name, post,
// OS/2. Шрифт с запретом встраивания (OS/2 fsType, бит 1 — Restricted License) отвергается.

#include <bendcore/drawing_v2/common.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bendcore::drawing_v2 {

class TrueTypeFont {
public:
    static Result<TrueTypeFont> loadFile(const std::string& path);
    static Result<TrueTypeFont> fromBytes(std::vector<std::uint8_t> bytes, std::string source);

    const std::string& source() const { return source_; }
    const std::string& postScriptName() const { return psName_; }
    const std::vector<std::uint8_t>& bytes() const { return data_; }

    std::uint16_t unitsPerEm() const { return unitsPerEm_; }
    std::uint16_t numGlyphs() const { return numGlyphs_; }
    int ascender() const { return ascender_; }
    int descender() const { return descender_; }
    // Высота прописных: OS/2 sCapHeight (версия ≥ 2), иначе yMax глифа 'H'.
    int capHeight() const { return capHeight_; }
    double italicAngle() const { return italicAngle_; }
    std::uint16_t fsType() const { return fsType_; }
    int xMin() const { return xMin_; }
    int yMin() const { return yMin_; }
    int xMax() const { return xMax_; }
    int yMax() const { return yMax_; }

    // Индекс глифа символа; 0 — символа в шрифте нет.
    std::uint16_t glyphIndex(char32_t cp) const;
    // Ширина продвижения глифа в единицах шрифта.
    std::uint16_t advanceWidth(std::uint16_t glyph) const;
    // Верх контура глифа (yMax из glyf); 0 для пустого глифа.
    int glyphYMax(std::uint16_t glyph) const;

    // Отношение высоты прописных к кеглю (capHeight / unitsPerEm).
    double capHeightRatio() const { return double(capHeight_) / double(unitsPerEm_); }
    // Ширина строки в мм при высоте прописных capHeightMm (без кернинга).
    double textWidthMm(std::u32string_view text, double capHeightMm) const;

private:
    TrueTypeFont() = default;
    Status parse();

    std::string source_;
    std::vector<std::uint8_t> data_;
    std::string psName_;
    std::uint16_t unitsPerEm_ = 0;
    std::uint16_t numGlyphs_ = 0;
    std::uint16_t numHMetrics_ = 0;
    int ascender_ = 0, descender_ = 0, capHeight_ = 0;
    double italicAngle_ = 0.0;
    std::uint16_t fsType_ = 0;
    int xMin_ = 0, yMin_ = 0, xMax_ = 0, yMax_ = 0;
    int indexToLocFormat_ = 0;
    std::uint32_t hmtxOffset_ = 0, locaOffset_ = 0, glyfOffset_ = 0, glyfLength_ = 0;
    std::unordered_map<char32_t, std::uint16_t> cmap_;
};

// Пути, где ищется шрифт с пермиссивной лицензией (DejaVu Sans — лицензия Bitstream Vera;
// Liberation Sans — SIL OFL 1.1). Первым проверяется переменная окружения
// BENDCORE_DRAWING_FONT.
std::vector<std::string> defaultFontCandidates();

// Первый существующий и читаемый шрифт из defaultFontCandidates(); ошибка IoError, если нет.
Result<std::string> findSystemFont();

}  // namespace bendcore::drawing_v2
