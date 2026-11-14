#pragma once

// Операции детали как чистые функции над OCCT: выдавливание, вращение, отверстие,
// скругление, фаска, массивы, зеркало, оболочка, уклон граней, кинематика, по сечениям,
// ребро жёсткости, резьбовое отверстие, булевы операции и копии тел многотельной детали.
// Каждая операция возвращает новое тело, инструмент (если он есть) и историю
// BRepTools_History, по которой работает топологическое именование.
// Подробности: docs/modeling/FEATURES.md.

#include <bendcore/kernel/result.hpp>

#include <BRepTools_History.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
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
enum class ThreadHand { Right, Left };

// Параметры условной резьбы, которые получает грань (атрибуты карты элементов «thread.*»,
// docs/modeling/FEATURES.md, раздел «Резьбовое отверстие»).
struct ThreadSpec {
    std::string standard = "ISO 261";  // ряд шагов
    std::string designation;           // M10, M10x1.25 (крупный шаг в обозначении не пишется)
    double nominal = 0.0;              // D, мм
    double pitch = 0.0;                // P, мм
    double minor = 0.0;                // D1, мм
    double drill = 0.0;                // диаметр отверстия под резьбу, мм
    double length = 0.0;               // длина резьбы от входа, мм
    ThreadHand hand = ThreadHand::Right;
    bool internal = true;              // внутренняя резьба (в отверстии)
    gp_Pnt entry;                      // центр входа резьбы
    gp_Dir direction = gp_Dir(0, 0, -1);  // направление резьбы от входа (в материал)
};

struct ThreadedFace {
    TopoDS_Face face;  // цилиндрическая стенка отверстия под резьбу
    ThreadSpec thread;
};


struct FeatureResult {
    TopoDS_Shape shape;
    TopoDS_Shape tool;
    opencascade::handle<BRepTools_History> history;
    std::vector<std::string> warnings;
    // Условная резьба (threadedHole): грани shape с параметрами резьбы.
    std::vector<ThreadedFace> threads;
    // Элементы, названные самой операцией (ребро жёсткости: SideA, SideB, Top, Wall.E<k>…):
    // элемент инструмента или результата и его роль; в результат они попадают по history.
    std::vector<std::pair<TopoDS_Shape, std::string>> roles;
    // Массивы и зеркала: преобразования копий (без исходного экземпляра) — по ним копии
    // условной резьбы получают атрибуты (threadCopies).
    std::vector<gp_Trsf> copies;
};

// Плоский профиль эскиза: грань (внешний контур и, возможно, отверстия) и система
// координат плоскости эскиза. Направление выдавливания по умолчанию — placement.Direction().
struct Profile {
    TopoDS_Face face;
    gp_Ax3 placement;
};

// Boss — добавить материал (объединение с телами цели, которых касается инструмент; без
//        целевого тела — новое тело).
// Cut — удалить материал (вычитание из тел цели, которые пересекает инструмент; цель обязательна).
// NewBody — инструмент становится отдельным телом многотельной детали (составное: тела цели +
//        инструмент), тела цели не меняются.
enum class FeatureMode { Boss, Cut, NewBody };

const char* toString(FeatureMode mode) noexcept;
// "Boss" / "Cut" / "NewBody" (как toString) или "boss" / "cut" / "newBody"; nullopt — иное.
std::optional<FeatureMode> featureModeFromString(const std::string& text) noexcept;

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

// ---------------------------------------------------------------- кинематика (sweep)

// Ориентация профиля вдоль траектории (BRepOffsetAPI_MakePipeShell::SetMode).
enum class SweepOrientation {
    CorrectedFrenet,  // Френе с поправкой кручения (SetMode(false)) — по умолчанию
    Frenet,           // трёхгранник Френе (SetMode(true))
    Fixed,            // постоянный трёхгранник: профиль переносится без поворота (SetMode(gp_Ax2))
    Binormal,         // постоянная бинормаль binormal (SetMode(gp_Dir))
    Guide,            // по направляющей кривой guide (SetMode(wire, curvilinear, contact))
};

struct SweepParams {
    FeatureMode mode = FeatureMode::Boss;
    SweepOrientation orientation = SweepOrientation::CorrectedFrenet;
    gp_Dir binormal = gp_Dir(0, 0, 1);  // Binormal
    TopoDS_Wire guide;                  // Guide: направляющая, задаёт поворот профиля вокруг траектории
    std::optional<ThinWall> thin;       // тонкостенная кинематика: стенка вдоль контуров профиля
};

