// Автокомпоновка размеров вида по ГОСТ 2.307-2011 (разд. 4): ряды размерных линий вне
// изображения (10 мм от контура, 7 мм между рядами), меньшие размеры ближе к контуру, без
// перекрытия надписей и, где возможно, без пересечения выносных линий с размерными; размеры
// окружностей — направление полки без пересечения с линиями изображения. Алгоритм —
// docs/drawing/ANNOTATIONS.md, «Автокомпоновка размеров».

#include <bendcore/drawing_v2/dimensions.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::annotations {

using namespace views;

namespace {

constexpr double kPi = std::numbers::pi;

double dot2(Point2 a, Point2 b) { return a.x * b.x + a.y * b.y; }

bool overlaps(const Box2& a, const Box2& b) {
    return !a.empty && !b.empty && std::min(a.maxX, b.maxX) > std::max(a.minX, b.minX) &&
           std::min(a.maxY, b.maxY) > std::max(a.minY, b.minY);
}

// Отрезок пересекает прямоугольник (отсечение Лианга — Барски).
bool segmentHitsBox(Point2 p, Point2 q, const Box2& b) {
    double t0 = 0.0, t1 = 1.0;
    const double dx = q.x - p.x, dy = q.y - p.y;
    const double pp[4] = {-dx, dx, -dy, dy};
    const double qq[4] = {p.x - b.minX, b.maxX - p.x, p.y - b.minY, b.maxY - p.y};
    for (int i = 0; i < 4; ++i) {
        if (std::abs(pp[i]) < 1e-15) {
            if (qq[i] < 0) return false;
            continue;
        }
        const double r = qq[i] / pp[i];
        if (pp[i] < 0) t0 = std::max(t0, r);
        else t1 = std::min(t1, r);
        if (t0 > t1) return false;
    }
    return true;
}

// Линии изображения на листе отрезками (дуги — по 10°).
std::vector<LineSeg> contourSegments(const SheetView& sv) {
    std::vector<LineSeg> out;
    auto arc = [&](Point2 c, double r, double a0, double sweep) {
        const int n = std::max(2, int(std::ceil(sweep / 10.0)));
        Point2 prev{};
        for (int i = 0; i <= n; ++i) {
            const double a = (a0 + sweep * i / n) * kPi / 180.0;
            const Point2 p = sv.toSheet({c.x + r * std::cos(a), c.y + r * std::sin(a)});
            if (i > 0) out.push_back({prev, p});
            prev = p;
        }
    };
    for (const ViewEdge& e : sv.view.edges) {
        if (const auto* l = std::get_if<LineSeg>(&e.curve)) {
            out.push_back({sv.toSheet(l->a), sv.toSheet(l->b)});
        } else if (const auto* a = std::get_if<Arc>(&e.curve)) {
            double sweep = a->endDeg - a->startDeg;
            while (sweep <= 0) sweep += 360.0;
            arc(a->center, a->radius, a->startDeg, sweep);
        } else if (const auto* c = std::get_if<Circle>(&e.curve)) {
            arc(c->center, c->radius, 0.0, 360.0);
        } else if (const auto* p = std::get_if<Polyline>(&e.curve)) {
            for (std::size_t i = 0; i + 1 < p->points.size(); ++i) out.push_back({sv.toSheet(p->points[i]), sv.toSheet(p->points[i + 1])});
        }
    }
    return out;
}

// Размер окружности (полка через центр), а не диаметр цилиндра сбоку: ссылка — ребро или
// цилиндрическая грань с осью вдоль взгляда.
bool isCircleDim(const DimensionDef& d, const ModelBinding& m, const ProjectedView& v) {
    if (d.type != DimensionType::Radius && d.type != DimensionType::Diameter) return false;
    const ModelRef& r = d.anchors.at(0).ref;
    if (r.kind != RefKind::Face) return true;
    if (r.index >= m.topology().faces().size()) return false;
    BRepAdaptor_Surface s(m.topology().faces()[r.index].face, false);
    if (s.GetType() != GeomAbs_Cylinder) return false;
    const gp_Dir ax = s.Cylinder().Axis().Direction();
    return std::abs(std::abs(ax.X() * v.frame.toward.x + ax.Y() * v.frame.toward.y + ax.Z() * v.frame.toward.z) - 1.0) < 1e-9;
}

struct Linear {
    std::size_t index = 0;
    Point2 u, n;          // направление измерения и нормаль (offset > 0 — по n)
    double sigma = 1.0;   // сторона: +1 — по n, −1 — против
    Point2 a, b;          // точки привязки на листе
    double span = 0.0;    // длина размерной линии между выносными линиями
};

struct Placed {
    double lo = 0, hi = 0;      // занятый ряд вдоль u (размерная линия и надпись)
    double lineLo = 0, lineHi = 0;  // размерная линия между выносными
    double ea = 0, eb = 0;      // координаты выносных линий вдоль u
    int tier = 0;
};

}  // namespace

