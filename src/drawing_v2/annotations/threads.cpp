// Изображение резьбы по ГОСТ 2.311-68 по условной резьбе модели (docs/drawing/ANNOTATIONS.md,
// раздел «Резьба»).

#include <bendcore/drawing_v2/threads.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <set>

namespace bendcore::drawing_v2::annotations {

using views::Vec3;
using views::ViewEdge;
using views::Visibility;

namespace {

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

Vec3 toVec(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }
Vec3 toVec(const gp_Dir& d) { return {d.X(), d.Y(), d.Z()}; }
Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 sub(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }
double norm(Point2 a) { return std::hypot(a.x, a.y); }

// Одна и та же резьба (грань, расщеплённая на части, несёт одинаковые атрибуты).
bool sameThread(const modeling::ThreadSpec& a, const modeling::ThreadSpec& b) {
    const double tol = 1e-6 * std::max(1.0, a.nominal);
    return std::abs(a.nominal - b.nominal) <= tol && std::abs(a.pitch - b.pitch) <= tol &&
           std::abs(a.length - b.length) <= tol && a.entry.Distance(b.entry) <= tol &&
           a.direction.IsEqual(b.direction, 1e-9) && a.hand == b.hand;
}

struct Group {
    modeling::ThreadSpec thread;
    std::string name;
    std::vector<std::uint32_t> faces;
};

// Прямой участок ребра вида: отрезок или ломаная на прямой (концы).
std::optional<LineSeg> straight(const views::Curve2& c) {
    if (const auto* l = std::get_if<LineSeg>(&c)) return *l;
    if (const auto* p = std::get_if<Polyline>(&c); p && p->points.size() >= 2 && !p->closed) {
        const Point2 a = p->points.front(), b = p->points.back();
        const double len = norm(sub(b, a));
        if (len < 1e-12) return std::nullopt;
        for (const Point2& q : p->points) {
            const Point2 d = sub(q, a);
            if (std::abs(d.x * (b.y - a.y) - d.y * (b.x - a.x)) / len > 1e-6 * std::max(1.0, len)) return std::nullopt;
        }
        return LineSeg{a, b};
    }
    return std::nullopt;
}

}  // namespace

std::string threadDesignationText(const modeling::ThreadSpec& spec, const std::string& toleranceField) {
    std::string s = "M" + formatNumber(spec.nominal, 3);
    // Крупный шаг в обозначении не пишется (ГОСТ 8724, ISO 261): шаг сравнивается с таблицей.
    auto coarse = modeling::metricCoarse(spec.nominal);
    const bool fine = !coarse || std::abs(coarse.value().pitch - spec.pitch) > 1e-9;
    if (fine) s += "×" + formatNumber(spec.pitch, 3);
    if (spec.hand == modeling::ThreadHand::Left) s += "LH";
    if (!toleranceField.empty()) s += "-" + toleranceField;
    return s;
}

Result<std::vector<ThreadImage>> threadImages(const std::vector<modeling::ThreadInfo>& threads,
                                              const kernel::TopologyIndex& topo, const views::ProjectedView& view,
                                              const ThreadImageOptions& opt) {
    using R = Result<std::vector<ThreadImage>>;
    std::vector<Group> groups;
    for (const modeling::ThreadInfo& t : threads) {
        if (t.face >= topo.faces().size()) {
            return invalid("thread image: face index " + std::to_string(t.face) + " is outside the shape");
        }
        if (!t.thread.internal) {
            return R::failure(ErrorCode::Unsupported, "thread image: external thread on '" + t.name +
                                                          "' (only internal threads are modelled)");
        }
        if (!(t.thread.nominal > 0) || !(t.thread.length > 0)) {
            return invalid("thread image: thread on '" + t.name + "' has no diameter or length");
        }
        auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) { return sameThread(x.thread, t.thread); });
        if (g == groups.end()) {
            groups.push_back({t.thread, t.name, {static_cast<std::uint32_t>(t.face)}});
        } else {
            g->faces.push_back(static_cast<std::uint32_t>(t.face));
        }
    }

    const views::ViewFrame& frame = view.frame;
    std::vector<ThreadImage> out;
    try {
        for (const Group& g : groups) {
            const Vec3 dir = toVec(g.thread.direction);
            const Vec3 entry3 = toVec(g.thread.entry);
            const double c = dot(dir, frame.toward);
            ThreadImage img;
            img.faces = g.faces;
            img.name = g.name;
            img.thread = g.thread;
            img.entry = frame.project(entry3);
            const double R = g.thread.nominal / 2.0;
            const double tol = 1e-6 * std::max(1.0, g.thread.nominal);

            // Рёбра граней резьбы и протяжённость стенки вдоль оси от входа.
            std::set<std::uint32_t> edges;
            double wallEnd = 0.0;
            struct EdgeCircle {
                std::uint32_t edge;
                double towardViewer;  // положение центра по направлению к наблюдателю
                double along;         // положение вдоль оси от входа
            };
            std::vector<EdgeCircle> circles;
            for (std::uint32_t f : g.faces) {
                const TopoDS_Face& face = topo.faces()[f].face;
                for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
                    auto e = topo.edgeIndex(ex.Current());
                    if (!e) continue;
                    edges.insert(*e);
                    const TopoDS_Edge& edge = TopoDS::Edge(ex.Current());
                    if (BRep_Tool::Degenerated(edge)) continue;
                    BRepAdaptor_Curve curve(edge);
                    if (curve.GetType() == GeomAbs_Circle) {
                        const Vec3 center = toVec(curve.Circle().Location());
                        circles.push_back({*e, dot(center, frame.toward), dot(center - entry3, dir)});
                    }
                }
                for (TopExp_Explorer ex(face, TopAbs_VERTEX); ex.More(); ex.Next()) {
                    const Vec3 p = toVec(BRep_Tool::Pnt(TopoDS::Vertex(ex.Current())));
                    wallEnd = std::max(wallEnd, dot(p - entry3, dir));
                }
            }
            auto belongs = [&](const ViewEdge& e) {
                if (e.faceIndex && std::find(g.faces.begin(), g.faces.end(), *e.faceIndex) != g.faces.end()) return true;
                return e.edgeIndex && edges.count(*e.edgeIndex) > 0;
            };

            if (std::abs(c) <= opt.angularTolerance) {
                // Ось параллельна плоскости проекции.
                img.aspect = ThreadAspect::Side;
                const Point2 a2 = frame.project(dir);
                const double la = norm(a2);
                img.axis = {a2.x / la, a2.y / la};
                bool any = false, visible = false;
                for (const ViewEdge& e : view.edges) {
                    if (!belongs(e)) continue;
                    auto s = straight(e.curve);
                    if (!s) continue;
                    const Point2 d = sub(s->b, s->a);
                    const double len = norm(d);
                    if (len < tol) continue;
                    if (std::abs(d.x * img.axis.y - d.y * img.axis.x) / len > 1e-6) continue;  // не вдоль оси
                    any = true;
                    if (e.visibility == Visibility::Visible) visible = true;
                }
                if (!any) continue;  // стенка на виде не изображена
                img.visibility = visible ? Visibility::Visible : Visibility::Hidden;
                const Point2 n{-img.axis.y, img.axis.x};
                const Point2 end = add(img.entry, mul(g.thread.length, img.axis));
                img.majorLines[0] = {add(img.entry, mul(R, n)), add(end, mul(R, n))};
                img.majorLines[1] = {sub(img.entry, mul(R, n)), sub(end, mul(R, n))};
                // Граница резьбы — если резьба кончается раньше стенки.
                if (g.thread.length < wallEnd - tol) img.boundary = LineSeg{add(end, mul(R, n)), sub(end, mul(R, n))};
                out.push_back(std::move(img));
            } else if (std::abs(std::abs(c) - 1.0) <= opt.angularTolerance) {
                // Вид вдоль оси.
                img.aspect = ThreadAspect::End;
                if (circles.empty()) continue;
                const auto near = std::max_element(circles.begin(), circles.end(), [](const EdgeCircle& x, const EdgeCircle& y) {
                    return x.towardViewer < y.towardViewer;
                });
                bool any = false, visible = false;
                for (const ViewEdge& e : view.edges) {
                    if (!e.edgeIndex || *e.edgeIndex != near->edge) continue;
                    any = true;
                    if (e.visibility == Visibility::Visible) visible = true;
                }
                if (!any) continue;
                // Резьба видна с этого края, только если доходит до него (вход — всегда).
                const bool reaches = near->along <= tol || g.thread.length >= near->along - tol;
                img.visibility = visible && reaches ? Visibility::Visible : Visibility::Hidden;
                img.majorArc = Arc{img.entry, R, opt.arcStartDeg, std::fmod(opt.arcStartDeg + opt.arcSweepDeg, 360.0)};
                out.push_back(std::move(img));
            }
            // Косой вид (аксонометрия): ГОСТ 2.311 изображение не задаёт — не выводится.
        }
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("thread image: OCCT: ") + f.what());
    }
    return out;
}

