// WebAssembly-мост (embind) для TypeScript-интерфейса: 2D-эскиз (7.9) и простая функция ядра.
// Результаты solve() и point() отдаются как обычные JS-объекты (без delete на стороне JS).
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "bendcore/sketch2d.hpp"
#include "bendcore/tubegeom.hpp"

using namespace emscripten;
using bendcore::sketch2d::Id;
using bendcore::sketch2d::Point2;
using bendcore::sketch2d::Report;
using bendcore::sketch2d::Sketch;

namespace {

val reportToJs(const Report& r) {
    val out = val::object();
    out.set("status", r.status);
    out.set("dof", r.dof);
    val conflicting = val::array();
    for (Id id : r.conflicting) conflicting.call<void>("push", id);
    out.set("conflicting", conflicting);
    val redundant = val::array();
    for (Id id : r.redundant) redundant.call<void>("push", id);
    out.set("redundant", redundant);
    return out;
}

val solveJs(Sketch& s) {
    return reportToJs(s.solve());
}

val pointJs(const Sketch& s, Id p) {
    const Point2 q = s.point(p);
    val out = val::object();
    out.set("x", q.x);
    out.set("y", q.y);
    return out;
}

}  // namespace

EMSCRIPTEN_BINDINGS(bendcore_module) {
    class_<Sketch>("Sketch")
        .constructor<>()
        .function("addPoint", &Sketch::add_point)
        .function("fixPoint", &Sketch::fix_point)
        .function("addLine", &Sketch::add_line)
        .function("addCircle", &Sketch::add_circle)
        .function("constrainCoincident", &Sketch::constrain_coincident)
        .function("constrainHorizontal", &Sketch::constrain_horizontal)
        .function("constrainVertical", &Sketch::constrain_vertical)
        .function("constrainDistance", &Sketch::constrain_distance)
        .function("constrainRadius", &Sketch::constrain_radius)
        .function("constrainParallel", &Sketch::constrain_parallel)
        .function("constrainPerpendicular", &Sketch::constrain_perpendicular)
        .function("constrainEqualLength", &Sketch::constrain_equal_length)
        .function("constrainAngle", &Sketch::constrain_angle)
        .function("constrainPointOnLine", &Sketch::constrain_point_on_line)
        .function("constrainPointOnCircle", &Sketch::constrain_point_on_circle)
        .function("constrainEqualRadius", &Sketch::constrain_equal_radius)
        .function("constrainSymmetric", &Sketch::constrain_symmetric)
        .function("removeConstraint", &Sketch::remove_constraint)
        .function("solve", &solveJs)
        .function("point", &pointJs)
        .function("circleRadius", &Sketch::circle_radius);

    function("tangentLength", &bendcore::tangent_length);
}