std::vector<Box2> labelBoxes(const DimensionGeometry& g, const AnnotationStyle& st) {
    std::vector<Box2> out;
    for (const DimensionLabel& l : g.labels) {
        const double w = estimateTextWidth(l.text, l.height, st.font) + (st.slantedText ? 0.27 * l.height : 0.0);
        const double r = l.rotationDeg * kPi / 180.0;
        const Point2 ux{std::cos(r), std::sin(r)}, uy{-std::sin(r), std::cos(r)};
        double x0 = 0.0;
        if (l.hAlign == HAlign::Center) x0 = -w / 2.0;
        else if (l.hAlign == HAlign::Right) x0 = -w;
        Box2 b;
        for (double x : {x0, x0 + w})
            for (double y : {-0.3 * l.height, l.height})
                b.add({l.position.x + x * ux.x + y * uy.x, l.position.y + x * ux.y + y * uy.y});
        out.push_back(b);
    }
    return out;
}

Result<ArrangeResult> arrangeDimensions(const std::vector<DimensionDef>& defs, const ModelBinding& model,
                                        const SheetView& sv, const AnnotationStyle& st, const ArrangeOptions& opt) {
    if (!(opt.firstOffset > 0) || !(opt.spacing > 0) || opt.maxTiers < 1 || !(opt.clearance >= 0))
        return Error{ErrorCode::InvalidInput, "arrange dimensions: offsets, spacing and tiers must be positive"};
    ArrangeResult res;
    res.defs = defs;
    const Box2 contour = sv.sheetBounds();
    if (contour.empty) return Error{ErrorCode::InvalidInput, "arrange dimensions: the view has no geometry"};
    const Point2 corners[4] = {{contour.minX, contour.minY}, {contour.maxX, contour.minY}, {contour.minX, contour.maxY}, {contour.maxX, contour.maxY}};
    const Point2 centre{0.5 * (contour.minX + contour.maxX), 0.5 * (contour.minY + contour.maxY)};

    // Точки привязки линейных размеров; прочие (кроме окружностей) — препятствия как есть.
    std::vector<Linear> linear;
    std::vector<std::size_t> circles;
    std::vector<bool> moved(defs.size(), false);
    for (std::size_t i = 0; i < defs.size(); ++i) {
        const DimensionDef& d = defs[i];
        if (d.type == DimensionType::Horizontal || d.type == DimensionType::Vertical) {
            auto pa = anchorPoint(d.anchors.at(0), model, sv.view);
            auto pb = anchorPoint(d.anchors.at(1), model, sv.view);
            if (!pa || !pb) continue;  // висячий — без изменений
            Linear L;
            L.index = i;
            L.u = d.type == DimensionType::Horizontal ? Point2{1, 0} : Point2{0, 1};
            L.n = d.type == DimensionType::Horizontal ? Point2{0, 1} : Point2{-1, 0};
            L.a = sv.toSheet(pa.value());
            L.b = sv.toSheet(pb.value());
            L.span = std::abs(dot2(L.b, L.u) - dot2(L.a, L.u));
            if (d.offset != 0.0) {
                L.sigma = d.offset > 0 ? 1.0 : -1.0;
            } else {
                const double mid = 0.5 * (dot2(L.a, L.n) + dot2(L.b, L.n));
                L.sigma = mid >= dot2(centre, L.n) ? 1.0 : -1.0;
            }
            linear.push_back(L);
        } else if ((d.type == DimensionType::Radius || d.type == DimensionType::Diameter) && !d.anchors.empty() &&
                   isCircleDim(d, model, sv.view)) {
            circles.push_back(i);
        }
    }

    auto offsetFor = [&](const Linear& L, int tier) {
        double e = L.sigma > 0 ? -1e300 : 1e300;
        for (Point2 c : corners) e = L.sigma > 0 ? std::max(e, dot2(c, L.n)) : std::min(e, dot2(c, L.n));
        const double t = e + L.sigma * (opt.firstOffset + tier * opt.spacing);
        const double out = L.sigma > 0 ? std::max(dot2(L.a, L.n), dot2(L.b, L.n)) : std::min(dot2(L.a, L.n), dot2(L.b, L.n));
        return t - out;
    };

    // Линейные: по сторонам, от коротких к длинным.
    std::stable_sort(linear.begin(), linear.end(), [](const Linear& x, const Linear& y) { return x.span < y.span; });
    struct GroupKey {
        DimensionType type;
        double sigma;
    };
    std::vector<std::pair<GroupKey, std::vector<Placed>>> groups;
    auto groupOf = [&](const Linear& L) -> std::vector<Placed>& {
        const DimensionType t = defs[L.index].type;
        for (auto& [k, v] : groups)
            if (k.type == t && k.sigma == L.sigma) return v;
        groups.push_back({{t, L.sigma}, {}});
        return groups.back().second;
    };
    for (const Linear& L : linear) {
        std::vector<Placed>& placed = groupOf(L);
        const double ea = dot2(L.a, L.u), eb = dot2(L.b, L.u);
        // Варианты надписи: на месте по умолчанию; при тесноте — по другую сторону размерной линии.
        std::vector<double> shifts{defs[L.index].textShift};
        {
            DimensionDef probe = defs[L.index];
            probe.offsetFrom.clear();
            probe.offset = offsetFor(L, 0);
            probe.textShift = 0.0;
            auto ev = evaluateDimension(probe, model, sv, st);
            if (ev && ev.value().state == DimensionState::Ok) {
                double w = 0.0;
                for (const Box2& b : labelBoxes(ev.value().geometry, st))
                    w = std::max(w, std::abs(dot2({b.maxX - b.minX, b.maxY - b.minY}, L.u)));
                if (w + 2.0 > L.span) {
                    const bool inside = L.span >= 2.0 * st.arrowLength + 1.0;
                    const double s = L.span / 2.0 + (inside ? 2.0 : st.arrowLength + 4.0) + w / 2.0;
                    if (shifts[0] == 0.0) shifts.push_back(-s);
                    else shifts.push_back(0.0);
                }
            }
        }
        struct Option {
            DimensionDef def;
            Placed p;
            std::size_t crossings = 0;
        };
        std::optional<Option> best;
        int firstFeasible = -1;
        for (int k = 0; k < opt.maxTiers; ++k) {
            for (double shift : shifts) {
                DimensionDef d = defs[L.index];
                d.offsetFrom.clear();
                d.offset = offsetFor(L, k);
                d.textShift = shift;
                auto ev = evaluateDimension(d, model, sv, st);
                if (!ev || ev.value().state != DimensionState::Ok) continue;
                Placed p;
                p.tier = k;
                p.ea = std::min(ea, eb);
                p.eb = std::max(ea, eb);
                p.lineLo = p.ea;
                p.lineHi = p.eb;
                p.lo = 1e300;
                p.hi = -1e300;
                for (const LineSeg& s : ev.value().geometry.dimensionLines)
                    for (Point2 q : {s.a, s.b}) p.lo = std::min(p.lo, dot2(q, L.u)), p.hi = std::max(p.hi, dot2(q, L.u));
                for (const Box2& b : labelBoxes(ev.value().geometry, st))
                    for (Point2 q : {Point2{b.minX, b.minY}, Point2{b.maxX, b.maxY}})
                        p.lo = std::min(p.lo, dot2(q, L.u)), p.hi = std::max(p.hi, dot2(q, L.u));
                p.lo -= opt.clearance / 2.0;
                p.hi += opt.clearance / 2.0;
                bool free = true;
                std::size_t crossings = 0;
                for (const Placed& q : placed) {
                    if (q.tier == k && std::min(q.hi, p.hi) > std::max(q.lo, p.lo)) free = false;
                    auto inside = [](double x, double lo, double hi) { return x > lo + 1e-6 && x < hi - 1e-6; };
                    if (q.tier < k) crossings += int(inside(p.ea, q.lineLo, q.lineHi)) + int(inside(p.eb, q.lineLo, q.lineHi));
                    if (q.tier > k) crossings += int(inside(q.ea, p.lineLo, p.lineHi)) + int(inside(q.eb, p.lineLo, p.lineHi));
                }
                if (!free) continue;
                if (firstFeasible < 0) firstFeasible = k;
                if (!best || crossings < best->crossings) best = Option{d, p, crossings};
                if (crossings == 0) break;
            }
            if (best && best->crossings == 0) break;
            if (firstFeasible >= 0 && k >= firstFeasible + 2) break;
        }
        if (!best) continue;  // нет свободного ряда — без изменений
        res.crossings += best->crossings;
        placed.push_back(best->p);
        res.defs[L.index] = best->def;
        moved[L.index] = true;
    }

    // Надписи поставленных и неподвижных размеров — препятствия для размеров окружностей.
    std::vector<std::vector<Box2>> boxes(defs.size());
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (std::find(circles.begin(), circles.end(), i) != circles.end()) continue;
        auto ev = evaluateDimension(res.defs[i], model, sv, st);
        if (ev && ev.value().state == DimensionState::Ok) boxes[i] = labelBoxes(ev.value().geometry, st);
    }
    const std::vector<LineSeg> lines = contourSegments(sv);
    auto score = [&](const std::vector<Box2>& mine, std::size_t self) {
        std::size_t n = 0;
        for (const Box2& b : mine) {
            for (std::size_t j = 0; j < boxes.size(); ++j)
                if (j != self)
                    for (const Box2& o : boxes[j]) n += overlaps(b, o) ? 1 : 0;
            for (const LineSeg& s : lines) n += segmentHitsBox(s.a, s.b, b) ? 1 : 0;
        }
        return n;
    };
    for (std::size_t i : circles) {
        std::vector<double> angles{defs[i].leaderAngleDeg, 45, 135, 225, 315, 30, 60, 120, 150, 210, 240, 300, 330, 15, 75, 105, 165, 195, 255, 285, 345};
        std::optional<std::pair<std::size_t, DimensionDef>> best;
        std::vector<Box2> bestBoxes;
        for (double a : angles) {
            DimensionDef d = defs[i];
            d.leaderAngleDeg = a;
            auto ev = evaluateDimension(d, model, sv, st);
            if (!ev || ev.value().state != DimensionState::Ok) break;  // висячий — без изменений
            const auto mine = labelBoxes(ev.value().geometry, st);
            const std::size_t sc = score(mine, i);
            if (!best || sc < best->first) {
                best = {sc, d};
                bestBoxes = mine;
            }
            if (sc == 0) break;
        }
        if (!best) continue;
        boxes[i] = bestBoxes;
        if (best->second.leaderAngleDeg != defs[i].leaderAngleDeg) {
            res.defs[i] = best->second;
            moved[i] = true;
        }
    }

    // Итог: касания надписей друг с другом и с линиями изображения.
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (std::find(circles.begin(), circles.end(), i) == circles.end()) {
            auto ev = evaluateDimension(res.defs[i], model, sv, st);
            if (ev && ev.value().state == DimensionState::Ok) boxes[i] = labelBoxes(ev.value().geometry, st);
        }
    }
    for (std::size_t i = 0; i < defs.size(); ++i) {
        for (const Box2& b : boxes[i]) {
            for (std::size_t j = i + 1; j < defs.size(); ++j)
                for (const Box2& o : boxes[j]) res.collisions += overlaps(b, o) ? 1 : 0;
            for (const LineSeg& s : lines) res.collisions += segmentHitsBox(s.a, s.b, b) ? 1 : 0;
        }
    }
    res.arranged = std::size_t(std::count(moved.begin(), moved.end(), true));
    return res;
}

}  // namespace bendcore::drawing_v2::annotations
