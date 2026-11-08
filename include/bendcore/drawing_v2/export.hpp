#pragma once

// Экспорт чертежа: DXF (ASCII, AutoCAD R2000 / AC1015), SVG 1.1, PDF 1.4 (векторный).

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/truetype_font.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace bendcore::drawing_v2 {

// DXF R2000: единицы — мм ($INSUNITS = 4, $MEASUREMENT = 1); не-ASCII символы текста —
// последовательности \U+XXXX, $DWGCODEPAGE = ANSI_1251 (см. docs/drawing/EXPORT.md).
// Символы вне BMP (> U+FFFF) — ошибка Unsupported.
Result<std::string> writeDxf(const Drawing& drawing);

// Толщина линии DXF (группа 370) — ближайшее значение из стандартного набора AutoCAD,
// в сотых долях мм.
int dxfLineweight(double widthMm);

struct SvgOptions {
    // Если задан шрифт, кегль вычисляется по его высоте прописных; иначе по capHeightRatio.
    const TrueTypeFont* font = nullptr;
    double capHeightRatio = 0.729;  // DejaVu Sans: 1493/2048
    std::string fontFamily = "DejaVu Sans";
    std::string title;
};

// SVG: width/height в мм, viewBox в мм листа (1 единица = 1 мм), ось Y перевёрнута.
Result<std::string> writeSvg(const Drawing& drawing, const SvgOptions& options = {});

struct PdfOptions {
    // Шрифт TrueType для текста; встраивается целиком (FontFile2) как CIDFontType2 с
    // кодировкой Identity-H, массивом ширин W и картой ToUnicode. Обязателен, если в
    // чертеже есть текст.
    const TrueTypeFont* font = nullptr;
    std::string title;
};

// PDF: одна страница, MediaBox = размер листа в мм · 72 / 25,4; координаты содержимого —
// в пунктах, без дополнительного преобразования.
Result<std::string> writePdf(const Drawing& drawing, const PdfOptions& options = {});

// Многостраничный PDF: каждая страница — свой лист (размер MediaBox — по листу страницы),
// шрифт встраивается один раз. Используется для спецификации ГОСТ 2.106 на нескольких
// листах. Ошибка InvalidInput — пустой список или нулевой указатель.
Result<std::string> writePdfPages(const std::vector<const Drawing*>& drawings, const PdfOptions& options = {});

// Запись буфера в файл (двоичный режим). Возвращает число записанных байт.
Result<std::size_t> saveToFile(const std::string& path, const std::string& bytes);

}  // namespace bendcore::drawing_v2
