#pragma once

// Разрезы, сечения и выносные элементы (фаза 4, п. 3; ГОСТ 2.305-2008, разделы 6–8).

#include <bendcore/drawing_v2/views.hpp>

namespace bendcore::drawing_v2::views {

// Секущая плоскость: точка и направление взгляда sight (от наблюдателя к модели, по
// стрелкам обозначения). Удаляется часть модели между наблюдателем и плоскостью:
// {x : (x − point)·sight < 0}.
struct CuttingPlane {
    Vec3 point;
    Vec3 sight{0, 1, 0};
};

enum class SectionKind {
    Cut,    // разрез: фигура сечения и всё, что за плоскостью (ГОСТ 2.305, разд. 6)
    Slice,  // сечение: только фигура в секущей плоскости (ГОСТ 2.305, разд. 7)
};

struct SectionView {
    ProjectedView view;  // рёбра в системе координат вида; привязка — к исходной форме
    // Контуры фигуры сечения в координатах вида (по одному списку контуров на грань сечения;
    // внешний и внутренние контуры — по правилу чётности, как в Hatch).
    std::vector<std::vector<std::vector<Point2>>> regions;
    double sectionArea = 0.0;  // площадь фигуры сечения, мм² модели (BRepGProp)
    kernel::Shape remaining;   // оставшаяся часть модели (для осевых линий)
};

// Полный разрез/сечение одной плоскостью. Плоскость вида должна быть параллельна секущей
// плоскости (frame.toward = −sight, допуск 1e-9), иначе InvalidInput. Плоскость, не
// пересекающая модель, — InvalidInput. Отрезание — BRepAlgoAPI_Cut ящиком со стороны
// наблюдателя; фигура сечения — грани результата, лежащие в секущей плоскости.
Result<SectionView> sectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                 const CuttingPlane& plane, SectionKind kind = SectionKind::Cut,
                                 const HlrOptions& options = {});

// След секущей плоскости на виде parent (координаты вида): отрезок прямой пересечения,
// продлённый до границ bounds. Плоскость должна быть перпендикулярна плоскости вида
// (sight ⟂ parent.toward), иначе InvalidInput.
struct CuttingTrace {
    Point2 a, b;     // концы следа на границе bounds
    Point2 sight2d;  // единичное направление взгляда в плоскости вида parent
};
Result<CuttingTrace> cuttingTrace(const ViewFrame& parent, const Box2& bounds,
                                  const CuttingPlane& plane);

// Выносной элемент (ГОСТ 2.305, раздел 8): рёбра вида parent, отсечённые окружностью
// (center, radius) в координатах вида. Дуги и окружности отсекаются точно, ломаные — по
// звеньям. Привязка рёбер к модели сохраняется. Габарит (bounds) включает окружность
// ограничения.
Result<ProjectedView> detailView(const ProjectedView& parent, Point2 center, double radius);

}  // namespace bendcore::drawing_v2::views
