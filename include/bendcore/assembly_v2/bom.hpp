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

}  // namespace bendcore::assembly_v2
