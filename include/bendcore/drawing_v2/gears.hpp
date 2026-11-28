#pragma once

// Условное изображение зубчатых колёс на чертеже по ГОСТ 2.402-68 и таблица параметров
// зубчатого венца по ГОСТ 2.403-75 — по атрибутам «gear.*» граней венца (modeling::gearsOf,
// операция GearTeeth, docs/modeling/FEATURES.md, «Зубчатый венец»).
//
// Зубья на чертеже не изображаются: модель для видов — «условная» форма, у которой впадины венца
// заполнены до окружности вершин (conventionalGearModel); на неё накладываются линии ГОСТ 2.402:
//   • окружность и образующие вершин — сплошная основная (это контур условной формы);
//   • делительные окружность и образующие — штрихпунктирная тонкая;
//   • образующие впадин на виде — сплошная тонкая, в разрезе — сплошная основная; окружность
//     впадин на виде с торца по умолчанию не изображается (допускается ГОСТ 2.402);
//   • в разрезе плоскостью через ось зуб не рассекается: полоса между окружностями впадин и
//     вершин на ширине венца не штрихуется (withUncutTeeth).
// Документация — docs/drawing/ANNOTATIONS.md, раздел «Зубчатые колёса».

#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/drawing_v2/title_block.hpp>
#include <bendcore/drawing_v2/view_sheet.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Заполнение впадин венца: кольцо от df/2 − m/4 до da/2 на ширине венца.
Result<TopoDS_Shape> gearFiller(const modeling::GearSpec& gear);

// Условная форма: real ∪ заполнения всех венцов (kernel::booleanOp, Fuse, с упрощением).
struct ConventionalShape {
    TopoDS_Shape shape;
    opencascade::handle<BRepTools_History> history;  // от элементов real к shape
};
Result<ConventionalShape> conventionalGearShape(const TopoDS_Shape& real, const std::vector<modeling::GearInfo>& gears);
// То же с картой имён: имена элементов real переходят по истории (вершины зубьев, слитые в
// цилиндр вершин, — Modified), грани зубьев исчезают.
Result<modeling::NamedShape> conventionalGearModel(const modeling::NamedShape& real,
                                                   const std::vector<modeling::GearInfo>& gears);

enum class GearAspect {
    Side,  // ось параллельна плоскости вида
    End,   // ось перпендикулярна плоскости вида
};

// Сторона венца, обращённая к сопряжённому колесу в зацеплении (вид сбоку, gearMeshes).
struct GearMeshSide {
    Point2 toward;         // единичное направление на виде от оси венца к оси сопряжённого колеса
    bool rear = false;     // венец изображается за сопряжённым (его зуб — невидимый в зоне зацепления)
    double s0 = 0.0, s1 = 0.0;  // ширина сопряжённого венца вдоль оси этого, мм модели от origin
};

// Венец на виде (координаты вида, мм модели).
struct GearImage {
    std::size_t gear = 0;  // индекс венца в списке gearImages
    modeling::GearSpec spec;
    modeling::GearGeometry geometry;
    GearAspect aspect = GearAspect::Side;
    bool section = false;   // разрез плоскостью через ось венца
    Point2 origin;          // центр торца начала венца
    Point2 axis{1.0, 0.0};  // Side: единичное направление оси на виде
    double depth = 0.0;     // положение начала венца вдоль toward вида, мм модели
    double along = 0.0;     // ось·toward вида (End: ±1 — венец занимает [depth, depth + along·b])
    // Side, косой зуб: направление линии зуба на ближней к наблюдателю стороне делительного
    // цилиндра на виде (единичное); у прямого зуба — (0, 0).
    Point2 helix{0.0, 0.0};
    std::optional<std::size_t> child;  // сборка: деталь (дочерняя форма компаунда) венца
    std::vector<GearMeshSide> mesh;    // зацепления на этом виде (gearMeshes)
};

struct GearImageOptions {
    bool rootCircleOnEnd = false;     // окружность впадин на виде с торца (тонкая)
    double angularTolerance = 1e-6;   // |cos| — ось параллельна/перпендикулярна плоскости вида
};

// Венцы на виде с рамкой frame: Side или End; венцы на косых видах пропускаются (ГОСТ 2.402
// условностей для аксонометрии не даёт). sectionPlane — секущая плоскость разреза: венец, ось
// которого лежит в ней (1e-6 мм), изображается как в разрезе.
// children — деталь каждого венца в сборке (пусто — деталь не известна, чертёж детали).
Result<std::vector<GearImage>> gearImages(const std::vector<modeling::GearInfo>& gears, const views::ViewFrame& frame,
                                          const std::optional<views::CuttingPlane>& sectionPlane = std::nullopt,
                                          const GearImageOptions& options = {},
                                          const std::vector<std::size_t>& children = {});

// Линия изображения венца (координаты листа).
struct GearLine {
    enum class Role { Pitch, Root, RootCircle, PitchCircle, Helix } role = Role::Pitch;
    std::variant<LineSeg, Circle> curve;
    LineType lineType = LineType::DashDotThin;
    std::string layer;
};