// Профиль заметается вдоль траектории path (связная цепочка рёбер, открытая или замкнутая).
// Начало траектории лежит в плоскости профиля (1e-6 · габарит). Отверстия профиля заметаются
// отдельно и вычитаются. Ошибки: InvalidInput (профиль, траектория, направляющая),
// OperationFailed (MakePipeShell не построил тело, самопересечение).
Result<FeatureResult> sweep(const Profile& profile, const TopoDS_Wire& path, const TopoDS_Shape& target,
                            const SweepParams& params);

// Связная цепочка рёбер → проволока (порядок рёбер любой, ориентация подбирается).
// Ошибки: пустой список, не ребро, разрыв, ветвление.
Result<TopoDS_Wire> chainEdges(const std::vector<TopoDS_Shape>& edges);

// ---------------------------------------------------------------- по сечениям (loft)

struct LoftParams {
    FeatureMode mode = FeatureMode::Boss;
    bool ruled = false;   // true — линейчатые грани между соседними сечениями, false — гладкие
    bool closed = false;  // замкнуть: последнее сечение соединяется с первым (≥ 3 сечения)
};

// Тело по сечениям (BRepOffsetAPI_ThruSections) через профили sections (≥ 2, в разных
// плоскостях, порядок — порядок обхода). Сечения с отверстиями: число контуров у всех сечений
// одинаково, внутренние контуры соединяются по порядку и вычитаются.
Result<FeatureResult> loft(const std::vector<Profile>& sections, const TopoDS_Shape& target, const LoftParams& params);

// ---------------------------------------------------------------- ребро жёсткости

enum class RibDirection {
    ParallelToSketch,       // материал в плоскости эскиза между линией и телом, толщина — по нормали
    PerpendicularToSketch,  // линия утолщается в плоскости эскиза и выдавливается по нормали до тела
};
enum class RibThickness { Symmetric, FirstSide, SecondSide };

struct RibParams {
    double thickness = 2.0;  // мм, > 0
    RibDirection direction = RibDirection::ParallelToSketch;
    // Symmetric — ±t/2; FirstSide — [0, t] по нормали эскиза (Parallel) или по нормали к хорде
    // n = нормаль эскиза × хорда (Perpendicular); SecondSide — [−t, 0].
    RibThickness side = RibThickness::Symmetric;
    // Сторона материала: ParallelToSketch — +n (false) или −n (true) от линии в плоскости
    // эскиза; PerpendicularToSketch — по нормали эскиза (false) или против неё (true).
    // nullopt — сторона, с которой ребро упирается в тело.
    std::optional<bool> flip;
};

// Открытый контур contour (рёбра в плоскости placement) → ребро жёсткости до граней target,
// объединённое с target. Ошибки: InvalidInput (контур замкнут, не в плоскости, толщина),
// OperationFailed (ребро не доходит до тела ни с одной стороны).
Result<FeatureResult> rib(const TopoDS_Wire& contour, const gp_Ax3& placement, const TopoDS_Shape& target,
                          const RibParams& params);

// ---------------------------------------------------------------- резьбовое отверстие

enum class ThreadSeries { Coarse, Fine };

// Метрическая резьба: ISO 261 / ГОСТ 8724 (ряды шагов), профиль ISO 68-1 / ГОСТ 9150,
// диаметр под сверление ISO 2306 / ГОСТ 19257 (D − P). Таблица — docs/modeling/FEATURES.md,
// источники и статус сверки — docs/STANDARDS_VERIFICATION.md.
struct MetricThread {
    double nominal = 0.0;  // D, мм
    double pitch = 0.0;    // P, мм
    ThreadSeries series = ThreadSeries::Coarse;
};

// Крупный шаг номинала из таблицы; ошибка — номинала нет в таблице.
Result<MetricThread> metricCoarse(double nominal);
// Мелкие шаги номинала из таблицы (может быть пусто); ошибка — номинала нет в таблице.
Result<std::vector<double>> metricFinePitches(double nominal);
// Пара (D, P) из таблицы (крупный или один из мелких шагов). Ошибка — нет такого сочетания.
Result<MetricThread> metricThread(double nominal, double pitch);
// Все номиналы таблицы по возрастанию.
std::vector<double> metricNominals();
// Диаметр сверла под резьбу: D − P (ISO 2306 для 6H).
double tapDrillDiameter(const MetricThread& t);
// Внутренний диаметр внутренней резьбы D1 = D − 1,082532·P (ISO 724 / ГОСТ 24705).
double minorDiameter(const MetricThread& t);
// Обозначение: M10, M10x1.25 (мелкий шаг), с суффиксом LH для левой резьбы.
std::string threadDesignation(const MetricThread& t, ThreadHand hand);

