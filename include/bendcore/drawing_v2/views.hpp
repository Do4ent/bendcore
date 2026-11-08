#pragma once

// Виды чертежа по модели (фаза 4, п. 2): ориентация видов по ГОСТ 2.305-2008 (метод первого
// угла, ЕСКД; метод третьего угла ISO 128-30 — опция), аксонометрия по ГОСТ 2.317-2011,
// расчёт видимых и невидимых линий (HLR) средствами OCCT. Документация — docs/drawing/VIEWS.md.
//
// Система координат вида: начало — начало координат модели, ось x — `right`, ось y — `up`,
// ось z — `toward` (к наблюдателю). Точка модели P на виде: (P·right, P·up), мм модели.

#include <bendcore/drawing_v2/model.hpp>
#include <bendcore/kernel/shape.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bendcore::drawing_v2::views {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(double k, Vec3 a);
double dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
double length(Vec3 a);

// Ортонормированная правая тройка вида: right × up = toward.
struct ViewFrame {
    Vec3 toward{0, -1, 0};  // от модели к наблюдателю (обратно направлению взгляда)
    Vec3 right{1, 0, 0};
    Vec3 up() const { return cross(toward, right); }
    // Проекция точки модели на плоскость вида, мм модели.
    Point2 project(Vec3 p) const;
};

// Проверяет и ортонормирует: toward и right ненулевые и неколлинеарные; right
// ортогонализуется к toward.
Result<ViewFrame> makeFrame(Vec3 toward, Vec3 right);

// Главный вид по умолчанию — вид спереди: наблюдатель со стороны −Y, ось Z вверх,
// ось X вправо.
ViewFrame defaultFrontFrame();

// ГОСТ 2.305-2008, разд. 5: шесть основных видов.
enum class StandardView { Front, Top, Left, Right, Bottom, Rear };

// Метод проецирования: FirstAngle — ЕСКД (ГОСТ 2.305, разд. 4; ISO 128-30 «метод E»),
// ThirdAngle — ISO 128-30 «метод A».
enum class ProjectionMethod { FirstAngle, ThirdAngle };

// Положение вида на листе относительно родительского вида.
enum class SheetSide { Above, Below, LeftOf, RightOf };

// Ориентация основного вида при заданном главном виде (вид сверху — наблюдатель сверху и т. д.).
ViewFrame standardFrame(const ViewFrame& main, StandardView view);

// Где располагается основной вид относительно главного (ГОСТ 2.305, разд. 5, рис. 1).
// Вид сзади ставится справа от вида слева (FirstAngle) или справа от вида справа
// (ThirdAngle); для него возвращается RightOf относительно этого соседнего вида. Для
// главного вида — nullopt.
std::optional<SheetSide> standardPlacement(StandardView view, ProjectionMethod method);

// Проекционный вид от любого вида: ориентация вида, который ставится на листе со стороны
// side родительского вида при данном методе проецирования.
ViewFrame projectedFrame(const ViewFrame& parent, SheetSide side, ProjectionMethod method);

// Дополнительный вид (ГОСТ 2.305, разд. 5): направление взгляда sight (от наблюдателя к
// модели), upHint — желаемое направление «вверх» (проецируется на плоскость вида).
Result<ViewFrame> auxiliaryFrame(Vec3 sight, Vec3 upHint);

// Аксонометрия ГОСТ 2.317-2011.
enum class Axonometry { Isometric, Dimetric };
// С какой стороны от главного вида смотрит наблюдатель: спереди-справа-сверху или
// спереди-слева-сверху.
enum class AxonometricCorner { FrontRightTop, FrontLeftTop };

// Ортогональная аксонометрия относительно главного вида: изометрия — направление
// (±right + toward + up)/√3; диметрия — коэффициенты искажения kx = kz = 2√2/3 ≈ 0,94,
// ky = √2/3 ≈ 0,47, где ось y — глубина главного вида.
ViewFrame axonometricFrame(const ViewFrame& main, Axonometry type,
                           AxonometricCorner corner = AxonometricCorner::FrontRightTop);

// Множитель изображения для приведённых коэффициентов ГОСТ 2.317 (изометрия: 1 по всем
// осям — увеличение √(3/2) ≈ 1,22; диметрия: 1 по x и z, 0,5 по y — увеличение
// 3/(2√2) ≈ 1,06). При reduced = false — 1 (точные коэффициенты 0,82 / 0,94 и 0,47).
double axonometricFactor(Axonometry type, bool reduced);

// --- HLR ---

enum class HlrMethod {
    Exact,      // HLRBRep_Algo: точные кривые
    Polygonal,  // HLRBRep_PolyAlgo: по триангуляции, отрезки
};

// Класс 2D-ребра (HLRBRep_HLRToShape: sharp / Rg1Line / OutLine).
enum class EdgeClass {
    Sharp,       // ребро модели с изломом между гранями (или свободное ребро)
    Smooth,      // ребро между гранями с непрерывной касательной (линия перехода)
    Silhouette,  // очерк (контурная линия) криволинейной грани
    SectionCut,  // контур фигуры сечения (только в разрезах и сечениях)
};

enum class Visibility { Visible, Hidden };

using Curve2 = std::variant<LineSeg, Arc, Circle, Polyline>;

struct ViewEdge {
    Curve2 curve;  // координаты вида, мм модели
    Visibility visibility = Visibility::Visible;
    EdgeClass cls = EdgeClass::Sharp;
    // Привязка к модели — индексы kernel::TopologyIndex исходной формы. У очерка указана
    // грань, у рёбер — ребро. Для разрезов — индексы исходной (не разрезанной) формы.
    std::optional<std::uint32_t> edgeIndex;
    std::optional<std::uint32_t> faceIndex;
};

struct Box2 {
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool empty = true;
    void add(Point2 p);
    double width() const { return empty ? 0.0 : maxX - minX; }
    double height() const { return empty ? 0.0 : maxY - minY; }
};

struct HlrStats {
    double milliseconds = 0.0;  // время расчёта HLR (без разметки и пост-обработки)
    std::size_t rawPieces = 0;  // участков от OCCT до удаления совпадающих линий
};

struct ProjectedView {
    ViewFrame frame;
    std::vector<ViewEdge> edges;
    Box2 bounds;  // по всем рёбрам (видимым и невидимым)
    HlrStats stats;
};

struct HlrOptions {
    HlrMethod method = HlrMethod::Exact;
    bool hidden = true;       // считать невидимые линии
    bool smooth = true;       // линии перехода (видимые)
    bool hiddenSmooth = false;
    // Polygonal: прогиб триангуляции, мм и рад (BRepMesh_IncrementalMesh).
    double linearDeflection = 0.1;
    double angularDeflection = 0.3;
    // Прогиб ломаной для кривых, которые не являются отрезком или окружностью на виде
    // (эллипсы, сплайны), мм модели.
    double curveDeflection = 0.01;
};

// Расчёт вида. Невидимые участки, совпадающие с видимыми, и повторяющиеся видимые участки
// удаляются (видимая линия имеет приоритет, ГОСТ 2.303). Ошибки: пустая форма —
// InvalidInput; сбой OCCT — OperationFailed.
Result<ProjectedView> projectShape(const kernel::Shape& shape, const ViewFrame& frame,
                                   const HlrOptions& options = {});

// Длина кривой (для тестов и статистики).
double curveLength(const Curve2& c);

}  // namespace bendcore::drawing_v2::views
