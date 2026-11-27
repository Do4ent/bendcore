#pragma once

// Таблица параметров детали (исполнения, аналог Design Table / «Исполнения» КОМПАС;
// docs/modeling/PART_DOCUMENT.md, раздел «Таблица параметров»).
//
// Строка — исполнение: обозначение (полное «РЦ.080.00.010» или суффикс «-01» к обозначению
// первой строки, ГОСТ 2.113) и наименование. Столбец — переменная документа, флаг подавления
// операции или материал (id материала библиотеки ядра, kernel::builtInMaterials()). Ячейка —
// текст: выражение переменной, «1/0» (true/false, да/нет) для подавления, id материала;
// пустая ячейка — базовое значение документа.
//
// CSV: разделитель «;», первая строка — заголовки: «designation;name;<столбцы>», переменная —
// её имя, подавление — «suppress:<id операции>», материал — «material» (при чтении также
// «обозначение», «наименование», «подавить:<id>», «материал»). Числа — с десятичной точкой или
// запятой. Кавычки — по RFC 4180.

#include <bendcore/kernel/result.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::modeling {

enum class ConfigurationColumnKind { Variable, Suppression, Material };

const char* toString(ConfigurationColumnKind kind) noexcept;
std::optional<ConfigurationColumnKind> configurationColumnKindFromString(std::string_view text) noexcept;

struct ConfigurationColumn {
    ConfigurationColumnKind kind = ConfigurationColumnKind::Variable;
    std::string target;  // имя переменной или id операции; у материала пусто
    bool operator==(const ConfigurationColumn&) const = default;
};

// Заголовок столбца: «W», «suppress:Hole», «material».
std::string columnTitle(const ConfigurationColumn& column);

struct Configuration {
    std::string designation;
    std::string name;
    std::vector<std::string> cells;  // по одной на столбец
    bool operator==(const Configuration&) const = default;
};

struct ConfigurationTable {
    std::vector<ConfigurationColumn> columns;
    std::vector<Configuration> rows;
    std::string active;  // обозначение активного исполнения; пусто — базовые значения документа

    bool empty() const noexcept { return columns.empty() && rows.empty(); }
    std::optional<std::size_t> rowOf(std::string_view designation) const;
    std::optional<std::size_t> columnOf(const ConfigurationColumn& column) const;
    bool operator==(const ConfigurationTable&) const = default;
};

// Полное обозначение исполнения: суффикс «-01» дополняется обозначением первой строки
// («РЦ.080.00.010» + «-01» → «РЦ.080.00.010-01»), иначе — как записано.
std::string fullDesignation(const ConfigurationTable& table, std::size_t row);
// То же с обозначением документа детали base: суффикс дополняет base, если оно задано,
// иначе — обозначение первой строки (PartDocument::configurationDesignation).
std::string fullDesignation(const ConfigurationTable& table, std::size_t row, const std::string& base);

// Значение ячейки подавления: 1/0, true/false, yes/no, да/нет (регистр не важен);
// nullopt — недопустимое значение.
std::optional<bool> parseSuppressionCell(std::string_view text);

struct CsvOptions {
    char decimal = '.';    // десятичный разделитель чисел при записи: '.' или ','
    bool byteOrderMark = true;  // UTF-8 BOM в начале (Excel читает кириллицу)
};

// Запись таблицы в CSV (активное исполнение в CSV не пишется). Числовые ячейки записываются с
// десятичным разделителем options.decimal; ячейки с «;», кавычками, переводом строки или
// пробелами по краям — в кавычках.
std::string configurationTableToCsv(const ConfigurationTable& table, const CsvOptions& options = {});

// Чтение CSV. Ошибки ParseError с номером строки: незакрытая кавычка, нет столбцов
// обозначения и наименования, неизвестный вид заголовка, повтор столбца, число ячеек строки не
// совпадает с заголовком. Числа с десятичной запятой («12,5») переводятся в «12.5». Проверка по
// документу (переменные, операции, значения) — PartDocument::setConfigurationTable.
kernel::Result<ConfigurationTable> configurationTableFromCsv(std::string_view csv);

// Результат проверки одного исполнения (PartDocument::checkConfigurations).
struct ConfigurationCheck {
    std::string designation;      // как в таблице
    std::string fullDesignation;  // с обозначением первой строки
    bool ok = false;
    std::vector<std::string> problems;  // причины: ошибки ячеек, переменных, операций
    std::optional<double> volume;       // мм³ итогового тела, если построено
};

}  // namespace bendcore::modeling
