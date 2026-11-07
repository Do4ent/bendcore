#pragma once

// Операции детали как чистые функции над OCCT: выдавливание, вращение, отверстие,
// скругление, фаска, массивы, зеркало, оболочка, уклон граней.
// Каждая операция возвращает новое тело, инструмент (если он есть) и историю
// BRepTools_History, по которой работает топологическое именование.
// Подробности: docs/modeling/FEATURES.md.

#include <bendcore/kernel/result.hpp>

#include <BRepTools_History.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::modeling {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

// Результат операции.
//   shape   — тело после операции (TopoDS_Solid или TopoDS_Compound из нескольких тел);
//   tool    — тело-инструмент, которое объединялось/вычиталось (пусто для скругления,
//             фаски, оболочки, уклона);
//   history — история от входов (профиль, целевое тело, инструмент) к shape:
//             Modified — элемент входа перешёл в изменённый элемент результата,
//             Generated — элемент результата построен из элемента входа,
//             IsRemoved — элемент входа исчез. Неупомянутый элемент входа, который есть в
//             shape, перешёл без изменений;
//   warnings — предупреждения (OCCT, многотельный результат и т.п.).
struct FeatureResult {
    TopoDS_Shape shape;
    TopoDS_Shape tool;
    opencascade::handle<BRepTools_History> history;
    std::vector<std::string> warnings;
};

// Плоский профиль эскиза: грань (внешний контур и, возможно, отверстия) и система
// координат плоскости эскиза. Направление выдавливания по умолчанию — placement.Direction().
struct Profile {
    TopoDS_Face face;
    gp_Ax3 placement;
};

// Boss — добавить материал (объединение с целевым телом; без целевого тела — новое тело).
// Cut — удалить материал (вычитание из целевого тела, оно обязательно).
enum class FeatureMode { Boss, Cut };

const char* toString(FeatureMode mode) noexcept;

// ---------------------------------------------------------------- выдавливание

enum class ExtentType {
    Blind,           // на distance по направлению
    Symmetric,       // distance всего, по distance/2 в обе стороны от плоскости эскиза
    TwoSided,        // distance по направлению и distance2 против него
    ThroughAll,      // через всё целевое тело по направлению
    ThroughAllBoth,  // через всё в обе стороны
    UpToFace,        // до грани limit (ограниченной её контуром)
    UpToSurface,     // до поверхности грани limit, продолженной за границы грани
};

const char* toString(ExtentType type) noexcept;

enum class DraftDirection { Inward, Outward };  // сужение / расширение от плоскости эскиза
enum class ThinSide { Inside, Outside };        // стенка внутрь / наружу от контура профиля

struct ThinWall {
    double thickness = 1.0;  // мм, > 0
    ThinSide side = ThinSide::Inside;
};

struct ExtrudeParams {
    FeatureMode mode = FeatureMode::Boss;
    ExtentType extent = ExtentType::Blind;
    double distance = 10.0;   // Blind, Symmetric, TwoSided (первое направление)
    double distance2 = 0.0;   // TwoSided: против направления
    bool reversed = false;    // направление = -placement.Direction()
    TopoDS_Face limit;        // UpToFace, UpToSurface
    double draftAngle = 0.0;  // рад, [0, π/2); 0 — без уклона
    DraftDirection draftDirection = DraftDirection::Inward;
    std::optional<ThinWall> thin;  // тонкостенное выдавливание (без торцов)
};

// target может быть пустым только для Boss. Ошибки: InvalidInput (профиль не плоский или
// не замкнут, неверные параметры), Unsupported (сочетание параметров), OperationFailed (OCCT).
Result<FeatureResult> extrude(const Profile& profile, const TopoDS_Shape& target, const ExtrudeParams& params);

// ---------------------------------------------------------------- вращение

enum class RevolveExtent {
    Angle,      // angle от плоскости эскиза
    Full,       // 360°
    Symmetric,  // angle всего, по angle/2 в обе стороны
    TwoSided,   // angle в положительную сторону, angle2 — в отрицательную
};

struct RevolveParams {
    FeatureMode mode = FeatureMode::Boss;
    gp_Ax1 axis;               // лежит в плоскости профиля, профиль по одну сторону от оси
    RevolveExtent extent = RevolveExtent::Angle;
    double angle = 0.0;        // рад
    double angle2 = 0.0;       // рад, TwoSided
    bool reversed = false;     // вращение в отрицательную сторону вокруг оси
};

Result<FeatureResult> revolve(const Profile& profile, const TopoDS_Shape& target, const RevolveParams& params);

// ---------------------------------------------------------------- отверстие

enum class HoleType { Simple, Counterbore, Countersink };
enum class HoleEnd { Blind, Through };

