// WebAssembly-мост (embind) для TypeScript-интерфейса: 2D-эскиз (7.9) и простая функция ядра.
// Результаты solve() и point() отдаются как обычные JS-объекты (без delete на стороне JS).
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "bendcore/sketch2d.hpp"
#include "bendcore/tube_model.hpp"
#include "bendcore/tubegeom.hpp"

#include <stdexcept>
#include <string>

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

// Импорт трубы (CSV или JSON) в модель. Ошибки возвращаются полем ok=false и error (исключения не выходят в JS).
val vec3Js(const bendcore::Vec3& v) {
    val o = val::object();
    o.set("x", v[0]);
    o.set("y", v[1]);
    o.set("z", v[2]);
    return o;
}

val pathItemJs(const bendcore::Item& item) {
    val o = val::object();
    if (const auto* s = std::get_if<bendcore::Straight>(&item)) {
        o.set("kind", std::string("straight"));
        o.set("length", s->S);
    } else if (const auto* b = std::get_if<bendcore::Bend>(&item)) {
        o.set("kind", std::string("bend"));
        o.set("radius", b->R);
        o.set("angle", b->B);
        o.set("rotation", b->A);
    } else if (const auto* sp = std::get_if<bendcore::Spiral>(&item)) {
        o.set("kind", std::string("spiral"));
        o.set("radius", sp->R);
        o.set("pitch", sp->p);
        o.set("turns", sp->n);
        o.set("direction", sp->direction);
    }
    return o;
}

val portJs(const bendcore::Port& p) {
    val o = val::object();
    o.set("name", p.name);
    o.set("pos", vec3Js(p.pos));
    o.set("direction", vec3Js(p.direction));
    o.set("od", p.od);
    return o;
}

val importTubeJs(const std::string& format, const std::string& text, const std::string& id, double od, double wall) {
    val out = val::object();
    try {
        bendcore::ImportData data;
        if (format == "csv") {
            data = bendcore::import_csv(text);
        } else if (format == "json") {
            data = bendcore::import_json(text);
        } else {
            throw std::invalid_argument("формат должен быть csv или json");
        }
        const bendcore::TubeModel m = bendcore::make_model(id, format, data, od, wall);
        out.set("ok", true);
        out.set("id", m.id);
        out.set("source", m.source);
        out.set("od", m.od);
        out.set("wall", m.wall);
        if (m.path) {
            val path = val::array();
            for (const auto& item : *m.path) path.call<void>("push", pathItemJs(item));
            out.set("path", path);
        } else {
            out.set("path", val::null());
        }
        val ports = val::array();
        for (const auto& p : m.ports) ports.call<void>("push", portJs(p));
        out.set("ports", ports);
    } catch (const std::exception& e) {
        out = val::object();
        out.set("ok", false);
        out.set("error", std::string(e.what()));
    }
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
    function("wrap180", &bendcore::wrap180);
    function("importTube", &importTubeJs);
}
