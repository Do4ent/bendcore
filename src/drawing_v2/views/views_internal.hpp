#pragma once

// Внутренние функции модуля видов: расчёт HLR над произвольной формой с отображением
// подформ на индексы исходной модели, удаление совпадающих линий, проекция рёбер.

#include <bendcore/drawing_v2/views.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <functional>
#include <optional>
#include <vector>

namespace bendcore::drawing_v2::views::detail {

using IndexOf = std::function<std::optional<std::uint32_t>(const TopoDS_Shape&)>;

struct SourceMapper {
    IndexOf edge;  // ребро формы HLR → индекс ребра исходной модели
    IndexOf face;  // грань формы HLR → индекс грани исходной модели
    // Переопределение класса ребра (например, контур сечения в разрезе).
    std::function<std::optional<EdgeClass>(const TopoDS_Edge&)> edgeClass;
    // Рёбра формы HLR, которые не изображаются (уступы ступенчатого разреза, стенки выреза
    // местного разреза — ГОСТ 2.305, разд. 6).
    std::function<bool(const TopoDS_Shape&)> skip;
};

// Отображение по TopologyIndex самой формы (точное совпадение подформ).
SourceMapper identityMapper(const kernel::TopologyIndex& index);

Result<ProjectedView> runHlr(const TopoDS_Shape& shape, const ViewFrame& frame,
                             const HlrOptions& options, const SourceMapper& mapper);

// Участок кривой [u0, u1] ребра в координатах вида: отрезок, дуга/окружность (окружность,
// ось которой параллельна направлению взгляда), иначе ломаная с прогибом deflection;
// почти прямая ломаная становится отрезком.
std::optional<Curve2> projectCurve(const BRepAdaptor_Curve& curve, double u0, double u1,
                                   const ViewFrame& frame, double deflection);

// Точки ребра в порядке его ориентации (для контуров штриховки).
std::vector<Point2> edgePoints(const TopoDS_Edge& edge, const ViewFrame& frame,
                               double deflection);

// Удаление участков, совпадающих с участками более высокого приоритета: видимые перед
// невидимыми; резкие, очерк, контур сечения перед линиями перехода; длиннее — раньше.
void removeOverlaps(std::vector<ViewEdge>& edges);

// Объединение подряд идущих отрезков одного источника, класса и видимости в ломаные
// (для Polygonal).
void chainSegments(std::vector<ViewEdge>& edges);

Box2 boundsOf(const std::vector<ViewEdge>& edges);
void addCurveToBox(Box2& box, const Curve2& c);

}  // namespace bendcore::drawing_v2::views::detail
