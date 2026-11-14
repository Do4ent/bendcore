#pragma once

// Размещение видов на листе (ГОСТ 2.302 — масштабы, ГОСТ 2.305 — проекционная связь),
// вывод в модель чертежа с типами линий ГОСТ 2.303, штриховка ГОСТ 2.306, осевые линии и
// центровые метки, обозначения разрезов, видов и выносных элементов.

#include <bendcore/drawing_v2/sections.hpp>

#include <string>

namespace bendcore::drawing_v2::views {

// --- Масштабы ГОСТ 2.302-68, п. 2 ---
struct ScaleRatio {
    int num = 1;  // num:den, например 1:2 → {1, 2}, 2:1 → {2, 1}
    int den = 1;
    double value() const { return double(num) / double(den); }
    std::string label() const;  // "1:2", "2,5:1" (запятая — десятичный знак ЕСКД)
};
// Ряд ГОСТ 2.302 в порядке убывания: 100:1 … 1:1000 (2,5 записан как {5, 2}/{2, 5}).
const std::vector<ScaleRatio>& gostScales();
// Наибольший масштаб ряда, при котором габарит w×h (мм модели) помещается в W×H листа.
Result<ScaleRatio> chooseGostScale(double w, double h, double availW, double availH);

// Вид на листе: точка вида p → anchor + scale·p (мм листа).
struct SheetView {
    ProjectedView view;
    double scale = 1.0;
    Point2 anchor;
    Point2 toSheet(Point2 p) const { return {anchor.x + scale * p.x, anchor.y + scale * p.y}; }
    Box2 sheetBounds() const;
};

// Проекционная связь: точка привязки дочернего вида, поставленного со стороны side
// родительского вида с зазором gap между габаритами (мм листа), с выравниванием по общей
// оси (Above/Below — по x, LeftOf/RightOf — по y). Масштаб дочернего вида — scale.
Point2 alignedAnchor(const SheetView& parent, const ProjectedView& child, double scale,
                     SheetSide side, double gap);

struct LayoutEntry {
    const ProjectedView* view = nullptr;
    int parent = -1;  // индекс родителя в списке (первый элемент — главный вид, parent = -1)
    SheetSide side = SheetSide::Below;
};
struct Layout {
    ScaleRatio scale;
    std::vector<Point2> anchors;
    Box2 bounds;  // габарит компоновки на листе
};
// Подбирает наибольший масштаб ГОСТ 2.302, при котором связанные виды с зазорами gap
// помещаются в области area, и центрирует компоновку в ней.
Result<Layout> layoutViews(const std::vector<LayoutEntry>& entries, const Box2& area,
                           double gap);

// --- Вывод в Drawing ---
struct RenderOptions {
    bool hidden = true;  // невидимые линии — штриховая (ГОСТ 2.303, табл. 1, п. 4)
    bool smooth = true;  // линии перехода — сплошная тонкая (п. 2)
    std::string visibleLayer = "VISIBLE";
    std::string hiddenLayer = "HIDDEN";
    std::string thinLayer = "THIN";
    std::string centerLayer = "CENTER";
    std::string hatchLayer = "HATCH";
    std::string symbolLayer = "SYMBOLS";
    double centerOvershoot = 3.0;  // выход осевых за контур, мм листа (ГОСТ 2.303: 2…5)
    double letterHeight = 7.0;     // буквы обозначений (ГОСТ 2.305: ≈ 2× цифр размеров 3,5)
};

// Создаёт слои вида (если их нет): VISIBLE 7, HIDDEN 1, THIN 3, CENTER 4, HATCH 8, SYMBOLS 2.
Status ensureViewLayers(Drawing& drawing, const RenderOptions& options = {});

// Рёбра вида: видимые резкие и очерковые — SolidThick, линии перехода — SolidThin,
// невидимые — Dashed. Ломаные ребра выводятся Polyline, отрезки — LineSeg, дуги — Arc.
Status renderView(Drawing& drawing, const SheetView& view, const RenderOptions& options = {});

// Шаг штриховки по площади фигуры сечения на листе (ГОСТ 2.306, п. 4: 1…10 мм в
// зависимости от площади): round(√A / 10) до 0,5 мм, в пределах 1…10.
double gostHatchSpacing(double sheetAreaMm2);

// Штриховка одной детали в разрезе: наклон и множитель шага.
struct HatchPattern {
    double angleDeg = 45.0;
    double spacingFactor = 1.0;
};

// Штриховки деталей разреза (по SectionView::regionOwner): у всех фигур одной детали — одна
// штриховка (ГОСТ 2.306-68, п. 4); у смежных деталей (контуры касаются, допуск tol мм модели) —
// разная: наклон вправо и влево (45° и 135°), затем — другой шаг при том же наклоне (×0,6; ×1,6;
// ×0,4; ×2,2). Раскраска — жадная по графу смежности в порядке номеров деталей. Возвращает
// узор для каждой области.
std::vector<HatchPattern> hatchPatterns(const SectionView& section, double tol = 1e-3);

// Штриховка фигуры сечения: металл — 45° к рамке (ГОСТ 2.306, табл. 1, п. 3), сплошная
// тонкая; смежные детали разреза сборки — по hatchPatterns. spacing ≤ 0 — по gostHatchSpacing
// суммарной площади (шаг с множителем ограничивается 1…10 мм). Возвращает базовый шаг.
Result<double> renderSectionHatch(Drawing& drawing, const SheetView& view,
                                  const SectionView& section, double spacing = 0.0,
                                  const RenderOptions& options = {});

// Сплошная волнистая линия вдоль ломаной path (мм листа): синусоида с амплитудой amplitude по
// нормали к ломаной и длиной волны около wavelength (число полуволн округляется, чтобы концы
// лежали на ломаной; замкнутая ломаная — целое число волн). ГОСТ 2.303, табл. 1, п. 3.
Result<Polyline> makeWavyPath(const std::vector<Point2>& path, double amplitude = 1.0, double wavelength = 8.0);

// Граница местного разреза (SectionView::breakLines) — сплошная волнистая тонкая (ГОСТ 2.303,
// табл. 1, п. 3; ГОСТ 2.305, разд. 6), слой THIN.
Status renderBreakLines(Drawing& drawing, const SheetView& view, const SectionView& section,
                        const RenderOptions& options = {}, double amplitude = 1.0,
                        double wavelength = 8.0);

// --- Осевые линии и центровые метки ---
struct CenterMark {
    Point2 center;  // координаты вида
    double radius = 0.0;
    std::uint32_t faceIndex = 0;
};
struct AxisLine {
    Point2 a, b;  // проекция образующей по оси между торцами, координаты вида
    double radius = 0.0;
    std::uint32_t faceIndex = 0;
};
struct CenterGeometry {
    std::vector<CenterMark> marks;
    std::vector<AxisLine> axes;
};
// Цилиндрические грани (ось ∥ направлению взгляда — центровая метка,
// ось ⟂ — осевая линия; наклонные оси пропускаются). Соосные грани одного радиуса
// объединяются; грань учитывается, если суммарный угловой охват ≥ minSweepRad (по
// умолчанию π — скругления с охватом < 180° меток не получают).
Result<CenterGeometry> findCenterGeometry(const kernel::Shape& shape, const ViewFrame& frame,
                                          double minSweepRad = 3.141592653589793);

// Штрихпунктирная тонкая (ГОСТ 2.303, табл. 1, п. 5), выход за контур overshoot (2…5 мм).
// Центровые линии строятся от центра двумя половинами — пересекаются штрихами; вылет
// подбирается в 2…5 мм так, чтобы линия кончалась штрихом. Окружность диаметром на листе
// < 12 мм — сплошные тонкие (ГОСТ 2.303, требования к линиям, пп. 5–7).
Status renderCenterLines(Drawing& drawing, const SheetView& view, const CenterGeometry& g,
                         const RenderOptions& options = {});

// --- Обозначения ---
// Буквы русского алфавита для обозначений без Ё, З, Й, О, Х, Ъ, Ы, Ь (ГОСТ 2.316): index 0 → «А».
std::string designationLetter(int index);

// Надпись над видом (по центру габарита, отступ 5 мм): «А–А», «А (2:1)», «Б».
Status renderViewLabel(Drawing& drawing, const SheetView& view, const std::string& text,
                       const RenderOptions& options = {});

// Обозначение секущей плоскости на виде parent (ГОСТ 2.305, разд. 6): разомкнутые
// штрихи длиной strokeLen (8…20 мм) у концов следа вне габарита, стрелки направления
// взгляда на расстоянии 2…3 мм от внешнего конца штриха, буквы у стрелок.
struct SectionMarkGeometry {
    LineSeg strokes[2];
    LineSeg arrowShafts[2];
    Polyline arrowHeads[2];
    Point2 letterPos[2];
};
Result<SectionMarkGeometry> renderCuttingPlaneMark(Drawing& drawing, const SheetView& parent,
                                                   const CuttingPlane& plane,
                                                   const std::string& letter,
                                                   double strokeLen = 12.0,
                                                   const RenderOptions& options = {});

// Обозначение ступенчатого разреза (ГОСТ 2.305, разд. 6: штрихи разомкнутой линии у концов и в
// местах перегибов следа): planes — параллельные плоскости, все перпендикулярны плоскости вида
// parent; на parent след плоскости i действует между серединами соседних плоскостей вдоль следа.
// У концов — штрихи и стрелки как у renderCuttingPlaneMark (по крайним плоскостям); в каждом
// перегибе — штрих strokeLen вдоль следа до угла с каждой стороны и штрих по перемычке между
// следами (длиной перемычки, если она 8…20 мм; длиннее — по штриху от каждого угла; короче —
// 8 мм по середине перемычки). Возвращает число штрихов.
Result<int> renderSteppedPlaneMark(Drawing& drawing, const SheetView& parent,
                                   const std::vector<CuttingPlane>& planes, const std::string& letter,
                                   double strokeLen = 12.0, const RenderOptions& options = {});

// Обозначение выносного элемента на виде parent: окружность сплошной тонкой линией,
// линия-выноска с полкой и буквой (ГОСТ 2.305, разд. 8).
Status renderDetailMark(Drawing& drawing, const SheetView& parent, Point2 center, double radius,
                        const std::string& letter, const RenderOptions& options = {});

// Граница выносного элемента на его изображении — сплошная тонкая окружность
// (ГОСТ 2.303, табл. 1, п. 2: «линии ограничения выносных элементов»).
Status renderDetailBoundary(Drawing& drawing, const SheetView& detail, Point2 center,
                            double radius, const RenderOptions& options = {});

}  // namespace bendcore::drawing_v2::views
