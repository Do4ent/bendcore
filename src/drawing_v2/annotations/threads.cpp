// Изображение резьбы по ГОСТ 2.311-68 по условной резьбе модели (docs/drawing/ANNOTATIONS.md,
// раздел «Резьба»).

#include <bendcore/drawing_v2/threads.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <numbers>
#include <optional>
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
                // Окружности краёв стенки проецируются в одну; совпадающие участки HLR удаляет
                // (removeOverlaps), поэтому видимость — по любому изображению окружностей стенки.
                bool any = false, visible = false;
                for (const ViewEdge& e : view.edges) {
                    if (!e.edgeIndex) continue;
                    if (std::none_of(circles.begin(), circles.end(), [&](const EdgeCircle& x) { return x.edge == *e.edgeIndex; }))
                        continue;
                    any = true;
                    if (e.visibility == Visibility::Visible) visible = true;
                }
                if (!any) continue;
                // Резьба видна с ближнего к наблюдателю края стенки, только если доходит до него
                // (вход — всегда); глухое отверстие со стороны дна — край не виден вовсе.
                const bool reaches = near->along <= tol || g.thread.length >= near->along - tol;
                img.visibility = visible && reaches ? Visibility::Visible : Visibility::Hidden;
                img.majorArc = Arc{img.entry, R, opt.arcStartDeg, std::fmod(opt.arcStartDeg + opt.arcSweepDeg, 360.0)};
                out.push_back(std::move(img));
            } else {
                // Косой вид (аксонометрия ГОСТ 2.317): эллипс наружного диаметра у входа.
                img.aspect = ThreadAspect::Oblique;
                if (circles.empty()) continue;
                const auto entryEdge = std::min_element(circles.begin(), circles.end(), [](const EdgeCircle& x, const EdgeCircle& y) {
                    return std::abs(x.along) < std::abs(y.along);
                });
                const double along0 = entryEdge->along;
                bool any = false, visible = false;
                for (const ViewEdge& e : view.edges) {
                    if (!e.edgeIndex) continue;
                    const bool atEntry = std::any_of(circles.begin(), circles.end(), [&](const EdgeCircle& x) {
                        return x.edge == *e.edgeIndex && std::abs(x.along - along0) <= tol;
                    });
                    if (!atEntry) continue;
                    any = true;
                    if (e.visibility == Visibility::Visible) visible = true;
                }
                if (!any) continue;
                img.visibility = visible ? Visibility::Visible : Visibility::Hidden;
                // Ортонормированный базис плоскости окружности резьбы: u ⟂ dir, v = dir × u.
                const Vec3 seed = std::abs(dir.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
                Vec3 u = seed - dot(seed, dir) * dir;
                u = (1.0 / std::sqrt(dot(u, u))) * u;
                const Vec3 v = cross(dir, u);
                // Проекция направления: разность проекций точек (project — проекция точки).
                const Point2 o0 = frame.project(Vec3{0, 0, 0});
                img.ellipseU = sub(frame.project(u), o0);
                img.ellipseV = sub(frame.project(v), o0);
                out.push_back(std::move(img));
            }
        }
    } catch (const Standard_Failure& f) {
        return R::failure(ErrorCode::OperationFailed, std::string("thread image: OCCT: ") + f.what());
    }
    return out;
}

