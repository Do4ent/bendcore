#pragma once

// Таблица исполнений сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8.2): аналог таблицы
// параметров детали (docs/modeling/PART_DOCUMENT.md, «Таблица параметров») для состава сборки.
//
// Строка — исполнение: обозначение (полное «РЦ.100.00.000» или суффикс «-01» к обозначению
// первой строки, ГОСТ 2.113) и наименование. Столбец — свойство вхождения или сопряжения:
//   Suppression   — вхождение исключено («1») или включено («0»); также true/false, да/нет;
//   Configuration — исполнение детали или внешней подсборки у вхождения: обозначение
//                   исполнения её документа («-01»); «#» — компонент как сохранён (без «#…»);
//   MateValue     — значение сопряжения Distance (мм), Angle (градусы) или Gear (отношение zb/za).
// Пустая ячейка — базовое значение (состояние документа без активного исполнения).
// Переменных у документа сборки нет: параметры состава — значения сопряжений.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::assembly_v2 {

enum class AssemblyColumnKind { Suppression, Configuration, MateValue };

const char* toString(AssemblyColumnKind kind) noexcept;
std::optional<AssemblyColumnKind> assemblyColumnKindFromString(std::string_view text) noexcept;

struct AssemblyConfigurationColumn {
    AssemblyColumnKind kind = AssemblyColumnKind::Suppression;
    // Suppression, Configuration — ID вхождения (OccurrenceId); MateValue — ID сопряжения (MateId).
    std::uint32_t target = 0;
    bool operator==(const AssemblyConfigurationColumn&) const = default;
};

struct AssemblyConfiguration {
    std::string designation;
    std::string name;
    std::vector<std::string> cells;  // по одной на столбец
    bool operator==(const AssemblyConfiguration&) const = default;
};

struct AssemblyConfigurationTable {
    std::vector<AssemblyConfigurationColumn> columns;
    std::vector<AssemblyConfiguration> rows;
    std::string active;  // обозначение активного исполнения; пусто — базовые значения

    bool empty() const noexcept { return columns.empty() && rows.empty(); }
    std::optional<std::size_t> rowOf(std::string_view designation) const;
    std::optional<std::size_t> columnOf(const AssemblyConfigurationColumn& column) const;
    bool operator==(const AssemblyConfigurationTable&) const = default;
};

// Полное обозначение исполнения: суффикс «-01» дописывается к обозначению первой строки, а если
// и она суффикс — к rootDesignation (обозначению сборки); иначе — как записано.
std::string fullDesignation(const AssemblyConfigurationTable& table, std::size_t row,
                            const std::string& rootDesignation = {});

}  // namespace bendcore::assembly_v2