struct HoleParams {
    HoleType type = HoleType::Simple;
    HoleEnd end = HoleEnd::Blind;
    double diameter = 5.0;              // мм
    double depth = 10.0;                // мм, глубина цилиндрической части от грани (Blind)
    bool drillPoint = true;             // конус сверла на дне глухого отверстия
    double drillPointAngle = 2.0594885173533086;  // рад, 118°
    double counterboreDiameter = 0.0;   // Counterbore
    double counterboreDepth = 0.0;      // Counterbore
    double countersinkDiameter = 0.0;   // Countersink: диаметр на грани
    double countersinkAngle = 1.5707963267948966;  // рад, 90° (угол при вершине конуса)
};

// Отверстие в target с центром point на плоской грани face (грань target).
// direction — ось сверления; по умолчанию — против внешней нормали грани (в материал).
Result<FeatureResult> hole(const TopoDS_Shape& target, const TopoDS_Face& face, const gp_Pnt& point,
                           const HoleParams& params, const std::optional<gp_Dir>& direction = std::nullopt);

// ---------------------------------------------------------------- скругление и фаска

// Ребро входа, на котором операция не построилась.
struct EdgeFailure {
    std::size_t index = 0;  // индекс в переданном списке рёбер
    TopoDS_Shape edge;
    std::string reason;
};

// Скругление постоянного радиуса. При неудаче сообщение ошибки перечисляет неудачные рёбра
// (их индексы во входном списке и причины).
Result<FeatureResult> fillet(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges, double radius);

// Диагностика: рёбра, на которых скругление радиуса radius не строится (каждое ребро
// пробуется отдельно и в составе цепочки). Пусто — все рёбра строятся.
Result<std::vector<EdgeFailure>> filletFailures(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges,
                                                double radius);

enum class ChamferType {
    EqualDistance,    // distance на обеих гранях
    TwoDistances,     // distance на опорной грани, distance2 на другой
    DistanceAngle,    // distance на опорной грани, angle между опорной гранью и фаской
};

struct ChamferParams {
    ChamferType type = ChamferType::EqualDistance;
    double distance = 1.0;
    double distance2 = 1.0;
    double angle = 0.7853981633974483;  // рад, 45°
    // Опорная грань для TwoDistances / DistanceAngle: одна грань на все рёбра (должна
    // примыкать к каждому) или пусто — первая примыкающая грань ребра (предупреждение).
    TopoDS_Shape referenceFace;
};

Result<FeatureResult> chamfer(const TopoDS_Shape& target, const std::vector<TopoDS_Shape>& edges,
                              const ChamferParams& params);

// ---------------------------------------------------------------- массивы и зеркало

// Экземпляр с индексом 0 — сам инструмент (он уже применён к target); операция
// добавляет копии 1..count-1 и выполняет одну булеву операцию mode.
struct LinearPatternParams {
    gp_Dir direction1;
    double spacing1 = 10.0;
    int count1 = 2;
    std::optional<gp_Dir> direction2;
    double spacing2 = 10.0;
    int count2 = 1;
};

struct CircularPatternParams {
    gp_Ax1 axis;
    int count = 2;
    double angle = 6.283185307179586;  // рад, общий угол
    // true — экземпляры равномерно на angle (при angle = 2π шаг angle/count, иначе
    // angle/(count-1)); false — angle это шаг между соседними экземплярами.
    bool equalSpacing = true;
};

// target — тело с уже применённой исходной операцией; tool — её FeatureResult::tool.
// В истории: грани/рёбра копий как входы (Modified/IsRemoved), а также
// Generated(элемент исходного инструмента) → образы копий в результате.
Result<FeatureResult> linearPattern(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                    const LinearPatternParams& params);
Result<FeatureResult> circularPattern(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                      const CircularPatternParams& params);

// Зеркало операции: зеркальная копия инструмента относительно плоскости
// (plane.Location(), нормаль plane.Direction()) и булева операция mode с target.
Result<FeatureResult> mirrorFeature(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                    const gp_Ax2& plane);

// Зеркало тела: merge = true — объединение тела с зеркальной копией, иначе — составное из
// двух тел. В истории: Generated(элемент тела) → образ зеркального элемента.
Result<FeatureResult> mirrorBody(const TopoDS_Shape& body, const gp_Ax2& plane, bool merge);

// ---------------------------------------------------------------- оболочка и уклон

enum class ShellDirection { Inward, Outward };

// Оболочка: удаляемые грани removedFaces (грани body; может быть пусто — полое тело),
// толщина thickness > 0 внутрь или наружу (стыки острые, GeomAbs_Intersection).
Result<FeatureResult> shell(const TopoDS_Shape& body, const std::vector<TopoDS_Shape>& removedFaces,
                            double thickness, ShellDirection direction);

// Уклон граней: грани faces поворачиваются на angle вокруг линии пересечения с
// нейтральной плоскостью; pullDirection — направление извлечения из формы.
// angle > 0 — грань наклоняется внутрь тела по мере движения вдоль pullDirection.
Result<FeatureResult> draftFaces(const TopoDS_Shape& body, const std::vector<TopoDS_Shape>& faces,
                                 const gp_Dir& pullDirection, double angle, const gp_Ax3& neutralPlane);

}  // namespace bendcore::modeling