namespace {

// Участок отрезка a→b внутри окружности (c, R): параметры [t0, t1] ⊂ [0, 1].
std::optional<std::pair<double, double>> insideSpan(Point2 a, Point2 b, Point2 c, double R) {
    const Point2 d = sub(b, a), f = sub(a, c);
    const double A = d.x * d.x + d.y * d.y, B = 2 * (f.x * d.x + f.y * d.y), C = f.x * f.x + f.y * f.y - R * R;
    if (A < 1e-24) return std::nullopt;
    const double disc = B * B - 4 * A * C;
    if (disc <= 0) return std::nullopt;
    const double s = std::sqrt(disc);
    const double t0 = std::max(0.0, (-B - s) / (2 * A)), t1 = std::min(1.0, (-B + s) / (2 * A));
    if (t1 - t0 <= 1e-12) return std::nullopt;
    return std::pair{t0, t1};
}

// Ломаная, отсечённая окружностью: куски внутри (≥ 2 точек). Замкнутая ломаная (первая точка =
// последней), пересекающая окружность, даёт куски без разрыва в начальной точке.
std::vector<std::vector<Point2>> clipPolyline(const std::vector<Point2>& pts, Point2 c, double R) {
    std::vector<std::vector<Point2>> out;
    std::vector<Point2> cur;
    auto lerp = [](Point2 a, Point2 b, double t) { return Point2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; };
    for (std::size_t i = 1; i < pts.size(); ++i) {
        auto span = insideSpan(pts[i - 1], pts[i], c, R);
        if (!span) {
            if (cur.size() >= 2) out.push_back(std::move(cur));
            cur.clear();
            continue;
        }
        const Point2 p0 = lerp(pts[i - 1], pts[i], span->first), p1 = lerp(pts[i - 1], pts[i], span->second);
        if (cur.empty() || span->first > 1e-12) {
            if (cur.size() >= 2) out.push_back(std::move(cur));
            cur = {p0};
        }
        cur.push_back(p1);
        if (span->second < 1.0 - 1e-12) {
            out.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (cur.size() >= 2) out.push_back(std::move(cur));
    const bool closed = pts.size() > 2 && norm(sub(pts.front(), pts.back())) < 1e-12;
    if (closed && out.size() >= 2 && norm(sub(out.back().back(), out.front().front())) < 1e-12) {
        out.back().insert(out.back().end(), out.front().begin() + 1, out.front().end());
        out.erase(out.begin());
    }
    return out;
}

// Дуга (центр c, радиус r, от a0 на sweep градусов против ч. с.), отсечённая окружностью (cc, R):
// интервалы {начало, раствор} внутри окружности.
std::vector<std::pair<double, double>> clipArc(Point2 c, double r, double a0, double sweep, Point2 cc, double R) {
    const double dd = norm(sub(cc, c));
    if (dd + r <= R) return {{a0, sweep}};
    if (dd >= R + r || r >= dd + R) return {};
    // Точки окружности резьбы внутри — углы в пределах ±α от направления на центр ограничения.
    const double alpha =
        std::acos(std::clamp((dd * dd + r * r - R * R) / (2 * dd * r), -1.0, 1.0)) * 180.0 / std::numbers::pi;
    const double mid = std::atan2(cc.y - c.y, cc.x - c.x) * 180.0 / std::numbers::pi;
    std::vector<std::pair<double, double>> out;
    for (int k = -2; k <= 2; ++k) {
        const double lo = std::max(a0, mid - alpha + 360.0 * k), hi = std::min(a0 + sweep, mid + alpha + 360.0 * k);
        if (hi - lo > 1e-9) out.emplace_back(lo, hi - lo);
    }
    return out;
}

}  // namespace

double threadSheetRadius(const ThreadImage& image, double scale, double minThinGap) {
    return std::max(image.thread.nominal / 2.0 * scale, image.thread.drill / 2.0 * scale + minThinGap);
}

Status renderThreads(Drawing& drawing, const views::SheetView& view, const std::vector<ThreadImage>& threads,
                     const views::RenderOptions& o, double minThinGap) {
    if (auto st = views::ensureViewLayers(drawing, o); !st) return st;
    if (!(view.scale > 0) || !std::isfinite(view.scale)) {
        return Status::failure(ErrorCode::InvalidInput, "render threads: scale must be positive");
    }
    if (!(minThinGap >= 0) || !std::isfinite(minThinGap)) {
        return Status::failure(ErrorCode::InvalidInput, "render threads: minThinGap must be >= 0");
    }
    for (const ThreadImage& t : threads) {
        const bool hidden = t.visibility == Visibility::Hidden;
        if (hidden && !o.hidden) continue;
        const LineType thin = hidden ? LineType::Dashed : LineType::SolidThin;
        const std::string& thinLayer = hidden ? o.hiddenLayer : o.thinLayer;
        const double r = threadSheetRadius(t, view.scale, minThinGap);
        const Point2 c = view.toSheet(t.entry);
        std::optional<std::pair<Point2, double>> clip;
        if (t.clip) clip = std::pair{view.toSheet(t.clip->center), t.clip->radius * view.scale};
        // Ломаная (closed — последняя точка совпадает с первой) с отсечением окружностью
        // выносного элемента.
        auto addPath = [&](const std::vector<Point2>& pts, bool closed, const std::string& layer, LineType lt) -> Status {
            if (!clip) {
                if (pts.size() == 2) return drawing.add(Entity{LineSeg{pts[0], pts[1]}, layer, lt});
                std::vector<Point2> p = pts;
                if (closed) p.pop_back();
                return drawing.add(Entity{Polyline{p, closed}, layer, lt});
            }
            for (const auto& piece : clipPolyline(pts, clip->first, clip->second)) {
                Status st = piece.size() == 2 ? drawing.add(Entity{LineSeg{piece[0], piece[1]}, layer, lt})
                                              : drawing.add(Entity{Polyline{piece, false}, layer, lt});
                if (!st) return st;
            }
            return okStatus();
        };
        if (t.aspect == ThreadAspect::Side) {
            const Point2 n{-t.axis.y, t.axis.x};
            const Point2 end = add(c, mul(t.thread.length * view.scale, t.axis));
            for (double side : {1.0, -1.0}) {
                if (auto st = addPath({add(c, mul(side * r, n)), add(end, mul(side * r, n))}, false, thinLayer, thin); !st)
                    return st;
            }
            if (t.boundary) {
                if (auto st = addPath({add(end, mul(r, n)), sub(end, mul(r, n))}, false, hidden ? o.hiddenLayer : o.visibleLayer,
                                      hidden ? LineType::Dashed : LineType::SolidThick);
                    !st)
                    return st;
            }
        } else if (t.aspect == ThreadAspect::End) {
            const Arc& a = t.majorArc;
            if (!clip) {
                if (auto st = drawing.add(Entity{Arc{c, r, a.startDeg, a.endDeg}, thinLayer, thin}); !st) return st;
            } else {
                double sweep = std::fmod(a.endDeg - a.startDeg, 360.0);
                if (sweep <= 0.0) sweep += 360.0;
                for (const auto& [s0, sw] : clipArc(c, r, a.startDeg, sweep, clip->first, clip->second)) {
                    if (auto st = drawing.add(Entity{Arc{c, r, s0, s0 + sw}, thinLayer, thin}); !st) return st;
                }
            }
        } else {
            constexpr int kSegments = 72;
            std::vector<Point2> pts;
            for (int i = 0; i <= kSegments; ++i) {
                const double a = 2.0 * std::numbers::pi * (i % kSegments) / kSegments;
                pts.push_back(add(c, mul(r, add(mul(std::cos(a), t.ellipseU), mul(std::sin(a), t.ellipseV)))));
            }
            if (auto st = addPath(pts, true, thinLayer, thin); !st) return st;
        }
    }
    return okStatus();
}

Result<DimensionResult> threadDimension(const ThreadImage& image, const views::SheetView& view,
                                        const ThreadDimensionDef& def, const AnnotationStyle& style) {
    if (!(view.scale > 0) || !std::isfinite(view.scale)) return invalid("thread dimension: view scale must be positive");
    if (image.aspect == ThreadAspect::Oblique)
        return Result<DimensionResult>::failure(ErrorCode::Unsupported,
                                                "thread dimension: not drawn on an axonometric view (GOST 2.317)");
    DimensionResult res;
    res.state = DimensionState::Ok;
    res.text = threadDesignationText(image.thread, def.toleranceField);
    if (image.aspect == ThreadAspect::Side) {
        const Point2 a = view.toSheet(image.majorLines[0].a), b = view.toSheet(image.majorLines[1].a);
        res.value = norm(sub(b, a)) / view.scale;
        // Выносные линии — от линий, как они выведены на лист (renderThreads, зазор 0,8 мм).
        const double r = threadSheetRadius(image, view.scale);
        const Point2 c = view.toSheet(image.entry), n{-image.axis.y, image.axis.x};
        const Point2 a2 = add(c, mul(r, n)), b2 = sub(c, mul(r, n));
        // Размерная линия — со стороны входа, против направления резьбы (вне материала).
        const Point2 side = mul(-1.0, image.axis);
        res.geometry = labelledLinearGeometry(a2, b2, side, def.offset, res.text, def.textShift, style);
    } else {
        res.value = 2.0 * image.majorArc.radius;
        res.geometry = labelledDiameterGeometry(view.toSheet(image.majorArc.center), threadSheetRadius(image, view.scale),
                                                def.leaderAngleDeg, res.text, style);
    }
    return res;
}

}  // namespace bendcore::drawing_v2::annotations
