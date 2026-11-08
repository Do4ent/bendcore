#pragma once

// Спецификация ГОСТ 2.106-96 (форма 1, листы А4 с основной надписью ГОСТ 2.104 форм 2 и 2а)
// и перечень составных частей ISO 7573 на поле чертежа — из состава документа сборки
// (assembly_v2::structuredBom); синхронизация номеров позиций с выносками.
// Документация — docs/drawing/SPECIFICATION.md.

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/drawing_v2/annotations.hpp>
#include <bendcore/drawing_v2/title_block.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Разделы спецификации в порядке записи по ГОСТ 2.106-96 (разд. 3).
enum class SpecSection {
    Documentation,     // Документация
    Complexes,         // Комплексы
    AssemblyUnits,     // Сборочные единицы
    Parts,             // Детали
    StandardProducts,  // Стандартные изделия
    OtherProducts,     // Прочие изделия
    Materials,         // Материалы
    Kits,              // Комплекты
};

const char* sectionTitle(SpecSection s);

struct SpecItem {
    std::string format;  // формат документа составной части («А4», «БЧ» — без чертежа)
    std::string zone;
    int position = 0;    // 0 — без номера позиции (документация)
    std::string designation;
    std::string name;
    std::optional<int> quantity;  // документация — без количества
    std::string note;
    std::string material;  // для перечня ISO 7573 (в спецификацию ГОСТ не выводится)
    std::optional<std::uint32_t> definition;  // assembly_v2::DefinitionId источника
};

struct SpecSectionData {
    SpecSection section = SpecSection::Parts;
    std::vector<SpecItem> items;
};

struct Specification {
    std::string designation;  // обозначение сборочной единицы
    std::string name;
    std::vector<SpecSectionData> sections;  // только непустые, в порядке ГОСТ
    // Номер позиции составной части; nullopt — нет в спецификации.
    std::optional<int> positionOf(std::uint32_t definition) const;
    const SpecSectionData* section(SpecSection s) const;
};

struct SpecificationOptions {
    // Раздел для определения. По умолчанию: сборка — «Сборочные единицы»; деталь с
    // обозначением — «Детали»; деталь без обозначения — «Стандартные изделия».
    std::map<std::uint32_t, SpecSection> sectionOverride;
    std::map<std::uint32_t, std::string> formats;  // графа «Формат»
    std::map<std::uint32_t, std::string> notes;    // графа «Примечание»
    // Документы раздела «Документация» (без позиций), кроме сборочного чертежа.
    std::vector<SpecItem> documents;
    // Сборочный чертёж «<обозначение> СБ» первым документом раздела «Документация».
    bool addAssemblyDrawing = true;
    std::string assemblyDrawingFormat = "А3";
    // Резервные номера позиций после каждого раздела (ГОСТ 2.106 разрешает резерв).
    int reservePositions = 0;
};

// Спецификация сборки: составные части верхнего уровня корневой сборки с количеством на одну
// сборку (structuredBom, уровень 0). Порядок в разделах: «Сборочные единицы», «Детали» — по
// возрастанию обозначений; «Стандартные изделия», «Прочие изделия», «Материалы» — по
// наименованию, затем по обозначению (сравнение с числами по значению: М6 < М10).
// Позиции — сквозная нумерация в порядке записи (кроме «Документации»).
Result<Specification> buildSpecification(const assembly_v2::AssemblyDocument& document,
                                         const SpecificationOptions& options = {});

// Сравнение строк «как люди»: последовательности цифр — по значению.
bool naturalLess(const std::string& a, const std::string& b);

// --- Листы спецификации ---

enum class SpecRowKind { Blank, Header, Item };

struct SpecRow {
    SpecRowKind kind = SpecRowKind::Blank;
    // Колонки формы 1: Формат, Зона, Поз., Обозначение, Наименование, Кол., Примечание.
    std::array<std::string, 7> cells;
    bool continuation = false;  // продолжение наименования предыдущей позиции
};

struct SpecSheetOptions {
    TitleBlockData title;            // обозначение и наименование берутся из спецификации
    const TrueTypeFont* font = nullptr;  // для подгонки текста по ширине граф
    double textHeight = 3.5;
    double rowHeight = 8.0;          // ГОСТ 2.106, форма 1: строки не менее 8 мм
    double headerHeight = 15.0;
    bool storageStrip = true;
};

struct SpecDocument {
    std::vector<Drawing> sheets;
    std::vector<std::vector<SpecRow>> rows;  // строки по листам
    int firstSheetRows = 0;  // ёмкость первого листа (форма 2) и последующих (форма 2а)
    int otherSheetRows = 0;
};

// Колонки формы 1 ГОСТ 2.106, мм: 6, 6, 8, 70, 63, 10, 22.
const std::array<double, 7>& specColumnWidths();

// Строки спецификации: пустая строка, заголовок раздела (подчёркнут), пустая строка, позиции;
// наименование переносится по словам в пределах графы. Листы А4 книжной ориентации: первый —
// основная надпись формы 2, последующие — формы 2а; заголовок раздела не остаётся последним
// на листе.
Result<SpecDocument> renderSpecification(const Specification& spec, const SpecSheetOptions& options = {});

// --- Перечень составных частей ISO 7573 на поле чертежа ---

struct PartsListOptions {
    const TrueTypeFont* font = nullptr;
    double rowHeight = 8.0;
    double textHeight = 3.5;
    // Заголовки граф: позиция, количество, наименование, обозначение, материал.
    std::array<std::string, 5> headers = {"Item", "Qty", "Title / Name", "Reference", "Material"};
    std::array<double, 5> widths = {15, 15, 60, 50, 45};
    std::string layer = "TABLE";
};

// Таблица над основной надписью: заголовок примыкает к основной надписи, позиции — снизу
// вверх по возрастанию номеров (ISO 7573). Возвращает габарит таблицы.
Result<Rect> addPartsList(Drawing& drawing, const TitleBlockLayout& layout, const Specification& spec,
                          const PartsListOptions& options = {});

// --- Синхронизация позиций ---

struct BalloonSyncReport {
    std::vector<std::uint32_t> withoutBalloon;  // позиции спецификации без выноски
    std::vector<std::uint32_t> duplicated;      // определения с несколькими выносками
};

// Номера выносок по спецификации. Ошибка InvalidInput — выноска ссылается на определение,
// которого нет в спецификации (номер не может быть назначен).
Result<BalloonSyncReport> synchronizeBalloons(std::vector<Balloon>& balloons, const Specification& spec);

}  // namespace bendcore::drawing_v2::annotations
