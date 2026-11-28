#pragma once

// Групповые конструкторские документы ГОСТ 2.113-75: таблица исполнений группового рабочего
// чертежа детали (буквенные обозначения размеров, различающихся у исполнений) и групповая
// спецификация (количество по каждому исполнению — графы по исполнениям или раздел
// «Переменные данные для исполнений»).
// Документация — docs/drawing/ANNOTATIONS.md («Групповой чертёж»), docs/drawing/SPECIFICATION.md
// («Групповая спецификация ГОСТ 2.113»).

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/drawing_v2/specification.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// --- Таблица исполнений группового чертежа ---

// Буквенное обозначение размера группового чертежа по порядку: A, B, C, … без I и O (их легко
// спутать с цифрами 1 и 0), после Z — AA, AB, … (docs/STANDARDS_VERIFICATION.md).
std::string groupDimensionLetter(int index);

// Таблица исполнений: заголовки граф и строка на каждое исполнение (ГОСТ 2.113, п. 2.6).
struct GroupTable {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;  // по исполнениям, ячейка на графу
};

struct GroupTableOptions {
    const TextMetrics* font = nullptr;  // метрики для ширины граф; nullptr — шрифт ГОСТ 2.304
    double rowHeight = 8.0;
    double headerHeight = 10.0;
    double textHeight = 3.5;
    double minColumnWidth = 12.0;
    double padding = 2.0;          // поле слева и справа от текста в графе, мм
    std::string layer = "TABLE";
};

// Ширины граф: наибольшая ширина текста графы (заголовок и значения) с полями, не меньше
// minColumnWidth, округлённая вверх до 1 мм.
Result<std::vector<double>> groupTableWidths(const GroupTable& table, const GroupTableOptions& options = {});
double groupTableHeight(const GroupTable& table, const GroupTableOptions& options = {});

struct GroupTableLayout {
    Rect box;
    std::vector<double> columnX;  // левые границы граф и правая граница таблицы
    std::vector<double> rowTops;  // верх строки заголовков и строк исполнений
};

// Таблица с правым нижним углом в точке bottomRight (над основной надписью): контур, вертикали и
// линия под заголовками — сплошные основные, строки — сплошные тонкие (ГОСТ 2.303); текст —
// ГОСТ 2.304 по центру графы. Ошибка InvalidInput — нет граф или строк, число ячеек строки не
// равно числу граф, таблица выходит за frame.
Result<GroupTableLayout> addGroupTable(Drawing& drawing, Point2 bottomRight, const Rect& frame, const GroupTable& table,
                                       const GroupTableOptions& options = {});

// --- Групповая спецификация ГОСТ 2.113 ---

// Способ записи переменных данных (ГОСТ 2.113, разд. 4):
//   Columns — графа «Кол.» разделена на графы по исполнениям (количество в каждом, «–» — нет);
//   VariableData — постоянная часть (составные части с одинаковым количеством во всех
//   исполнениях) записывается в разделы как обычно, остальное — в разделе «Переменные данные для
//   исполнений» под заголовком обозначения каждого исполнения.
enum class GroupSpecLayout { Columns, VariableData };
const char* toString(GroupSpecLayout layout) noexcept;  // "columns", "variableData"

struct GroupSpecItem {
    SpecItem item;               // quantity не задано: количество — quantities
    std::vector<int> quantities;  // по исполнениям; 0 — нет в исполнении
    bool constant() const;        // количество одинаково во всех исполнениях
};

struct GroupSpecSection {
    SpecSection section = SpecSection::Parts;
    std::vector<GroupSpecItem> items;
};

struct GroupSpecification {
    std::string designation;  // обозначение базового исполнения
    std::string name;
    GroupSpecLayout layout = GroupSpecLayout::Columns;
    std::vector<std::string> configurations;  // полные обозначения исполнений по порядку таблицы
    std::vector<std::string> columnHeaders;   // заголовки граф количества («–», «01», «02», …)
    std::vector<GroupSpecSection> sections;   // только непустые, в порядке ГОСТ 2.106

    std::optional<int> positionOf(std::uint32_t definition) const;
    // Спецификация одного исполнения с номерами позиций групповой (составные части с
    // количеством 0 не входят): для позиций сборочного чертежа и перечня ISO 7573.
    Specification forConfiguration(std::size_t configuration) const;
};

struct GroupSpecOptions {
    SpecificationOptions base;  // разделы, графы «Формат»/«Примечание», документы, резерв
    GroupSpecLayout layout = GroupSpecLayout::Columns;
    // Заголовки граф количества по исполнениям; пусто или пустая строка — по умолчанию: суффикс
    // обозначения без «-» («01»), у исполнения без суффикса — «–».
    std::vector<std::string> columnHeaders;
};

// Групповая спецификация по составу всех исполнений (assembly_v2::configurationBom): разделы и
// порядок — как в buildSpecification (ГОСТ 2.106); позиции — сквозные: Columns — в порядке
// записи; VariableData — сначала постоянная часть, затем переменные составные части (номер
// позиции у составной части один во всех исполнениях). designation/name — обозначение и
// наименование базового исполнения (пустое обозначение — обозначение первого исполнения).
// Ошибки InvalidInput: нет исполнений, составная часть без наименования, «разборные»
// подсборки (base.dissolve) — групповая спецификация их не поддерживает.
Result<GroupSpecification> buildGroupSpecification(const assembly_v2::ConfigurationBom& bom, std::string designation,
                                                   std::string name, const GroupSpecOptions& options = {});

// Графы групповой спецификации, мм: «Формат» 6, «Зона» 6, «Поз.» 8, «Обозначение» 70,
// «Наименование», графы количества (по 10 мм до 4 исполнений, по 8 мм — до 7), «Примечание»
// (22, при 5–7 исполнениях — 12); всего 185. Ошибка InvalidInput — больше 7 исполнений.
Result<std::vector<double>> groupSpecColumnWidths(std::size_t configurations);

// Листы групповой спецификации — форма 1 ГОСТ 2.106 с изменениями ГОСТ 2.113: Columns — графа
// «Кол. на исполн.» с графами по исполнениям (SpecRow::quantities), VariableData — обычная
// форма 1 с разделом «Переменные данные для исполнений». Остальное (листы, основная надпись,
// перенос наименований) — как renderSpecification.
Result<SpecDocument> renderGroupSpecification(const GroupSpecification& spec, const SpecSheetOptions& options = {});

}  // namespace bendcore::drawing_v2::annotations
