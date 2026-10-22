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

// Путь из JS-массива участков в формате importTube: {kind: straight|bend|spiral, ...}.
// Ошибки (неизвестный kind, нет поля) — исключения, ловятся в обёртках ниже.
bendcore::Path pathFromJs(const val& arr) {
    bendcore::Path path;
    const unsigned n = arr["length"].as<unsigned>();
    for (unsigned i = 0; i < n; ++i) {
        const val it = arr[i];
        const std::string kind = it["kind"].as<std::string>();
        if (kind == "straight") {
            path.push_back(bendcore::Straight{it["length"].as<double>()});
        } else if (kind == "bend") {
            path.push_back(bendcore::Bend{it["radius"].as<double>(), it["angle"].as<double>(),
                                          it["rotation"].as<double>()});
        } else if (kind == "spiral") {
            path.push_back(bendcore::Spiral{it["radius"].as<double>(), it["pitch"].as<double>(),
                                            it["turns"].as<double>(), static_cast<int>(it["direction"].as<double>())});
        } else {
            throw std::invalid_argument("неизвестный участок пути: " + kind);
        }
    }
    return path;
}

val okJs(const val& value) {
    val o = val::object();
    o.set("ok", true);
    o.set("value", value);
    return o;
}

val errJs(const std::string& message) {
    val o = val::object();
    o.set("ok", false);
    o.set("error", message);
    return o;
}

val axisLengthJs(const val& path) {
    try {
        return okJs(val(bendcore::axis_length(pathFromJs(path))));
    } catch (const std::exception& e) {
        return errJs(e.what());
    }
}

// Ломаная оси: массив {x, y, z}. steps — число отсчётов на гиб.
val tracePathJs(const val& path, int steps) {
    try {
        val points = val::array();
        for (const auto& p : bendcore::trace(pathFromJs(path), steps)) points.call<void>("push", vec3Js(p));
        return okJs(points);
    } catch (const std::exception& e) {
        return errJs(e.what());
    }
}

// Порт по точке, направлению и ∅ (manual_port). Ошибки — полем ok=false.
val manualPortJs(double x, double y, double z, double dx, double dy, double dz, double od, const std::string& name) {
    try {
        return okJs(portJs(bendcore::manual_port({x, y, z}, {dx, dy, dz}, od, name)));
    } catch (const std::exception& e) {
        return errJs(e.what());
    }
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
    function("axisLength", &axisLengthJs);
    function("manualPort", &manualPortJs);
    function("tracePath", &tracePathJs);
}

// Phase 23-24 modules: BOM, CAM, Scene (minimal WASM bindings)

#include "bendcore/bom.hpp"
#include "bendcore/cam.hpp"
#include "bendcore/scene.hpp"

namespace {

// BOM: convert to JS
val tubeSpecJs(const bendcore::TubeSpec& spec) {
    val o = val::object();
    o.set("id", spec.id);
    o.set("od", spec.od);
    o.set("wall", spec.wall);
    o.set("blankLength", spec.blank_length);
    o.set("quantity", spec.quantity);
    return o;
}

val bomJs(const bendcore::BOM& bom) {
    val groups = val::array();
    for (const auto& g : bom.groups) {
        val group = val::object();
        group.set("spec", tubeSpecJs(g.spec));
        val ids = val::array();
        for (const auto& id : g.tube_ids) ids.call<void>("push", id);
        group.set("tubeIds", ids);
        groups.call<void>("push", group);
    }
    return groups;
}

// CAM: convert to JS
val bendOpJs(const bendcore::BendOperation& op) {
    val o = val::object();
    o.set("sequence", op.sequence);
    o.set("radius", op.radius_mm);
    o.set("angle", op.angle_deg);
    return o;
}

val jobJs(const bendcore::ProductionJob& job) {
    val o = val::object();
    o.set("tubeId", job.tube_id);
    o.set("blankLength", job.blank_length_mm);
    o.set("time", job.estimated_time_sec);
    val bends = val::array();
    for (const auto& b : job.bends) bends.call<void>("push", bendOpJs(b));
    o.set("bends", bends);
    return o;
}

}  // namespace
