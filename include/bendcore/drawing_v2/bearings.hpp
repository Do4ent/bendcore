#pragma once

// Условное упрощённое изображение подшипников качения на сборочных чертежах по ГОСТ 2.420-69
// (ISO 8826-1 — общее упрощённое изображение): в разрезе плоскостью через ось подшипник не
// рассекается как сплошная деталь и не штрихуется; по обе стороны оси — контур сечения
// подшипника d×D×B (сплошная основная) с диагоналями (сплошная тонкая) или с символом тела
// качения. Документация — docs/drawing/VIEWS.md, «Подшипники качения».
//
// Распознавание подшипника (у модели нет атрибутов подшипника — ни у граней, ни у тел):
//   • по геометрии — тело вращения кольцевого сечения (соосные внутренний и наружный цилиндры,
//     все вершины в кольце и между торцами) из трёх и более тел (кольца и тела качения) со
//     сферическими или торовыми гранями (шарики, дорожки) — так выглядят каталожные подшипники;
//   • по имени — простое кольцо d×D×B, у которого наименование или имя определения начинается
//     со слова «Подшипник» или «Bearing» (кольцо без тел качения иначе не отличить от втулки).
// Размеры d, D, B и ось — всегда по геометрии (bearingGeometry).

#include <bendcore/drawing_v2/gears.hpp>
#include <bendcore/drawing_v2/sections.hpp>
#include <bendcore/drawing_v2/view_sheet.hpp>

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bendcore::drawing_v2::annotations {

// Подшипник в сборке (мировая СК).
struct BearingInfo {
    std::size_t child = 0;          // дочерняя форма компаунда сборки (ModelSnapshot::childOwner)
    views::Vec3 origin;             // центр торца s = 0
    views::Vec3 axis{0, 0, 1};      // единичное направление оси (от торца s = 0 к s = B)
    double d = 0.0, D = 0.0, B = 0.0;  // внутренний, наружный диаметры, ширина, мм
    // Тела качения: не меньше трёх тел (кольца и тела качения) и сферические или торовые грани
    // (шарики, дорожки качения). Одно тело с торовыми скруглениями — не подшипник.
    bool rollingElements = false;
};

// Геометрия кольца по форме детали: ось — у цилиндрической грани наибольшей площади; d и D —
// наименьший и наибольший диаметры соосных ей цилиндров; торцы — крайние положения вершин и
// плоских граней, перпендикулярных оси. Ошибки InvalidInput: нет цилиндрических граней, один
// диаметр (не кольцо), вершина вне кольца d/2…D/2 (с допуском tolerance) — не тело вращения
// кольцевого сечения.
Result<BearingInfo> bearingGeometry(const TopoDS_Shape& part, double tolerance = 1e-6);

// Наименование начинается со слова «Подшипник» / «Bearing» (регистр не важен, «Подшипники» — нет).
bool bearingName(const std::string& name);

// Подшипник ли деталь: по геометрии с телами качения или по имени (names — наименование и имя
// определения). nullopt — не подшипник.
std::optional<BearingInfo> recognizeBearing(const TopoDS_Shape& part, const std::vector<std::string>& names);

enum class BearingSymbol {
    Diagonals,       // упрощённое изображение: контур и диагонали (ГОСТ 2.420)
    RollingElement,  // контур и символ тела качения — окружность в центре сечения кольца
};

// Подшипник на виде разреза: ось лежит в секущей плоскости.
struct BearingImage {
    std::size_t bearing = 0;  // индекс в списке bearingImages
    BearingInfo info;
    Point2 origin;            // центр торца s = 0 на виде (мм модели)
    Point2 axis{1.0, 0.0};    // единичное направление оси на виде
};

// Подшипники, ось которых параллельна плоскости вида (|ось·toward| ≤ angularTolerance) и лежит в
// секущей плоскости (расстояние ≤ 1e-6 мм, ось ⟂ направлению взгляда).
std::vector<BearingImage> bearingImages(const std::vector<BearingInfo>& bearings, const views::ViewFrame& frame,
                                        const views::CuttingPlane& plane, double angularTolerance = 1e-6);

struct BearingLine {
    enum class Role { Contour, Diagonal, RollingElement } role = Role::Contour;
    std::variant<LineSeg, Circle> curve;  // координаты листа
    LineType lineType = LineType::SolidThick;
    std::string layer;
};

// Линии подшипника на листе: по обе стороны оси — прямоугольник s ∈ [0, B], t ∈ [d/2, D/2]
// (4 отрезка, сплошная основная, слой видимых линий) и диагонали прямоугольника (2 отрезка,
// сплошная тонкая) или окружность тела качения ⌀0,3(D − d) (не больше 0,9B) в центре
// прямоугольника (сплошная основная).
std::vector<BearingLine> bearingLines(const BearingImage& image, const views::SheetView& view,
                                      const views::RenderOptions& options = {},
                                      BearingSymbol symbol = BearingSymbol::Diagonals);
Status renderBearings(Drawing& drawing, const views::SheetView& view, const std::vector<BearingImage>& images,
                      const views::RenderOptions& options = {}, BearingSymbol symbol = BearingSymbol::Diagonals);

// Номер детали области фигуры сечения, на контуре которой лежит ребро (середина ребра ближе tol
// к звену контура); nullopt — ни одной.
std::optional<int> sectionEdgeOwner(const views::SectionView& section, const views::ViewEdge& edge, double tol);

// Точка на ребре вида (середина отрезка, дуги, звена ломаной; точка окружности под 0°).
Point2 edgeMidpoint(const views::Curve2& curve);

// Разрез с условным изображением подшипников: области фигуры сечения деталей-подшипников
// (regionOwner = child) и рёбра, принадлежащие им (childOf), удаляются — подшипник не
// штрихуется, вместо его линий выводятся bearingLines.
views::SectionView withConventionalBearings(const views::SectionView& section, const std::vector<BearingImage>& images,
                                            const EdgeChildOf& childOf);

// Точка внутри области (контуры по правилу чётности): середина самого длинного внутреннего
// участка горизонтальных прямых через 1/8 … 7/8 высоты габарита; nullopt — область пустая.
std::optional<Point2> regionInteriorPoint(const std::vector<std::vector<Point2>>& region);

}  // namespace bendcore::drawing_v2::annotations