// Линии венца на листе: Side — делительные образующие (±d/2, с выходом overshoot мм листа за
// торцы), образующие впадин (±df/2, по ширине венца); End — делительная окружность и, если
// rootCircleOnEnd, окружность впадин. В зацеплении (mesh) делительные образующие колёс совпадают:
// со стороны сопряжённого колеса образующую выводит только переднее колесо — на общей длине
// обоих венцов. Косой зуб на виде (не в разрезе, helix ≠ 0) — три параллельные сплошные тонкие
// линии направления зуба (ГОСТ 2.402) в средней части венца по одну сторону оси: в
// прямоугольнике s ∈ [0,2b; 0,8b], t ∈ [0,15; 0,85]·da/2, шаг между линиями — 0,2 меньшей
// стороны прямоугольника.
std::vector<GearLine> gearLines(const GearImage& image, const views::SheetView& view,
                                const views::RenderOptions& options = {}, double overshoot = 3.0,
                                const GearImageOptions& imageOptions = {});
Status renderGears(Drawing& drawing, const views::SheetView& view, const std::vector<GearImage>& images,
                   const views::RenderOptions& options = {}, double overshoot = 3.0,
                   const GearImageOptions& imageOptions = {});

// Разрез с нерассечёнными зубьями: из фигур сечения вычитается полоса |t| ≥ df/2 на ширине
// венца (t — расстояние от оси на виде) для каждого венца с section = true (отсечение контуров
// выпуклыми областями, правило чётности сохраняется). У венца с child — только из областей этой
// детали (regionOwner), иначе — из всех.
views::SectionView withUncutTeeth(const views::SectionView& section, const std::vector<GearImage>& images);

// ---------------------------------------------------------------- зацепление (сборка)

// Пара колёс внешнего зацепления на виде: индексы в списке изображений.
struct GearMesh {
    std::size_t front = 0;  // колесо, зуб которого изображается перед зубом сопряжённого
    std::size_t rear = 0;
};

// Зацепления на виде (ГОСТ 2.402): оба венца Side — оси параллельны на виде и лежат в одной
// плоскости, параллельной плоскости вида (|Δdepth| ≤ tol), расстояние между осями
// (d1 + d2)/2 ± tol, ширины венцов перекрываются вдоль оси; оба End — расстояние между центрами
// (d1 + d2)/2 ± tol, венцы перекрываются по глубине. tol = tolerance·m (наименьший модуль пары).
// Переднее колесо — с меньшим числом зубьев (шестерня), при равных — первое. Заполняет
// GearImage::mesh у обоих колёс пары Side.
std::vector<GearMesh> gearMeshes(std::vector<GearImage>& images, double tolerance = 0.01);

// Деталь (дочерняя форма компаунда), которой принадлежит ребро вида; nullopt — неизвестно.
using EdgeChildOf = std::function<std::optional<std::size_t>(const views::ViewEdge&)>;

// Линии вида в зоне зацепления по ГОСТ 2.402 (вид и разрез через оси): рёбра заднего колеса
// (childOf = rear.child) внутри полосы зубьев переднего — на его ширине между образующими
// впадин и вершин со стороны заднего — невидимые (отрезки рассекаются по границе полосы, прочие
// кривые — по середине); рёбра переднего колеса в полосе зубьев заднего — видимые. Так в зоне
// зацепления нет лишних основных линий от пересечения цилиндров вершин условных форм. Вид с
// торца: дуги и окружности вершин обоих колёс — видимые (сплошные основные). Колёса без child
// не обрабатываются.
void applyGearMesh(std::vector<views::ViewEdge>& edges, const std::vector<GearImage>& images,
                   const std::vector<GearMesh>& meshes, const EdgeChildOf& childOf);

// ---------------------------------------------------------------- таблица ГОСТ 2.403

// Длина общей нормали (ГОСТ 16532): W = m·cos α·[π(zn − 0,5) + z′·inv α] + 2x·m·sin α,
// z′ = z·inv αt / inv α (у прямозубого z′ = z), zn = round(z′·αx/π + 0,5),
// cos αx = z′·cos α / (z′ + 2x) (при x = 0 αx = α). Возвращает W, мм; zn — число зубьев в
// длине общей нормали.
double commonNormalLength(const modeling::GearSpec& gear, int& zn);

struct GearTableDef {
    std::size_t gear = 0;              // индекс венца модели (gearsOf)
    std::string accuracy;              // степень точности и вид сопряжения: «8-B ГОСТ 1643-81»
    int matingTeeth = 0;               // число зубьев сопряжённого колеса (0 — строки нет)
    std::string matingDesignation;     // обозначение чертежа сопряжённого колеса
    double centerDistance = 0.0;       // межосевое расстояние aw, мм (0 — строки нет)
    bool commonNormal = true;          // контрольный параметр — длина общей нормали W
};

struct GearTableRow {
    std::string name, symbol, value;
    int part = 1;  // 1 — основные данные, 2 — данные для контроля, 3 — справочные данные
};

// Строки таблицы (ГОСТ 2.403, п. 2–5; состав — docs/STANDARDS_VERIFICATION.md).
Result<std::vector<GearTableRow>> gearTableRows(const modeling::GearSpec& gear, const GearTableDef& def);

struct GearTableLayout {
    Rect box;                        // габарит таблицы на листе
    std::vector<double> rowTops;     // верх каждой строки, мм листа
};

// Таблица в правом верхнем углу рамки (ГОСТ 2.403, п. 1): ширина 110 мм (графы 65 + 10 + 35),
// строки 8 мм, части разделены основными линиями. Обозначение, которого нет в шрифте
// (hasGlyph), не пишется (графа остаётся пустой).
Result<GearTableLayout> addGearTable(Drawing& drawing, const Rect& frame, const std::vector<GearTableRow>& rows,
                                     const TextMetrics* metrics = nullptr);

}  // namespace bendcore::drawing_v2::annotations
