#pragma once

// Вспомогательные средства тестов drawing_v2: независимый разбор групповых кодов DXF,
// разбор структуры PDF, запуск сторонних программ, эталонный чертёж.

#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/title_block.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dv2test {

using namespace bendcore::drawing_v2;

std::string outPath(const std::string& name);

// Шрифт из системы (DejaVu Sans / Liberation Sans); nullptr, если не найден.
const TrueTypeFont* systemFont();

// --- DXF: пары «код — значение» ---
struct DxfPair {
    int code;
    std::string value;
};
struct DxfObject {
    std::string type;
    std::vector<DxfPair> pairs;  // без начальной пары 0
    std::vector<std::string> all(int code) const;
    std::optional<std::string> first(int code) const;
    double real(int code) const;  // первое значение кода как число (NaN, если нет)
};
struct DxfFile {
    std::vector<std::pair<std::string, std::vector<DxfObject>>> sections;  // имя → объекты
    const std::vector<DxfObject>* section(const std::string& name) const;
};
// Разбор ASCII DXF. Бросает std::runtime_error при нарушении формата (нечётное число
// строк, нечисловой код, секция без ENDSEC, отсутствие EOF).
DxfFile parseDxf(const std::string& text);
// Декодирование \U+XXXX в UTF-8.
std::string decodeDxfUnicode(const std::string& s);

// --- PDF ---
struct PdfObject {
    int id;
    std::size_t offset;
    std::string dict;    // текст между "obj" и "stream"/"endobj"
    std::string stream;  // содержимое потока (если есть)
};
struct PdfFile {
    std::vector<std::size_t> xrefOffsets;  // по номеру объекта (0 — свободный)
    std::size_t startxref = 0;
    std::vector<PdfObject> objects;
    const PdfObject* find(const std::string& needle) const;
};
// Разбор PDF без сжатия: xref, объекты, потоки по /Length. Бросает при ошибке структуры.
PdfFile parsePdf(const std::string& bytes);

// Запуск команды, вывод stdout; код возврата — в exitCode.
std::string runCommand(const std::string& cmd, int* exitCode);

std::string readFile(const std::string& path);

// Эталонный чертёж: лист, рамка, основная надпись, набор примитивов всех типов.
struct Sample {
    Drawing drawing;
    TitleBlockLayout layout;
};
Sample makeSample(GostFormat format, Orientation orientation, const TrueTypeFont* font);
TitleBlockData sampleTitleData();

}  // namespace dv2test
