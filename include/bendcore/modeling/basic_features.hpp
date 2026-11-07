#pragma once

// Базовые операции документа детали, реализованные прямо на OCCT с полной историей:
//   Box       — коробка (length, width, height, x, y, z)
//   Cylinder  — цилиндр по оси Z (radius, height, x, y, z)
//   PrismCut  — вырез призмой с профилем на плоской грани (ссылка "face"; опции profile =
//               rectangle|circle, extent = blind|through; centerX/Y/Z, sizeU, sizeV, radius, depth)
//   Fillet    — скругление рёбер (ссылки "edges", radius)
//   Chamfer   — равносторонняя фаска (ссылки "edges", distance)
// Box и Cylinder при непустом входе сплавляются с ним (BRepAlgoAPI_Fuse).
// Это рабочие операции; при интеграции их заменят операции из src/modeling/features/
// (см. docs/modeling/PART_DOCUMENT.md, раздел «Операции»).

#include <bendcore/modeling/part_document.hpp>

#include <memory>

namespace bendcore::modeling::basic {

// Registers Box, Cylinder, PrismCut, Fillet, Chamfer. Fails if a type is already registered.
Status registerBasicFeatures(FeatureRegistry& registry);

// A registry with the basic features only.
std::shared_ptr<const FeatureRegistry> basicRegistry();

}  // namespace bendcore::modeling::basic
