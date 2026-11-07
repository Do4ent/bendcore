#pragma once

// Лечение геометрии на OCCT: ориентация граней, ShapeFix, удаление малых элементов,
// перевод B-сплайнов в аналитику, объединение граней одной поверхности, проверка.
// Подробности: docs/kernel/GEOMETRY_HEALING.md.

#include <bendcore/kernel/result.hpp>

#include <TopoDS_Shape.hxx>

#include <map>
#include <string>
#include <vector>

namespace bendcore::kernel {

struct HealingOptions {
    // Шаги выполняются в этом порядке; любой можно отключить.
    bool directFaces = true;          // ShapeCustom::DirectFaces
    bool fixShape = true;             // ShapeFix_Shape
    bool removeSmallElements = true;  // ShapeFix_FixSmallFace + ShapeFix_Wireframe
    bool convertToAnalytic = true;    // GeomConvert_SurfToAnaSurf / GeomConvert_CurveToAnaCurve
    bool unifySameDomain = true;      // ShapeUpgrade_UnifySameDomain

    // Рабочая точность ShapeFix и порог «малого» ребра/грани, мм.
    double precision = 1e-6;
    // Предел, до которого ShapeFix может поднимать допуски элементов, мм.
    double maxTolerance = 1e-3;
    // Ребро короче / грань уже этого значения считается малым элементом, мм.
    double smallElementSize = 1e-5;
    // Максимальное отклонение аналитической поверхности/кривой от исходной, мм.
    double analyticTolerance = 1e-6;
    // Допуски UnifySameDomain: линейный (мм) и угловой (рад).
    double unifyLinearTolerance = 1e-6;
    double unifyAngularTolerance = 1e-8;
};

// Счётчики по типам: ключ — имя типа OCCT (Plane, Cylinder, BSplineSurface, Line, Circle...).
using TypeCounts = std::map<std::string, int>;

struct HealingReport {
    TypeCounts surfacesBefore;
    TypeCounts surfacesAfter;
    TypeCounts curvesBefore;  // по 3D-кривым невырожденных рёбер
    TypeCounts curvesAfter;
    int facesBefore = 0;
    int facesAfter = 0;
    int edgesBefore = 0;
    int edgesAfter = 0;

    int directedFaces = 0;           // грани, у которых DirectFaces сменил поверхность
    int smallFacesRemoved = 0;       // убыль граней на шаге малых элементов
    int smallEdgesRemoved = 0;       // убыль рёбер на шаге малых элементов
    int surfacesConverted = 0;       // B-сплайн/Безье поверхности, ставшие аналитическими
    int curvesConverted = 0;         // B-сплайн/Безье кривые, ставшие аналитическими
    double maxConversionDeviation = 0.0;  // наибольшее отклонение при переводе, мм
    int facesMerged = 0;             // убыль граней на шаге UnifySameDomain

    bool validBefore = false;        // BRepCheck_Analyzer на входе
    bool valid = false;              // BRepCheck_Analyzer на выходе
    double volumeBefore = 0.0;       // BRepGProp::VolumeProperties, мм³ (0 без тел)
    double volumeAfter = 0.0;
    double volumeChange = 0.0;       // volumeAfter - volumeBefore
    double maxToleranceAfter = 0.0;  // наибольший допуск элемента результата

    std::vector<std::string> steps;     // выполненные шаги в порядке выполнения
    std::vector<std::string> warnings;  // шаг не дал результата / откат шага
};

struct HealingResult {
    TopoDS_Shape shape;
    HealingReport report;
};

// Возвращает InvalidInput для пустой формы, OperationFailed при исключении OCCT.
// Невалидный итог не считается ошибкой: смотрите report.valid.
Result<HealingResult> healShape(const TopoDS_Shape& shape, const HealingOptions& options = {});

// Только шаг перевода B-сплайновых/Безье поверхностей и кривых в аналитические.
// В report заполняются счётчики типов, surfacesConverted, curvesConverted, valid.
Result<HealingResult> convertToAnalytic(const TopoDS_Shape& shape, double tolerance = 1e-6);

// Подсчёт граней по типам поверхностей (BRepAdaptor_Surface::GetType).
TypeCounts countSurfaceTypes(const TopoDS_Shape& shape);
// Подсчёт невырожденных рёбер по типам 3D-кривых (BRepAdaptor_Curve::GetType).
TypeCounts countCurveTypes(const TopoDS_Shape& shape);

}  // namespace bendcore::kernel
