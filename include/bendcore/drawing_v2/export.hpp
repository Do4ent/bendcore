#pragma once

// Экспорт чертежа: DXF (ASCII, AutoCAD R2000 / AC1015), SVG 1.1, PDF 1.4 (векторный).

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/drawing_v2/stroke_font.hpp>
#include <bendcore/drawing_v2/truetype_font.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace bendcore::drawing_v2 {

// DXF R2000: единицы — мм ($INSUNITS = 4, $MEASUREMENT = 1); не-ASCII символы текста —
// последовательности \U+XXXX, $DWGCODEPAGE = ANSI_1251 (см. docs/drawing/EXPORT.md).
// Символы вне BMP (> U+FFFF) — ошибка Unsupported.
//
// Лист со шрифтом ГОСТ 2.304 (Drawing::textFont() == Gost2304): каждый текст — полилинии
// LWPOLYLINE по линиям знаков на слое текста (тип Continuous, толщина 370 — ближайшая к
// d = h/10); если sourceText, рядом пишется и сам текст (TEXT) невидимым (группа 60 = 1) — для
// поиска и редактирования в CAD. Знак, которого нет в шрифте, — ошибка Unsupported.
struct DxfOptions {
    bool sourceText = true;
};
Result<std::string> writeDxf(const Drawing& drawing, const DxfOptions& options = {});

// Толщина линии DXF (группа 370) — ближайшее значение из стандартного набора AutoCAD,
// в сотых долях мм.
int dxfLineweight(double widthMm);

struct SvgOptions {
    // Если задан шрифт, кегль вычисляется по его высоте прописных; иначе по capHeightRatio.
    const TrueTypeFont* font = nullptr;
    double capHeightRatio = 0.729;  // DejaVu Sans: 1493/2048
    std::string fontFamily = "DejaVu Sans";
    std::string title;
    // Лист со шрифтом ГОСТ 2.304: текст — <path> по линиям знаков (stroke-width = h/10, круглые
    // концы) в группе <g class="text" data-text="…">; если sourceText, в группе ещё и
    // прозрачный <text> той же длины (textLength) — для поиска и выделения текста.
    bool sourceText = true;
};

// SVG: width/height в мм, viewBox в мм листа (1 единица = 1 мм), ось Y перевёрнута.
Result<std::string> writeSvg(const Drawing& drawing, const SvgOptions& options = {});

struct PdfOptions {
    // Шрифт TrueType для текста; встраивается целиком (FontFile2) как CIDFontType2 с
    // кодировкой Identity-H, массивом ширин W и картой ToUnicode. Обязателен, если в
    // чертеже есть текст.
    const TrueTypeFont* font = nullptr;
    std::string title;
    // Лист со шрифтом ГОСТ 2.304: текст — контуры линий знаков (толщина h/10, круглые концы и
    // соединения); шрифт TrueType не обязателен. Если sourceText и font задан, поверх пишется
    // невидимый текст (режим 3 Tr) той же длины строки — для поиска и копирования (pdftotext);
    // строки со знаками, которых нет в font, невидимым слоем не дублируются.
    bool sourceText = true;
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
