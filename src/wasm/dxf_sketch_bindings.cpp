// DXF import into sketch data (docs/ui/SKETCH_EDITOR.md, "Импорт DXF"): the file is read by
// bendcore_dxf (docs/drawing/DXF_IMPORT.md), scaled to millimetres by $INSUNITS (or by the units
// the user chose for a file without them), filtered by layer, and its curves become sketch
// points and entities. End points closer than the tolerance become one sketch point, so a
// contour drawn as separate LINE/ARC entities is a closed profile of the sketch.
//
//   dxfSketch(bytes, {layers?, units?, defaultUnits?, tolerance?, fixed?, paperSpace?, into?}) -> {ok, value}
//
// Curves: LINE -> line, ARC -> arc (counter-clockwise, as in DXF), CIRCLE -> circle, LWPOLYLINE
// and POLYLINE -> a line per straight segment and an arc per bulge segment (bulgeToArc; a
// clockwise segment becomes the counter-clockwise arc from its end to its start). TEXT/MTEXT are
// not geometry; SPLINE, ELLIPSE, INSERT and the other types the reader skips are reported.

#include <bendcore/dxf_reader.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include "modeling_bindings_internal.hpp"
#include "exchange_common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace bendcore::wasm_modeling {
namespace {

namespace dxf = bendcore::dxf;
constexpr double kPi = 3.14159265358979323846;

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Points of the imported geometry with merging of positions within the tolerance (grid hash:
// a point is compared with the points of its cell and the 8 neighbours).
class PointPool {
public:
    PointPool(m::SketchData& data, double tolerance, bool fixed)
        : data_(data), tol_(tolerance), fixed_(fixed), first_(static_cast<int>(data.points.size())) {}

    int add(dxf::Point2D p) {
        const long long cx = cell(p.x), cy = cell(p.y);
        int best = -1;
        double bestD = tol_;
        for (long long dx = -1; dx <= 1; ++dx) {
            for (long long dy = -1; dy <= 1; ++dy) {
                auto it = grid_.find(key(cx + dx, cy + dy));
                if (it == grid_.end()) continue;
                for (int idx : it->second) {
                    const dxf::Point2D& q = pos_[static_cast<std::size_t>(idx - first_)];
                    const double d = std::hypot(p.x - q.x, p.y - q.y);
                    if (d <= bestD) {
                        bestD = d;
                        best = idx;
                    }
                }
            }
        }
        if (best >= 0) {
            ++merged_;
            return best;
        }
        const int idx = static_cast<int>(data_.points.size());
        data_.points.push_back({decimalText(p.x), decimalText(p.y), fixed_});
        pos_.push_back(p);
        grid_[key(cx, cy)].push_back(idx);
        return idx;
    }

    dxf::Point2D position(int idx) const { return pos_[static_cast<std::size_t>(idx - first_)]; }
    int merged() const noexcept { return merged_; }
    int created() const noexcept { return static_cast<int>(pos_.size()); }
    int first() const noexcept { return first_; }

private:
    long long cell(double v) const { return static_cast<long long>(std::floor(v / tol_)); }
    static std::uint64_t key(long long x, long long y) {
        return (static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull) ^ static_cast<std::uint64_t>(y);
    }

    m::SketchData& data_;
    double tol_;
    bool fixed_;
    int first_;
    int merged_ = 0;
    std::vector<dxf::Point2D> pos_;
    std::unordered_map<std::uint64_t, std::vector<int>> grid_;
};

struct Counts {
    int lines = 0, arcs = 0, circles = 0, polylines = 0, degenerate = 0;
};

dxf::Point2D onCircle(dxf::Point2D c, double r, double deg) {
    const double a = deg * kPi / 180.0;
    return {c.x + r * std::cos(a), c.y + r * std::sin(a)};
}

val numberMap(const std::map<std::string, int>& m) {
    val o = val::object();
    for (const auto& [k, v] : m) o.set(k, v);
    return o;
}

val dxfSketch(val bytes, val options) {
    return guarded([&]() -> val {
        std::string text;
        try {
            text = wasm_exchange::bytesFromJs(bytes, "dxfSketch bytes");
        } catch (const wasm_exchange::OptionError& e) {
            throw InputError(e.what());
        }
        if (present(options) && !isObject(options)) throw InputError("dxfSketch options must be an object");
        const val opt = present(options) ? options : val::object();

        auto parsed = dxf::parseDxf(text);
        if (!parsed) return failure(parsed.error());
        dxf::Document doc = std::move(parsed).value();
        const int fileUnits = doc.insunits;

        // Units: the file's $INSUNITS unless the caller chose others.
        // defaultUnits: only for a file without them.
        int units = fileUnits;
        if (present(opt["units"])) units = requireInt(opt["units"], "units");
        else if (fileUnits == 0 && present(opt["defaultUnits"])) units = requireInt(opt["defaultUnits"], "defaultUnits");
        const auto mmPerUnit = dxf::millimetersPerUnit(units);
        if (!mmPerUnit) {
            return failure("InvalidInput", units == 0
                                               ? std::string("the DXF file has no $INSUNITS; choose the drawing units")
                                               : "unknown DXF units code " + std::to_string(units));
        }
        doc.insunits = units;
        auto scaled = dxf::scaleToMillimeters(doc);
        if (!scaled) return failure(scaled.error());
        const dxf::Document& mm = scaled.value();

        double tolerance = 1e-3;
        if (present(opt["tolerance"])) {
            if (!opt["tolerance"].isNumber()) throw InputError("tolerance must be a number (mm)");
            tolerance = opt["tolerance"].as<double>();
        }
        if (!(std::isfinite(tolerance) && tolerance > 0)) throw InputError("tolerance must be a positive number (mm)");
        const bool fixed = optionalBool(opt, "fixed", false, "fixed");
        const bool paperSpace = optionalBool(opt, "paperSpace", false, "paperSpace");

        // Layers: every layer of the table and of the entities, with its geometry count.
        struct LayerInfo {
            std::string name;
            int color = 7;
            bool frozen = false;
            bool off = false;
            int curves = 0;
        };
        std::map<std::string, LayerInfo> layers;  // by upper-case name
        for (const auto& l : mm.layers) {
            layers[upper(l.name)] = LayerInfo{l.name, std::abs(l.color), l.frozen, l.color < 0, 0};
        }
        auto layerOf = [&](const dxf::EntityProps& p) -> LayerInfo& {
            auto& info = layers[upper(p.layer)];
            if (info.name.empty()) info.name = p.layer;
            return info;
        };
        int ignoredPaper = 0;
        auto countLayer = [&](const dxf::EntityProps& p) {
            if (p.paper_space && !paperSpace) return;
            ++layerOf(p).curves;
        };
        for (const auto& e : mm.lines) countLayer(e.props);
        for (const auto& e : mm.arcs) countLayer(e.props);
        for (const auto& e : mm.circles) countLayer(e.props);
        for (const auto& e : mm.polylines) countLayer(e.props);

        std::set<std::string> chosen;
        if (present(opt["layers"])) {
            for (const val& v : arrayItems(opt["layers"], "layers")) chosen.insert(upper(requireString(v, "layer name")));
        } else {
            for (const auto& [key, info] : layers) {
                if (!info.frozen && !info.off) chosen.insert(key);
            }
        }
        for (const auto& name : chosen) {
            if (!layers.count(name)) throw InputError("the DXF file has no layer '" + name + "'");
        }
        int otherLayers = 0;
        auto take = [&](const dxf::EntityProps& p) {
            if (p.paper_space && !paperSpace) {
                ++ignoredPaper;
                return false;
            }
            if (!chosen.count(upper(p.layer))) {
                ++otherLayers;
                return false;
            }
            return true;
        };

        m::SketchData data = present(opt["into"]) ? sketchFromJs(opt["into"]) : m::SketchData{};
        const int firstEntity = static_cast<int>(data.geometry.size()) + 1;
        PointPool pool(data, tolerance, fixed);
        Counts counts;
        // End-point use of each imported point (lines and arcs): 1 = open end.
        std::map<int, int> endUse;
        double minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool anyBounds = false;
        auto grow = [&](dxf::Point2D p) {
            if (!anyBounds) {
                minX = maxX = p.x;
                minY = maxY = p.y;
                anyBounds = true;
            }
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        };

        auto addLine = [&](dxf::Point2D a, dxf::Point2D b) {
            if (std::hypot(b.x - a.x, b.y - a.y) <= tolerance) {
                ++counts.degenerate;
                return false;
            }
            const int s = pool.add(a), e = pool.add(b);
            if (s == e) {
                ++counts.degenerate;
                return false;
            }
            m::SketchGeometryDef g;
            g.type = m::SketchGeometryType::Line;
            g.start = s;
            g.end = e;
            data.geometry.push_back(g);
            ++endUse[s];
            ++endUse[e];
            // Bounds of the sketch geometry: the merged points, not the file positions.
            grow(pool.position(s));
            grow(pool.position(e));
            return true;
        };
        // Counter-clockwise arc from angle a0 to a1 (degrees).
        auto addArc = [&](dxf::Point2D c, double r, double a0, double a1) {
            double sweep = std::fmod(a1 - a0, 360.0);
            if (sweep < 0) sweep += 360.0;
            const dxf::Point2D ps = onCircle(c, r, a0), pe = onCircle(c, r, a0 + sweep);
            if (sweep * kPi / 180.0 * r <= tolerance || std::hypot(pe.x - ps.x, pe.y - ps.y) <= tolerance) {
                ++counts.degenerate;
                return false;
            }
            const int ci = pool.add(c), s = pool.add(ps), e = pool.add(pe);
            if (s == e || ci == s || ci == e) {
                ++counts.degenerate;
                return false;
            }
            m::SketchGeometryDef g;
            g.type = m::SketchGeometryType::Arc;
            g.center = ci;
            g.start = s;
            g.end = e;
            g.radius = decimalText(r);
            g.counterClockwise = true;
            data.geometry.push_back(g);
            ++endUse[s];
            ++endUse[e];
            // Bounds of the arc: its (merged) ends and the axis extremes it passes.
            grow(pool.position(s));
            grow(pool.position(e));
            for (int q = 0; q < 4; ++q) {
                double rel = std::fmod(q * 90.0 - a0, 360.0);
                if (rel < 0) rel += 360.0;
                if (rel <= sweep) grow(onCircle(c, r, q * 90.0));
            }
            return true;
        };

        for (const auto& e : mm.lines) {
            if (!take(e.props)) continue;
            if (addLine(e.start, e.end)) ++counts.lines;
        }
        for (const auto& e : mm.arcs) {
            if (!take(e.props)) continue;
            if (addArc(e.center, e.radius, e.start_deg, e.end_deg)) ++counts.arcs;
        }
        for (const auto& e : mm.circles) {
            if (!take(e.props)) continue;
            m::SketchGeometryDef g;
            g.type = m::SketchGeometryType::Circle;
            g.center = pool.add(e.center);
            g.radius = decimalText(e.radius);
            data.geometry.push_back(g);
            grow({e.center.x - e.radius, e.center.y - e.radius});
            grow({e.center.x + e.radius, e.center.y + e.radius});
            ++counts.circles;
        }
        int segments = 0;
        for (const auto& p : mm.polylines) {
            if (!take(p.props)) continue;
            const std::size_t n = p.vertices.size();
            const std::size_t segs = p.closed ? n : (n == 0 ? 0 : n - 1);
            bool any = false;
            for (std::size_t i = 0; i < segs; ++i) {
                const dxf::PolylineVertex& v0 = p.vertices[i];
                const dxf::Point2D p1 = p.vertices[(i + 1) % n].point;
                bool added = false;
                if (v0.bulge != 0.0 && std::hypot(p1.x - v0.point.x, p1.y - v0.point.y) > tolerance) {
                    auto arc = dxf::bulgeToArc(v0.point, p1, v0.bulge);
                    if (!arc) return failure(arc.error());
                    const auto& a = arc.value();
                    const double a0 = a.sweep_deg > 0 ? a.start_deg : a.start_deg + a.sweep_deg;
                    added = addArc(a.center, a.radius, a0, a0 + std::abs(a.sweep_deg));
                } else {
                    added = addLine(v0.point, p1);
                }
                if (added) {
                    ++segments;
                    any = true;
                }
            }
            if (any) ++counts.polylines;
        }

        std::vector<int> openEnds;
        for (const auto& [idx, uses] : endUse) {
            if (uses == 1) openEnds.push_back(idx);
        }
        int texts = 0;
        for (const auto& t : mm.texts) {
            if (!(t.props.paper_space && !paperSpace)) ++texts;
        }
        std::vector<int> created;
        for (int id = firstEntity; id <= static_cast<int>(data.geometry.size()); ++id) created.push_back(id);

        val out = val::object();
        out.set("sketch", toJs(data));
        val layerList = val::array();
        for (const auto& [key, info] : layers) {
            val l = val::object();
            l.set("name", info.name);
            l.set("color", info.color);
            l.set("frozen", info.frozen);
            l.set("off", info.off);
            l.set("curves", info.curves);
            l.set("selected", chosen.count(key) > 0);
            layerList.call<void>("push", l);
        }
        out.set("layers", layerList);
        out.set("version", mm.version);
        out.set("fileUnits", fileUnits);
        out.set("units", units);
        out.set("mmPerUnit", *mmPerUnit);
        out.set("tolerance", tolerance);
        val c = val::object();
        c.set("lines", counts.lines);
        c.set("arcs", counts.arcs);
        c.set("circles", counts.circles);
        c.set("polylines", counts.polylines);
        c.set("polylineSegments", segments);
        c.set("degenerate", counts.degenerate);
        out.set("imported", c);
        out.set("entities", typedArray("Int32Array", created));
        out.set("points", pool.created());
        out.set("merged", pool.merged());
        out.set("openEnds", typedArray("Int32Array", openEnds));
        val ignored = val::object();
        ignored.set("texts", texts);
        ignored.set("paperSpace", ignoredPaper);
        ignored.set("otherLayers", otherLayers);
        out.set("ignored", ignored);
        out.set("skipped", numberMap(mm.skipped));
        val warnings = val::array();
        for (const auto& w : mm.warnings) warnings.call<void>("push", w);
        out.set("warnings", warnings);
        if (anyBounds) {
            val b = val::object();
            val lo = val::array(), hi = val::array();
            lo.call<void>("push", minX);
            lo.call<void>("push", minY);
            hi.call<void>("push", maxX);
            hi.call<void>("push", maxY);
            b.set("min", lo);
            b.set("max", hi);
            out.set("bounds", b);
        } else {
            out.set("bounds", val::null());
        }
        return success(out);
    });
}

}  // namespace
}  // namespace bendcore::wasm_modeling

EMSCRIPTEN_BINDINGS(bendcore_dxf_sketch) {
    emscripten::function("dxfSketch", &bendcore::wasm_modeling::dxfSketch);
}
