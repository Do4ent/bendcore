#pragma once

// Общие модели и проверки тестов видов (test_drawing_views_*.cpp).

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <string>
#include <vector>

namespace dvtest {

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::views;

// Брусок L×W×H (X×Y×Z) от начала координат со сквозным вертикальным отверстием диаметром d
// в точке (cx, cy).
struct BlockSpec {
    double L = 60, W = 40, H = 20, d = 10, cx = 30, cy = 20;
};
bendcore::kernel::Shape blockWithHole(const BlockSpec& s = {});

// Отрезки вида с заданной видимостью (только LineSeg).
std::vector<LineSeg> segments(const ProjectedView& v, Visibility vis);
// Число рёбер вида данного типа кривой и видимости.
template <typename T>
int countCurves(const ProjectedView& v, Visibility vis) {
    int n = 0;
    for (const ViewEdge& e : v.edges)
        if (e.visibility == vis && std::holds_alternative<T>(e.curve)) ++n;
    return n;
}

// Есть ли отрезок с концами a, b (в любом порядке) с допуском tol.
bool hasSegment(const std::vector<LineSeg>& segs, Point2 a, Point2 b, double tol = 1e-6);

// Площадь многоугольника (знаковая, против часовой стрелки > 0).
double polygonArea(const std::vector<Point2>& p);

std::string describe(const ProjectedView& v);

}  // namespace dvtest
