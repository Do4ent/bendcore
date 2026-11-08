#pragma once

// Общие модели тестов аннотаций: брусок с отверстием как документ детали (карта имён с
// историей), поиск элементов по геометрии.

#include <bendcore/drawing_v2/dimensions.hpp>
#include <bendcore/modeling/basic_features.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <optional>
#include <string>

namespace annot_test {

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::annotations;
using namespace bendcore::drawing_v2::views;

// Брусок L×W×H от начала координат со сквозным отверстием радиуса r в (hx, hy), заданный как
// документ детали: Box «Body» + PrismCut «Hole» (circle, through) на верхней грани.
struct BlockPart {
    bendcore::modeling::PartDocument doc{bendcore::modeling::basic::basicRegistry()};
    BlockPart(double L = 60, double W = 40, double H = 20, double hx = 30, double hy = 20, double r = 5);
    void set(const std::string& var, double value);  // setVariable + rebuild
    ModelBinding binding() const;
};

// Индекс прямолинейного ребра с концами a и b (в любом порядке); nullopt — нет такого.
std::optional<std::uint32_t> lineEdge(const ModelBinding& m, Vec3 a, Vec3 b);
// Индекс ребра-окружности радиуса r с центром c.
std::optional<std::uint32_t> circleEdge(const ModelBinding& m, Vec3 c, double r);
// Индекс цилиндрической грани радиуса r.
std::optional<std::uint32_t> cylinderFace(const ModelBinding& m, double r);
// Индекс вершины в точке p.
std::optional<std::uint32_t> vertexAt(const ModelBinding& m, Vec3 p);

Anchor anchor(const ModelBinding& m, RefKind kind, std::uint32_t index, AnchorKind ak);

}  // namespace annot_test
