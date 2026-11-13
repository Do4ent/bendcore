#pragma once

// Размеры по ГОСТ 2.307-2011 / ISO 129-1 с привязкой к 2D-рёбрам вида (фаза 4, п. 5):
// линейные (горизонтальные, вертикальные, параллельные), радиальные, диаметральные, угловые,
// цепочкой и от общей базы; допуски ISO 286 в размерной надписи; ассоциативность с моделью
// через индексы kernel::TopologyIndex и топологическое именование (modeling::TopoReference).
// Документация — docs/drawing/ANNOTATIONS.md.

#include <bendcore/drawing_v2/annotation_style.hpp>
#include <bendcore/drawing_v2/tolerances.hpp>
#include <bendcore/drawing_v2/view_sheet.hpp>
#include <bendcore/modeling/naming.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::drawing_v2::annotations {

using views::SheetView;

enum class RefKind { Face, Edge, Vertex };

// Ссылка на элемент модели: индекс TopologyIndex текущей формы и устойчивое имя с
// геометрической сигнатурой для перепривязки после перестроения.
struct ModelRef {
    RefKind kind = RefKind::Edge;
    std::uint32_t index = 0;
    modeling::TopoReference persistent;  // name пусто — разрешение только по сигнатуре
};

// Форма модели с картой имён. Индексы карты совпадают с TopologyIndex той же формы.
class ModelBinding {
public:
    // Форма из документа детали (карта имён с историей операций).
    static Result<ModelBinding> fromNamedShape(const modeling::NamedShape& named);
    // Форма без истории: ссылки получают пустое имя и разрешаются после изменения формы
    // только по геометрической сигнатуре.
    static Result<ModelBinding> fromShape(const kernel::Shape& shape);

    const kernel::Shape& shape() const { return *shape_; }
    const kernel::TopologyIndex& topology() const { return *topology_; }
    const modeling::NamedShape& named() const { return *named_; }
    bool hasHistoryNames() const { return historyNames_; }

    // Ссылка на элемент по индексу TopologyIndex. Ошибка InvalidInput — индекс вне формы.
    Result<ModelRef> ref(RefKind kind, std::uint32_t index) const;

private:
    std::shared_ptr<const kernel::Shape> shape_;
    std::shared_ptr<const kernel::TopologyIndex> topology_;
    std::shared_ptr<const modeling::NamedShape> named_;
    bool historyNames_ = false;
};

// Точка привязки размера на виде.
enum class AnchorKind {
    Start,   // начало ребра (TopExp::FirstVertex), совмещённое с концом 2D-участка ребра на виде
    End,     // конец ребра
    Center,  // центр окружности: ребро-окружность/дуга или цилиндрическая грань (ось)
    Point,   // вершина
};

struct Anchor {
    ModelRef ref;
    AnchorKind kind = AnchorKind::Start;
};

enum class DimensionType {
    Horizontal,  // проекция расстояния на ось x листа
    Vertical,    // на ось y листа
    Aligned,     // параллельно отрезку между точками привязки
    Radius,      // «R…»: ребро-окружность/дуга или цилиндрическая грань
    Diameter,    // «⌀…»: окружность на виде или цилиндр сбоку (между очерковыми линиями)
    Angle,       // угол между двумя прямолинейными рёбрами вида
};

struct DimensionDef {
    DimensionType type = DimensionType::Horizontal;
    // Horizontal/Vertical/Aligned — 2 точки; Radius/Diameter — 1 ссылка (вид привязки не
    // используется); Angle — 2 прямолинейных ребра.
    std::vector<Anchor> anchors;
    // Линейные: расстояние размерной линии от крайней точки привязки, мм листа; знак — сторона:
    // Horizontal «+» вверх, Vertical «+» влево, Aligned «+» влево от направления a→b.
    // Diameter цилиндра сбоку — вылет размерной линии за концы очерка: «+» — за верхний конец
    // (для горизонтального очерка — за правый), «−» — за нижний. Angle — радиус дуги.
    double offset = 10.0;
    // Точки, от крайней из которых отсчитывается offset (размеры цепочкой и от базы задают
    // общий отсчёт для всей группы); пусто — собственные точки привязки.
    std::vector<Anchor> offsetFrom;
    // Radius/Diameter на окружности: направление размерной линии, градусы от оси x листа.
    double leaderAngleDeg = 45.0;
    // Сдвиг размерного числа вдоль размерной линии от середины, мм листа.
    double textShift = 0.0;
    // Ручная надпись: «<>» заменяется вычисленным значением; без «<>» — заменяет его целиком.
    std::optional<std::string> textOverride;
    std::optional<ToleranceClass> tolerance;
    ToleranceDisplay toleranceDisplay = ToleranceDisplay::ClassOnly;
    std::string prefix;  // перед числом, например «2 отв. »
    std::string suffix;
    int decimals = 2;    // округление линейного значения
};

enum class DimensionState {
    Ok,
    Dangling,  // «висячий»: ссылка не разрешилась или элемент не изображён на виде
};

struct DimensionLabel {
    Point2 position;
    double rotationDeg = 0.0;
    std::string text;
    double height = 3.5;
    HAlign hAlign = HAlign::Left;
};

