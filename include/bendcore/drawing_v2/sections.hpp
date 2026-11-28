#pragma once

// Разрезы, сечения и выносные элементы (фаза 4, п. 3; ГОСТ 2.305-2008, разделы 6–8):
// полный разрез одной плоскостью, ступенчатый (несколько параллельных плоскостей), местный
// (замкнутая область на виде и глубина), условности разрезов сборок (детали, которые не
// рассекаются, — ГОСТ 2.305, разд. 9; ГОСТ 2.109), разная штриховка смежных деталей (ГОСТ 2.306).

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
    // Для каждой области — номер детали (дочерней формы компаунда при SectionParts::perChild),
    // которой принадлежит грань сечения; 0 — форма рассекается как одна деталь.
    std::vector<int> regionOwner;
    double sectionArea = 0.0;  // площадь фигуры сечения, мм² модели (BRepGProp)
    kernel::Shape remaining;   // оставшаяся часть модели (для осевых линий)
    // Местный разрез: участки границы области, под которыми есть материал (там, где граница
    // пересекает модель), — ломаные в координатах вида; выводятся сплошной волнистой линией
    // (ГОСТ 2.303, табл. 1, п. 3). Замкнутый участок — первая точка повторена в конце.
    std::vector<std::vector<Point2>> breakLines;
};

// Разрез сборки (ГОСТ 2.305-2008, разд. 9; ГОСТ 2.109-73; ISO 128-50): детали,
// которые в разрезе показываются нерассечёнными. perChild — дочерние формы компаунда
// (TopoDS_Iterator, порядок развёртки сборки) — отдельные детали: фигура сечения каждой получает
// свой номер (regionOwner) и свою штриховку; keepWhole[i] — дочерняя форма i не рассекается
// (крепёж, валы, оси при продольном разрезе, шарики). Пустой keepWhole — рассекаются все.
struct SectionParts {
    bool perChild = false;
    std::vector<bool> keepWhole;
};

// Полный разрез/сечение одной плоскостью. Плоскость вида должна быть параллельна секущей
// плоскости (frame.toward = −sight, допуск 1e-9), иначе InvalidInput. Плоскость, не
// пересекающая модель, — InvalidInput. Отрезание — BRepAlgoAPI_Cut ящиком со стороны
// наблюдателя; фигура сечения — грани результата, лежащие в секущей плоскости.
Result<SectionView> sectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                 const CuttingPlane& plane, SectionKind kind = SectionKind::Cut,
                                 const HlrOptions& options = {}, const SectionParts& parts = {});

// Ступенчатый разрез (ГОСТ 2.305-2008, разд. 6 — сложный разрез из параллельных плоскостей):
// плоскости проходят через planePoints, все параллельны плоскости вида (взгляд −frame.toward).
// stepAxis — направление в плоскости вида, вдоль которого плоскости сменяют друг друга (след
// ступенчатой линии на виде обозначения): точки упорядочиваются по stepAxis, плоскость i
// действует в полосе между серединами соседних точек, крайние — до бесконечности. По ГОСТ 2.305
// разрез изображается совмещённым в одну плоскость: линии уступов (стенки между плоскостями) не
// изображаются, фигуры сечения соседних плоскостей штрихуются как одна. Ошибки InvalidInput: меньше
// двух точек, stepAxis не перпендикулярен направлению взгляда, две точки в одной полосе
// (совпадают по stepAxis), плоскости не пересекают модель.
Result<SectionView> steppedSectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                        const std::vector<Vec3>& planePoints, Vec3 stepAxis,
                                        SectionKind kind = SectionKind::Cut,
                                        const HlrOptions& options = {},
                                        const SectionParts& parts = {});

// Граница местного разреза — замкнутый контур в координатах вида (мм модели): ломаная по
// вершинам или замкнутый интерполяционный кубический сплайн через точки (spline = true,
// GeomAPI_Interpolate, периодический). Не менее 3 точек, контур без самопересечений.
struct LocalBoundary {
    std::vector<Point2> points;
    bool spline = false;
};

// Местный разрез (ГОСТ 2.305-2008, разд. 6, «местный разрез»; номер пункта — docs/STANDARDS_VERIFICATION.md):
// внутри области boundary материал удаляется от наблюдателя до плоскости, параллельной
// плоскости вида, на глубине depth (мм модели от самой близкой к наблюдателю точки модели);
// внутри — фигура сечения со штриховкой и то, что за плоскостью, снаружи — исходный вид.
struct LocalSection {
    LocalBoundary boundary;
    double depth = 0.0;
};

// Контур границы в координатах вида: вершины ломаной или точки сплайна с прогибом deflection.
// Ошибки InvalidInput: меньше 3 точек, нечисловые координаты, совпадающие соседние точки,
// самопересечение, нулевая площадь.
Result<std::vector<Point2>> localBoundaryLoop(const LocalBoundary& boundary, double deflection = 0.01);

// Вид с местными разрезами (один или несколько на виде). Рёбра, лежащие на стенках выреза
// (проекция — граница области), не выводятся; вместо них — breakLines. Ошибки InvalidInput:
// неверная граница, depth ≤ 0 или за моделью, разрез не задевает модель.
Result<SectionView> localSectionShape(const kernel::Shape& shape, const ViewFrame& frame,
                                      const std::vector<LocalSection>& cuts,
                                      const HlrOptions& options = {},
                                      const SectionParts& parts = {});

// Расстояние вдоль frame.toward от начала координат до самой близкой к наблюдателю точки
// модели (max x·toward по точному габариту BRepBndLib::AddOptimal в системе вида).
Result<double> frontExtent(const kernel::Shape& shape, const ViewFrame& frame);

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
