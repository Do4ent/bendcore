#pragma once

// Состав изделия (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8): плоский (только детали,
// количество по всему дереву) и по уровням (на каждом уровне вхождения сгруппированы по
// определению). Масса детали — объём BRepGProp × плотность из PartProperties.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <optional>
#include <string>
#include <vector>

namespace bendcore::assembly_v2 {

struct BomRow {
    int position = 0;  // номер позиции на своём уровне (плоский: порядок первого появления)
    std::string positionPath;  // по уровням: "1", "1.2"; в плоском — номер
    int level = 0;             // 0 — верхний уровень
    DefinitionId definition = 0;
    DefinitionKind kind = DefinitionKind::Part;
    std::string designation;
    std::string name;
    std::string material;
    int quantity = 0;  // плоский: всего в изделии; по уровням: в родительской сборке
    double unitVolume = 0.0;           // мм³ (сборка — сумма по составу)
    std::optional<double> unitMass;    // кг; нет, если у какой-либо детали нет плотности
    std::optional<double> totalMass;   // unitMass × quantity
    std::vector<BomRow> children;      // только по уровням, для сборок
};

struct Bom {
    std::vector<BomRow> rows;
    std::optional<double> totalMass;  // кг; нет, если масса известна не для всех деталей
    double totalVolume = 0.0;         // мм³
    int totalParts = 0;               // число вхождений деталей
};

// Ошибки: InvalidInput — деталь без тела (объём не определён), отрицательная плотность.
kernel::Result<Bom> flatBom(const AssemblyDocument& document);
kernel::Result<Bom> structuredBom(const AssemblyDocument& document);

// ---- состав всех исполнений сразу (групповая спецификация ГОСТ 2.113, docs/drawing/SPECIFICATION.md)

struct ConfigurationBomRow {
    DefinitionId definition = 0;
    DefinitionKind kind = DefinitionKind::Part;
    std::string designation;
    std::string name;
    std::string material;
    std::vector<int> quantities;  // по исполнению (индекс — ConfigurationBom::configurations); 0 — нет в нём
    double unitVolume = 0.0;         // мм³
    std::optional<double> unitMass;  // кг
};

struct ConfigurationBomColumn {
    std::string designation;      // как в таблице («-01»)
    std::string fullDesignation;  // «РЦ.100.00.000-01»
    std::string name;
    std::optional<double> totalMass;  // кг; нет, если масса известна не для всех деталей
    double totalVolume = 0.0;
    int totalParts = 0;
};

struct ConfigurationBom {
    std::vector<ConfigurationBomColumn> configurations;  // строки таблицы исполнений по порядку
    // Первый уровень состава (как structuredBom, уровень 0: детали и сборочные единицы
    // верхнего уровня) — то, что пишет спецификация; порядок — первое появление по исполнениям.
    std::vector<ConfigurationBomRow> rows;
    // Все детали по всему дереву (как flatBom) — количества в изделии.
    std::vector<ConfigurationBomRow> parts;
};

// Состав каждого исполнения таблицы документа: на копии документа исполнения активируются по
// очереди без решения сопряжений (состав от положений не зависит), строки — по определению
// (одно определение — одно исполнение компонента); определения с теми же видом, обозначением,
// наименованием, материалом, объёмом и массой — одна строка (definition — первое из них). Таблица без строк — один столбец с текущим
// составом (обозначение — документа). options — загрузка исполнений компонентов, которых ещё нет
// в документе. Ошибки: активации исполнения (с его обозначением), BOM (деталь без геометрии).
kernel::Result<ConfigurationBom> configurationBom(const AssemblyDocument& document, const LoadOptions& options = {});

}  // namespace bendcore::assembly_v2