struct DimensionGeometry {
    std::vector<LineSeg> extensionLines;
    std::vector<LineSeg> dimensionLines;  // в т. ч. полки
    std::vector<Arc> dimensionArcs;
    std::vector<SolidFill> arrows;
    std::vector<DimensionLabel> labels;
};

struct DimensionResult {
    DimensionState state = DimensionState::Dangling;
    std::string reason;   // для Dangling: почему; для Ok — примечание о перепривязке (может быть пусто)
    double value = 0.0;   // мм модели (линейные, радиус, диаметр) или градусы (Angle)
    std::string text;     // основная надпись: «60», «⌀10H7», «R5», «30°»
    bool overridden = false;
    std::optional<LimitDeviations> deviations;
    DimensionGeometry geometry;  // мм листа
};

// Значение и геометрия размера на виде. Значение берётся из 2D-рёбер вида, привязанных к
// элементам модели (ViewEdge::edgeIndex / faceIndex); элемент, не изображённый на виде,
// делает размер висячим. Ошибки (Result) — только неверное определение размера (число
// точек, тип ссылки).
Result<DimensionResult> evaluateDimension(const DimensionDef& def, const ModelBinding& model,
                                          const SheetView& view, const AnnotationStyle& style = {});

// Вывод размера в чертёж: выносные и размерные линии — сплошная тонкая (ГОСТ 2.303, табл. 1,
// п. 2), стрелки — заливка, текст — шрифт ГОСТ 2.304. Висячий размер — ошибка InvalidInput.
Status renderDimension(Drawing& drawing, const DimensionResult& result, const AnnotationStyle& style = {});

// Геометрия размера с готовой надписью text (обозначение резьбы и т. п., ГОСТ 2.311) в
// координатах листа, оформление — как у evaluateDimension:
//   линейный: измерение между a и b вдоль a→b, размерная линия ∥ a→b на расстоянии offset за
//   крайней из точек в направлении side (единичный вектор ⟂ a→b), выносные линии от a и b;
//   по окружности: размерная линия через центр под angleDeg, стрелки на окружности radius.
DimensionGeometry labelledLinearGeometry(Point2 a, Point2 b, Point2 side, double offset, const std::string& text,
                                         double textShift, const AnnotationStyle& style = {});
DimensionGeometry labelledDiameterGeometry(Point2 center, double radius, double angleDeg, const std::string& text,
                                           const AnnotationStyle& style = {});

// --- Ассоциативность ---

struct RebindResult {
    bool ok = false;
    modeling::ResolveMethod method = modeling::ResolveMethod::Broken;
    std::string detail;
};

// Переводит ссылку на новую форму (после перестроения модели): имя → история → сигнатура
// (modeling::resolveReference). Ссылка, разрешившаяся в несколько элементов (расщепление),
// считается сломанной. При успехе обновляются индекс и сохранённое имя/сигнатура.
RebindResult rebindRef(ModelRef& ref, const ModelBinding& model);

struct DimensionUpdate {
    DimensionState state = DimensionState::Dangling;
    std::optional<double> oldValue;
    double newValue = 0.0;
    std::string reason;
};

// Набор размеров одного вида с пересчётом после изменения модели.
class DimensionSet {
public:
    std::size_t add(DimensionDef def);
    const std::vector<DimensionDef>& definitions() const { return defs_; }
    const std::vector<DimensionResult>& results() const { return results_; }

    // Вычисляет все размеры на виде.
    Status evaluate(const ModelBinding& model, const SheetView& view, const AnnotationStyle& style = {});
    // После перестроения: все ссылки переводятся на новую форму, размеры пересчитываются;
    // неразрешённые помечаются висячими с причиной. Возвращает отчёт по каждому размеру.
    Result<std::vector<DimensionUpdate>> update(const ModelBinding& model, const SheetView& view,
                                                const AnnotationStyle& style = {});
    // Выводит неповисшие размеры; число выведенных.
    Result<std::size_t> render(Drawing& drawing, const AnnotationStyle& style = {}) const;

private:
    std::vector<DimensionDef> defs_;
    std::vector<DimensionResult> results_;
};

// Размеры цепочкой (ГОСТ 2.307): точки сортируются вдоль оси (Horizontal — x листа,
// Vertical — y), соседние пары дают размеры на одной размерной линии.
Result<std::vector<DimensionDef>> chainDimensions(DimensionType type, const std::vector<Anchor>& points,
                                                  const ModelBinding& model, const SheetView& view,
                                                  double offset);

// Размеры от общей базы: от base до каждой точки; меньшие размеры ближе к контуру, шаг между
// размерными линиями spacing (≥ 7 мм).
Result<std::vector<DimensionDef>> baselineDimensions(DimensionType type, const Anchor& base,
                                                     const std::vector<Anchor>& points,
                                                     const ModelBinding& model, const SheetView& view,
                                                     double firstOffset, double spacing = 7.0);

// Точка привязки на виде (мм модели в плоскости вида), для построения цепочек и тестов.
Result<Point2> anchorPoint(const Anchor& anchor, const ModelBinding& model, const views::ProjectedView& view);

}  // namespace bendcore::drawing_v2::annotations