// Атрибуты «thread.*» (строки) ↔ ThreadSpec.
std::vector<std::pair<std::string, std::string>> threadAttributes(const ThreadSpec& spec);
Result<ThreadSpec> threadFromAttributes(const std::vector<std::pair<std::string, std::string>>& attributes);

// Резьба после преобразования копии (массив, зеркало): вход и направление переносятся
// преобразованием; номинал, шаг, длина и направление витков (hand) не меняются — зеркальная
// копия резьбового отверстия остаётся резьбой того же обозначения (ГОСТ 2.311 изображает её так
// же; левая резьба задаётся явно).
ThreadSpec transformedThread(const ThreadSpec& spec, const gp_Trsf& transform);

// Атрибуты «thread.*» копий: для каждой резьбы seeds и каждого преобразования — цилиндрические
// грани result радиусом drill/2 (отн. допуск 1e-6), ось которых совпадает с осью
// преобразованной резьбы (допуск 1e-6·max(1, D)), а участок оси пересекается с длиной резьбы.
// Пусто, если копия не найдена (копия не пересекла тело).
std::vector<std::pair<TopoDS_Shape, std::vector<std::pair<std::string, std::string>>>> threadCopies(
    const std::vector<ThreadSpec>& seeds, const std::vector<gp_Trsf>& transforms, const TopoDS_Shape& result);

enum class ThreadEnd { Blind, Through };

struct ThreadedHoleParams {
    MetricThread thread;           // номинал и шаг (metricThread / metricCoarse)
    ThreadHand hand = ThreadHand::Right;
    ThreadEnd end = ThreadEnd::Blind;
    double threadLength = 10.0;    // мм; Blind: ≤ drillDepth; Through: 0 — на всю длину отверстия
    double drillDepth = 0.0;       // мм, Blind: глубина отверстия под резьбу; 0 — threadLength + 3·P
    std::optional<double> drillDiameter;  // по умолчанию tapDrillDiameter
    bool drillPoint = true;        // конус 118° на дне глухого отверстия
};

// Отверстие под резьбу (hole() диаметром под сверление) + условная резьба на его стенке:
// FeatureResult::threads — грань стенки с ThreadSpec.
Result<FeatureResult> threadedHole(const TopoDS_Shape& target, const TopoDS_Face& face, const gp_Pnt& point,
                                   const ThreadedHoleParams& params,
                                   const std::optional<gp_Dir>& direction = std::nullopt);

// ---------------------------------------------------------------- тела многотельной детали

// Тела (TopoDS_Solid) формы в порядке TopExp::MapShapes.
std::vector<TopoDS_Shape> solidsOf(const TopoDS_Shape& shape);
// Составное из форм (одна форма — она сама).
TopoDS_Shape makeCompound(const std::vector<TopoDS_Shape>& shapes);

enum class BodyBooleanType { Union, Subtract, Intersect };

// Булева операция между телами детали: target — тело (solid) из bodies, tools — другие тела
// из bodies. keepTools — инструменты остаются отдельными телами. Остальные тела переходят без
// изменений. Результат — составное всех тел. Ошибки: тело не из bodies, target среди tools,
// пустое пересечение / вычитание всего тела.
Result<FeatureResult> bodyBoolean(const TopoDS_Shape& bodies, const TopoDS_Shape& target,
                                  const std::vector<TopoDS_Shape>& tools, BodyBooleanType type, bool keepTools);

// Удаление тел removed из bodies (элементы удалённых тел — IsRemoved). Ошибка — удаляются все.
Result<FeatureResult> deleteBodies(const TopoDS_Shape& bodies, const std::vector<TopoDS_Shape>& removed);

// Копии тел selected по преобразованиям (массив или зеркало тел): копии — новые тела
// (merge = false) или объединяются с исходным телом (merge = true). История:
// Generated(элемент исходного тела) → образ элемента копии.
Result<FeatureResult> copyBodies(const TopoDS_Shape& bodies, const std::vector<TopoDS_Shape>& selected,
                                 const std::vector<gp_Trsf>& transforms, bool merge);

}  // namespace bendcore::modeling
