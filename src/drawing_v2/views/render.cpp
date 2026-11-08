// Вывод видов в модель чертежа: типы линий ГОСТ 2.303-68, штриховка ГОСТ 2.306-68,
// осевые и центровые линии, обозначения разрезов и выносных элементов ГОСТ 2.305-2008.

#include <bendcore/drawing_v2/view_sheet.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::views {

namespace {

Status addEntity(Drawing& d, Geometry g, const std::string& layer, LineType t) {
    return d.add(Entity{std::move(g), layer, t});
}

Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }

double polygonArea(const std::vector<Point2>& p) {
    double s = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const Point2& a = p[i];
        const Point2& b = p[(i + 1) % p.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

}  // namespace

Status ensureViewLayers(Drawing& drawing, const RenderOptions& o) {
    const std::pair<const std::string*, int> layers[] = {
        {&o.visibleLayer, 7}, {&o.hiddenLayer, 1}, {&o.thinLayer, 3},
        {&o.centerLayer, 4},  {&o.hatchLayer, 8},  {&o.symbolLayer, 2}};
    for (const auto& [name, color] : layers) {
        if (drawing.findLayer(*name)) continue;
        Status st = drawing.addLayer(Layer{*name, color});
        if (!st) return st;
    }
    return okStatus();
}

Status renderView(Drawing& drawing, const SheetView& view, const RenderOptions& o) {
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st;
    if (!(view.scale > 0) || !std::isfinite(view.scale))
        return Status::failure(ErrorCode::InvalidInput, "render view: scale must be positive");
    const double minLen = 1e-6;
    for (const ViewEdge& e : view.view.edges) {
        LineType type = LineType::SolidThick;
        std::string layer = o.visibleLayer;
        if (e.visibility == Visibility::Hidden) {
            if (!o.hidden) continue;
            type = LineType::Dashed;
            layer = o.hiddenLayer;
        } else if (e.cls == EdgeClass::Smooth) {
            if (!o.smooth) continue;
            type = LineType::SolidThin;
            layer = o.thinLayer;
        }
        Geometry g;
        if (auto* l = std::get_if<LineSeg>(&e.curve)) {
            const Point2 a = view.toSheet(l->a), b = view.toSheet(l->b);
            if (std::hypot(b.x - a.x, b.y - a.y) < minLen) continue;
            g = LineSeg{a, b};
        } else if (auto* a = std::get_if<Arc>(&e.curve)) {
            if (a->radius * view.scale < minLen) continue;
            g = Arc{view.toSheet(a->center), a->radius * view.scale, a->startDeg, a->endDeg};
        } else if (auto* c = std::get_if<Circle>(&e.curve)) {
            if (c->radius * view.scale < minLen) continue;
            g = Circle{view.toSheet(c->center), c->radius * view.scale};
        } else {
            const auto& p = std::get<Polyline>(e.curve);
            Polyline q;
            q.closed = p.closed;
            for (const Point2& pt : p.points) q.points.push_back(view.toSheet(pt));
            if (q.points.size() < 2 || (q.closed && q.points.size() < 3)) continue;
            g = std::move(q);
        }
        st = addEntity(drawing, std::move(g), layer, type);
        if (!st) return st;
    }
    return okStatus();
}

double gostHatchSpacing(double sheetAreaMm2) {
    if (!(sheetAreaMm2 > 0)) return 1.0;
    const double s = std::round(std::sqrt(sheetAreaMm2) / 10.0 * 2.0) / 2.0;
    return std::clamp(s, 1.0, 10.0);
}

Result<double> renderSectionHatch(Drawing& drawing, const SheetView& view,
                                  const SectionView& section, double spacing,
                                  const RenderOptions& o) {
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st.error();
    if (section.regions.empty())
        return Result<double>::failure(ErrorCode::InvalidInput, "hatch: section has no regions");
    if (spacing <= 0) {
        double area = 0;
        for (const auto& region : section.regions)
            for (const auto& loop : region) area += polygonArea(loop);
        spacing = gostHatchSpacing(std::abs(area) * view.scale * view.scale);
    }
    for (const auto& region : section.regions) {
        Hatch h;
        h.angleDeg = 45.0;  // металлы: 45° к линиям рамки (ГОСТ 2.306, п. 3)
        h.spacing = spacing;
        for (const auto& loop : region) {
            std::vector<Point2> l;
            for (const Point2& p : loop) l.push_back(view.toSheet(p));
            h.loops.push_back(std::move(l));
        }
        st = addEntity(drawing, std::move(h), o.hatchLayer, LineType::SolidThin);
        if (!st) return st.error();
    }
    return spacing;
}

namespace {

// Вылет o ∈ [2, 5] мм, при котором отрезок длиной half + o, начатый штрихом, кончается
// штрихом длиной не меньше 1 мм (ГОСТ 2.303: штрихпунктирные линии начинаются и
// оканчиваются штрихами). Ищется ближайший к preferred.
double pickOvershoot(double half, const std::vector<double>& pattern, double preferred) {
    double period = 0;
    for (double v : pattern) period += std::abs(v);
    const double dash = pattern.empty() ? 0.0 : pattern.front();
    if (period <= 0 || dash <= 0) return preferred;
    auto ok = [&](double o) {
        const double m = std::fmod(half + o, period);
        return m >= std::min(1.0, dash) && m <= dash;
    };
    preferred = std::clamp(preferred, 2.0, 5.0);
    for (int k = 0; k <= 300; ++k) {
        for (double o : {preferred + 0.01 * k, preferred - 0.01 * k})
            if (o >= 2.0 - 1e-12 && o <= 5.0 + 1e-12 && ok(o)) return o;
    }
    return preferred;
}

// Линия через центр c в направлении u длиной 2·(half + o): две половины от центра
// (пересечение штрихами) или одна сплошная тонкая.
Status centerLine(Drawing& d, Point2 c, Point2 u, double half, bool solid, const RenderOptions& o) {
    if (solid) {
        const double h = half + std::clamp(o.centerOvershoot, 2.0, 5.0);
        return addEntity(d, LineSeg{add(c, mul(-h, u)), add(c, mul(h, u))}, o.centerLayer,
                         LineType::SolidThin);
    }
    const double ov = pickOvershoot(half, d.lineTypeSpec(LineType::DashDotThin).pattern,
                                    o.centerOvershoot);
    const double h = half + ov;
    Status st = addEntity(d, LineSeg{c, add(c, mul(h, u))}, o.centerLayer, LineType::DashDotThin);
    if (!st) return st;
    return addEntity(d, LineSeg{c, add(c, mul(-h, u))}, o.centerLayer, LineType::DashDotThin);
}

}  // namespace

Status renderCenterLines(Drawing& drawing, const SheetView& view, const CenterGeometry& g,
                         const RenderOptions& o) {
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st;
    for (const CenterMark& m : g.marks) {
        const Point2 c = view.toSheet(m.center);
        const double r = m.radius * view.scale;
        const bool solid = 2.0 * r < 12.0;
        st = centerLine(drawing, c, {1, 0}, r, solid, o);
        if (!st) return st;
        st = centerLine(drawing, c, {0, 1}, r, solid, o);
        if (!st) return st;
    }
    for (const AxisLine& a : g.axes) {
        const Point2 p = view.toSheet(a.a), q = view.toSheet(a.b);
        const double len = std::hypot(q.x - p.x, q.y - p.y);
        if (len < 1e-6) continue;
        const Point2 u{(q.x - p.x) / len, (q.y - p.y) / len};
        const Point2 c{0.5 * (p.x + q.x), 0.5 * (p.y + q.y)};
        st = centerLine(drawing, c, u, 0.5 * len, 2.0 * a.radius * view.scale < 12.0, o);
        if (!st) return st;
    }
    return okStatus();
}

Status renderViewLabel(Drawing& drawing, const SheetView& view, const std::string& text,
                       const RenderOptions& o) {
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st;
    const Box2 b = view.sheetBounds();
    if (b.empty) return Status::failure(ErrorCode::InvalidInput, "view label: empty view");
    Text t;
    t.position = {0.5 * (b.minX + b.maxX), b.maxY + 5.0};
    t.utf8 = text;
    t.height = o.letterHeight;
    t.hAlign = HAlign::Center;
    t.vAlign = VAlign::Baseline;
    return addEntity(drawing, std::move(t), o.symbolLayer, LineType::SolidThin);
}

Result<SectionMarkGeometry> renderCuttingPlaneMark(Drawing& drawing, const SheetView& parent,
                                                   const CuttingPlane& plane,
                                                   const std::string& letter, double strokeLen,
                                                   const RenderOptions& o) {
    using R = Result<SectionMarkGeometry>;
    if (!(strokeLen >= 8.0 && strokeLen <= 20.0))
        return R::failure(ErrorCode::InvalidInput,
                          "cutting plane mark: stroke length must be 8..20 mm (GOST 2.303)");
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st.error();
    auto tr = cuttingTrace(parent.view.frame, parent.view.bounds, plane);
    if (!tr) return tr.error();
    const Point2 a = parent.toSheet(tr.value().a), b = parent.toSheet(tr.value().b);
    const double len = std::hypot(b.x - a.x, b.y - a.y);
    if (len < 1e-9) return R::failure(ErrorCode::InvalidInput, "cutting plane mark: degenerate trace");
    const Point2 t{(b.x - a.x) / len, (b.y - a.y) / len};
    const Point2 s = tr.value().sight2d;
    const Point2 perp{-s.y, s.x};

    // Параметры стрелки (ГОСТ 2.305, разд. 6): головка 5 мм, полуширина 1,25 мм, стержень 12 мм;
    // стрелка — на 2,5 мм от внешнего конца штриха; штрих — на 1 мм вне габарита изображения.
    const double clear = 1.0, fromEnd = 2.5, shaft = 12.0, head = 5.0, halfW = 1.25;
    SectionMarkGeometry g;
    const Point2 ends[2] = {a, b};
    const Point2 outward[2] = {mul(-1, t), t};
    for (int i = 0; i < 2; ++i) {
        const Point2 inner = add(ends[i], mul(clear, outward[i]));
        const Point2 outer = add(inner, mul(strokeLen, outward[i]));
        g.strokes[i] = LineSeg{inner, outer};
        const Point2 tip = add(outer, mul(-fromEnd, outward[i]));
        const Point2 tail = add(tip, mul(-shaft, s));
        const Point2 base = add(tip, mul(-head, s));
        g.arrowShafts[i] = LineSeg{tail, base};
        g.arrowHeads[i] = Polyline{{tip, add(base, mul(halfW, perp)), add(base, mul(-halfW, perp))}, true};
        g.letterPos[i] = add(add(tip, mul(-0.5 * shaft, s)), mul(fromEnd + 0.5 * o.letterHeight, outward[i]));
        st = addEntity(drawing, g.strokes[i], o.symbolLayer, LineType::Open);
        if (!st) return st.error();
        st = addEntity(drawing, g.arrowShafts[i], o.symbolLayer, LineType::SolidThin);
        if (!st) return st.error();
        st = addEntity(drawing, g.arrowHeads[i], o.symbolLayer, LineType::SolidThin);
        if (!st) return st.error();
        Text txt;
        txt.position = g.letterPos[i];
        txt.utf8 = letter;
        txt.height = o.letterHeight;
        txt.hAlign = HAlign::Center;
        txt.vAlign = VAlign::Middle;
        st = addEntity(drawing, std::move(txt), o.symbolLayer, LineType::SolidThin);
        if (!st) return st.error();
    }
    return g;
}

Status renderDetailMark(Drawing& drawing, const SheetView& parent, Point2 center, double radius,
                        const std::string& letter, const RenderOptions& o) {
    if (!(radius > 0)) return Status::failure(ErrorCode::InvalidInput, "detail mark: radius <= 0");
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st;
    const Point2 c = parent.toSheet(center);
    const double r = radius * parent.scale;
    st = addEntity(drawing, Circle{c, r}, o.thinLayer, LineType::SolidThin);
    if (!st) return st;
    // Линия-выноска от окружности под 45° и полка длиной 2·h буквы.
    const double k = std::sqrt(0.5);
    const Point2 p0{c.x + r * k, c.y + r * k};
    const Point2 p1{p0.x + 8.0 * k, p0.y + 8.0 * k};
    const Point2 p2{p1.x + 2.0 * o.letterHeight, p1.y};
    st = addEntity(drawing, LineSeg{p0, p1}, o.thinLayer, LineType::SolidThin);
    if (!st) return st;
    st = addEntity(drawing, LineSeg{p1, p2}, o.thinLayer, LineType::SolidThin);
    if (!st) return st;
    Text t;
    t.position = {0.5 * (p1.x + p2.x), p1.y + 1.0};
    t.utf8 = letter;
    t.height = o.letterHeight;
    t.hAlign = HAlign::Center;
    return addEntity(drawing, std::move(t), o.symbolLayer, LineType::SolidThin);
}

Status renderDetailBoundary(Drawing& drawing, const SheetView& detail, Point2 center,
                            double radius, const RenderOptions& o) {
    if (!(radius > 0)) return Status::failure(ErrorCode::InvalidInput, "detail boundary: radius <= 0");
    Status st = ensureViewLayers(drawing, o);
    if (!st) return st;
    return addEntity(drawing, Circle{detail.toSheet(center), radius * detail.scale}, o.thinLayer,
                     LineType::SolidThin);
}

}  // namespace bendcore::drawing_v2::views