Status renderThreads(Drawing& drawing, const views::SheetView& view, const std::vector<ThreadImage>& threads,
                     const views::RenderOptions& o) {
    if (auto st = views::ensureViewLayers(drawing, o); !st) return st;
    if (!(view.scale > 0) || !std::isfinite(view.scale)) {
        return Status::failure(ErrorCode::InvalidInput, "render threads: scale must be positive");
    }
    auto addLine = [&](const LineSeg& l, LineType type, const std::string& layer) {
        return drawing.add(Entity{LineSeg{view.toSheet(l.a), view.toSheet(l.b)}, layer, type});
    };
    for (const ThreadImage& t : threads) {
        const bool hidden = t.visibility == Visibility::Hidden;
        if (hidden && !o.hidden) continue;
        const LineType thin = hidden ? LineType::Dashed : LineType::SolidThin;
        const std::string& thinLayer = hidden ? o.hiddenLayer : o.thinLayer;
        if (t.aspect == ThreadAspect::Side) {
            for (const LineSeg& l : t.majorLines) {
                if (auto st = addLine(l, thin, thinLayer); !st) return st;
            }
            if (t.boundary) {
                if (auto st = addLine(*t.boundary, hidden ? LineType::Dashed : LineType::SolidThick,
                                      hidden ? o.hiddenLayer : o.visibleLayer);
                    !st)
                    return st;
            }
        } else {
            const Arc& a = t.majorArc;
            Status st = drawing.add(Entity{Arc{view.toSheet(a.center), a.radius * view.scale, a.startDeg, a.endDeg},
                                           thinLayer, thin});
            if (!st) return st;
        }
    }
    return okStatus();
}

Result<DimensionResult> threadDimension(const ThreadImage& image, const views::SheetView& view,
                                        const ThreadDimensionDef& def, const AnnotationStyle& style) {
    if (!(view.scale > 0) || !std::isfinite(view.scale)) return invalid("thread dimension: view scale must be positive");
    DimensionResult res;
    res.state = DimensionState::Ok;
    res.text = threadDesignationText(image.thread, def.toleranceField);
    if (image.aspect == ThreadAspect::Side) {
        const Point2 a = view.toSheet(image.majorLines[0].a), b = view.toSheet(image.majorLines[1].a);
        res.value = norm(sub(b, a)) / view.scale;
        // Размерная линия — со стороны входа, против направления резьбы (вне материала).
        const Point2 side = mul(-1.0, image.axis);
        res.geometry = labelledLinearGeometry(a, b, side, def.offset, res.text, def.textShift, style);
    } else {
        res.value = 2.0 * image.majorArc.radius;
        res.geometry = labelledDiameterGeometry(view.toSheet(image.majorArc.center), image.majorArc.radius * view.scale,
                                                def.leaderAngleDeg, res.text, style);
    }
    return res;
}

}  // namespace bendcore::drawing_v2::annotations
