#pragma once

// Зубчатый венец цилиндрического колеса внешнего зацепления: эвольвентный профиль,
// нарезанный исходным контуром ГОСТ 13755-2015 (ISO 53:1998, профиль A): α = 20°, ha* = 1,
// c* = 0,25, ρf* = 0,38; прямой и косой зуб, смещение x. Геометрия — ГОСТ 16532-70.
// Профиль строится в торцовом сечении: эвольвента — аналитически (B-сплайн по точкам
// эвольвенты с контролем отклонения), переходная кривая — огибающая скругления головки
// исходной рейки при обкатке (B-сплайн по точкам огибающей), подрезание — пересечение
// огибающей с эвольвентой. Подробности: docs/modeling/FEATURES.md, раздел «Зубчатый венец».

#include <bendcore/modeling/features.hpp>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax3.hxx>

#include <string>
#include <utility>
#include <vector>

namespace bendcore::modeling {

// Исходный контур (зубчатой рейки) в нормальном сечении, коэффициенты в долях модуля.
struct BasicRack {
    std::string standard = "ГОСТ 13755-2015";
    double alpha = 0.3490658503988659;  // α, рад (20°)
    double haStar = 1.0;                // ha* — высота головки
    double cStar = 0.25;                // c* — радиальный зазор
    double rhoStar = 0.38;              // ρf* — радиус кривизны переходной кривой (скругление головки рейки)
};

enum class GearHand { Right, Left };  // направление линии зуба косозубого колеса

// Зубчатый венец. Ось колеса — axis.Direction(), торец начала венца — плоскость axis через
// axis.Location(); зуб № 1 направлен по axis.XDirection(). Венец занимает [0, width] вдоль оси.
struct GearSpec {
    double module = 2.0;   // m — нормальный модуль, мм
    int teeth = 20;        // z
    double shift = 0.0;    // x — коэффициент смещения исходного контура
    double width = 20.0;   // b — ширина венца, мм
    double helix = 0.0;    // β — угол наклона линии зуба на делительном цилиндре, рад, [0, 45°]
    GearHand hand = GearHand::Right;
    BasicRack rack;
    gp_Ax3 axis;
    // Степень точности и вид сопряжения (ГОСТ 1643) — только для таблицы параметров чертежа.
    std::string accuracy;
};

// Размеры венца по ГОСТ 16532-70 (без коэффициента уравнительного смещения: Δy = 0).
struct GearGeometry {
    double mt = 0.0;          // торцовый модуль m / cos β
    double alphaT = 0.0;      // торцовый угол профиля, tg αt = tg α / cos β
    double d = 0.0;           // делительный диаметр z·mt
    double db = 0.0;          // основной диаметр d·cos αt
    double da = 0.0;          // диаметр вершин d + 2(ha* + x)m
    double df = 0.0;          // диаметр впадин d − 2(ha* + c* − x)m
    double pitch = 0.0;       // нормальный шаг π·m
    double s = 0.0;           // нормальная толщина зуба на делительном цилиндре m(π/2 + 2x tg α)
    double st = 0.0;          // торцовая толщина s / cos β
    double sa = 0.0;          // торцовая толщина по окружности вершин (дуга)
    double dForm = 0.0;       // диаметр начала эвольвенты (граничная точка профиля)
    double zMin = 0.0;        // наименьшее z без подрезания при данном x: 2(ha* − x)cos β / sin²αt
    double xMin = 0.0;        // наименьшее x без подрезания при данном z
    bool undercut = false;    // z < zMin (нарезание подрезает ножку)
    double lead = 0.0;        // ход винтовой линии π·d / tg β (0 — прямой зуб)
    double twist = 0.0;       // поворот торцового профиля на ширине b, рад (со знаком направления)
};

// Проверка параметров (m > 0, 3 ≤ z ≤ 1000, |x| ≤ 2, b > 0, 0 ≤ β ≤ 45°, исходный контур) и
// размеры. Ошибки InvalidInput: неверные параметры; заострение зуба (sa ≤ 0).
Result<GearGeometry> gearGeometry(const GearSpec& spec);

// Точки торцового профиля одного зуба, полученные из формул (не из B-сплайнов): правая сторона
// (по часовой стрелке от оси зуба) от окружности впадин до окружности вершин, в локальной СК
// венца (x — ось зуба № 1, y = axis × x), мм. root — дуга впадины от середины впадины, fillet —
// переходная кривая (огибающая; при подрезании — до пересечения с эвольвентой), involute —
// эвольвента от граничной точки до вершины, tipHalf — половина дуги вершины.
struct ToothSide {
    std::vector<std::pair<double, double>> root, fillet, involute, tipHalf;
};
Result<ToothSide> toothSidePoints(const GearSpec& spec, int samples = 200);

// Торцовый профиль венца (в плоскости axis через axis.Location()): замкнутый контур из 6
// рёбер на зуб — дуга впадины, переходная кривая, эвольвента, дуга вершины, эвольвента,
// переходная кривая. roles — роль каждого ребра «T<k>.Root», «T<k>.FilletR», «T<k>.FlankR»,
// «T<k>.Tip», «T<k>.FlankL», «T<k>.FilletL» (k = 1…z; R — сторона по часовой стрелке при взгляде
// с конца оси). tolerance — допустимое отклонение B-сплайна от формулы, мм.
struct GearProfile {
    TopoDS_Wire wire;
    TopoDS_Face face;
    std::vector<std::pair<TopoDS_Edge, std::string>> roles;
    double maxDeviation = 0.0;  // наибольшее отклонение B-сплайнов от формул в контрольных точках
    std::vector<std::string> warnings;
};
Result<GearProfile> gearProfile(const GearSpec& spec, double tolerance = 1e-6);

// Операция «Зубчатый венец».
//   Cut     — нарезание зубьев на заготовке target: удаляется кольцо между окружностью,
//             охватывающей заготовку, и профилем венца на ширине b (впадины зубьев);
//   Boss    — тело венца (сплошной диск с зубьями) объединяется с target (или становится телом);
//   NewBody — тело венца отдельным телом.
// Прямой зуб — BRepPrimAPI_MakePrism; косой — BRepOffsetAPI_MakePipeShell вдоль оси со
// вспомогательной винтовой линией (поворот профиля на twist). FeatureResult::roles — грани
// инструмента с ролями рёбер профиля («T<k>.FlankR» …) и торцы «Start», «End» (Boss/NewBody).
// Предупреждения: подрезание, малая толщина зуба на вершине (sa < 0,25m).
struct GearParams {
    GearSpec spec;
    FeatureMode mode = FeatureMode::Cut;
    double tolerance = 1e-6;  // мм, отклонение B-сплайнов профиля
};
Result<FeatureResult> gearTeeth(const TopoDS_Shape& target, const GearParams& params);

// Атрибуты «gear.*» граней венца ↔ GearSpec (docs/modeling/FEATURES.md, «Зубчатый венец»).
std::vector<std::pair<std::string, std::string>> gearAttributes(const GearSpec& spec);
Result<GearSpec> gearFromAttributes(const std::vector<std::pair<std::string, std::string>>& attributes);

}  // namespace bendcore::modeling
