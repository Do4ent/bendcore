// Размеры ГОСТ 2.307-2011 / ISO 129-1 на видах с привязкой к модели. Алгоритм и допущения —
// docs/drawing/ANNOTATIONS.md, раздел «Размеры».

#include <bendcore/drawing_v2/dimensions.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bendcore::drawing_v2::annotations {

using namespace views;

namespace {

constexpr double kPi = std::numbers::pi;

Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 sub(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }
double dot2(Point2 a, Point2 b) { return a.x * b.x + a.y * b.y; }
double cross2(Point2 a, Point2 b) { return a.x * b.y - a.y * b.x; }
double norm(Point2 a) { return std::hypot(a.x, a.y); }
Point2 unit(Point2 a) {
    const double n = norm(a);
    return {a.x / n, a.y / n};
}
Point2 perp(Point2 a) { return {-a.y, a.x}; }
double angleDeg(Point2 a) { return std::atan2(a.y, a.x) * 180.0 / kPi; }

Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }

modeling::ElementKind elementKind(RefKind k) {
    switch (k) {
        case RefKind::Face: return modeling::ElementKind::Face;
        case RefKind::Edge: return modeling::ElementKind::Edge;
        case RefKind::Vertex: return modeling::ElementKind::Vertex;
    }
    return modeling::ElementKind::Edge;
}

const char* kindName(RefKind k) {
    switch (k) {
        case RefKind::Face: return "face";
        case RefKind::Edge: return "edge";
        case RefKind::Vertex: return "vertex";
    }
    return "?";
}

std::string refLabel(const ModelRef& r) {
    std::string s = std::string(kindName(r.kind)) + " #" + std::to_string(r.index);
    if (!r.persistent.name.empty()) s += " '" + r.persistent.name + "'";
    return s;
}

std::vector<Point2> endpoints(const Curve2& c) {
    if (const auto* l = std::get_if<LineSeg>(&c)) return {l->a, l->b};
    if (const auto* a = std::get_if<Arc>(&c)) {
        const double s = a->startDeg * kPi / 180.0, e = a->endDeg * kPi / 180.0;
        return {{a->center.x + a->radius * std::cos(s), a->center.y + a->radius * std::sin(s)},
                {a->center.x + a->radius * std::cos(e), a->center.y + a->radius * std::sin(e)}};
    }
    if (const auto* p = std::get_if<Polyline>(&c)) {
        if (p->points.empty()) return {};
        return {p->points.front(), p->points.back()};
    }
    return {};
}

double viewTolerance(const ProjectedView& v) {
    return 1e-4 * std::max(1.0, std::hypot(v.bounds.width(), v.bounds.height())) + 1e-6;
}

std::vector<const ViewEdge*> piecesOfEdge(const ProjectedView& v, std::uint32_t idx) {
    std::vector<const ViewEdge*> out;
    for (const ViewEdge& e : v.edges) {
        if (e.edgeIndex && *e.edgeIndex == idx) out.push_back(&e);
    }
    return out;
}

std::vector<const ViewEdge*> piecesOfFace(const ProjectedView& v, std::uint32_t idx) {
    std::vector<const ViewEdge*> out;
    for (const ViewEdge& e : v.edges) {
        if (e.faceIndex && *e.faceIndex == idx) out.push_back(&e);
    }
    return out;
}

Vec3 toVec(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }
Vec3 toVec(const gp_Dir& p) { return {p.X(), p.Y(), p.Z()}; }

bool parallel(Vec3 a, Vec3 b) { return std::abs(std::abs(dot(a, b)) - 1.0) < 1e-9; }

// Ошибка привязки — причина «висячего» размера.
Error dangling(std::string m) { return Error{ErrorCode::OperationFailed, std::move(m)}; }

Result<std::monostate> checkIndex(const ModelRef& r, const ModelBinding& m) {
    const auto& t = m.topology();
    const std::size_t n = r.kind == RefKind::Face ? t.faces().size()
                          : r.kind == RefKind::Edge ? t.edges().size()
                                                    : t.vertices().size();
    if (r.index >= n) return dangling(refLabel(r) + " does not exist in the model (" + std::to_string(n) + " " +
                                      kindName(r.kind) + "s)");
    return std::monostate{};
}

// Есть ли на виде изображение грани: очерк грани или рёбра её контура.
bool faceShown(const ModelBinding& m, const ProjectedView& v, std::uint32_t faceIdx) {
    if (!piecesOfFace(v, faceIdx).empty()) return true;
    const TopoDS_Face& f = m.topology().faces()[faceIdx].face;
    for (TopExp_Explorer ex(f, TopAbs_EDGE); ex.More(); ex.Next()) {
        if (auto ei = m.topology().edgeIndex(ex.Current()); ei && !piecesOfEdge(v, *ei).empty()) return true;
    }
    return false;
}

struct CircleOnView {
    Point2 center;  // координаты вида
    double radius = 0.0;
};

// Окружность ребра или цилиндрической грани, видимая в натуральную величину.
Result<CircleOnView> circleOf(const ModelRef& r, const ModelBinding& m, const ProjectedView& v) {
    if (auto c = checkIndex(r, m); !c) return c.error();
    const Vec3 toward = v.frame.toward;
    if (r.kind == RefKind::Edge) {
        const auto pieces = piecesOfEdge(v, r.index);
        if (pieces.empty()) return dangling(refLabel(r) + " is not shown in the view");
        for (const ViewEdge* e : pieces) {
            if (const auto* c = std::get_if<Circle>(&e->curve)) return CircleOnView{c->center, c->radius};
            if (const auto* a = std::get_if<Arc>(&e->curve)) return CircleOnView{a->center, a->radius};
        }
        BRepAdaptor_Curve curve(m.topology().edges()[r.index].edge);
        if (curve.GetType() != GeomAbs_Circle) return dangling(refLabel(r) + " is not a circle");
        const gp_Circ c = curve.Circle();
        if (!parallel(toVec(c.Axis().Direction()), toward)) {
            return dangling(refLabel(r) + ": the circle is not seen in true shape in this view");
        }
        // Вид по триангуляции (PolyAlgo): окружность — ломаная; центр и радиус — из модели.
        return CircleOnView{v.frame.project(toVec(c.Location())), c.Radius()};
    }
    if (r.kind == RefKind::Face) {
        BRepAdaptor_Surface s(m.topology().faces()[r.index].face, false);
        if (s.GetType() != GeomAbs_Cylinder) return dangling(refLabel(r) + " is not a cylinder");
        const gp_Cylinder cyl = s.Cylinder();
        if (!parallel(toVec(cyl.Axis().Direction()), toward)) {
            return dangling(refLabel(r) + ": the cylinder axis is not along the line of sight");
        }
        if (!faceShown(m, v, r.index)) return dangling(refLabel(r) + " is not shown in the view");
        return CircleOnView{v.frame.project(toVec(cyl.Location())), cyl.Radius()};
    }
    return invalid("radius/diameter needs an edge or a face reference");
}

struct SideCylinder {
    LineSeg first, second;  // очерковые линии (координаты вида)
    Point2 dir;             // направление образующей
    double diameter = 0.0;
};

// Цилиндр сбоку: две параллельные очерковые линии грани.
Result<SideCylinder> sideCylinder(const ModelRef& r, const ModelBinding& m, const ProjectedView& v) {
    if (auto c = checkIndex(r, m); !c) return c.error();
    std::vector<LineSeg> lines;
    for (const ViewEdge* e : piecesOfFace(v, r.index)) {
        if (e->cls != EdgeClass::Silhouette) continue;
        if (const auto* l = std::get_if<LineSeg>(&e->curve)) lines.push_back(*l);
    }
    // Шов цилиндра, совпавший с очерком, привязан к своему ребру (VIEWS.md, «Точный HLR»):
    // прямолинейные рёбра грани тоже дают очерковые линии.
    for (TopExp_Explorer ex(m.topology().faces()[r.index].face, TopAbs_EDGE); ex.More(); ex.Next()) {
        const auto ei = m.topology().edgeIndex(ex.Current());
        if (!ei || m.topology().edges()[*ei].curve != kernel::CurveType::Line) continue;
        for (const ViewEdge* e : piecesOfEdge(v, *ei)) {
            if (const auto* l = std::get_if<LineSeg>(&e->curve)) lines.push_back(*l);
        }
    }
    if (lines.empty()) return dangling(refLabel(r) + ": no silhouette lines of the cylinder in the view");
    Point2 d = unit(sub(lines[0].b, lines[0].a));
    // Каноническое направление образующей: вверх, для горизонтальной — вправо.
    if (d.y < -1e-9 || (std::abs(d.y) <= 1e-9 && d.x < 0)) d = mul(-1.0, d);
    const Point2 nrm = perp(d);
    const double s0 = dot2(lines[0].a, nrm);
    const LineSeg* other = nullptr;
    double best = 0.0;
    for (const LineSeg& l : lines) {
        if (std::abs(cross2(unit(sub(l.b, l.a)), d)) > 1e-6) continue;
        const double s = std::abs(dot2(l.a, nrm) - s0);
        if (s > best) {
            best = s;
            other = &l;
        }
    }
    if (!other || best < viewTolerance(v)) {
        return dangling(refLabel(r) + ": only one silhouette line of the cylinder is shown");
    }
    return SideCylinder{lines[0], *other, d, best};
}

// Прямая ребра на виде: точка, направление, крайние точки всех участков.
struct LineOnView {
    Point2 origin;
    Point2 dir;
    std::vector<Point2> points;
};

Result<LineOnView> lineOf(const ModelRef& r, const ModelBinding& m, const ProjectedView& v) {
    if (auto c = checkIndex(r, m); !c) return c.error();
    if (r.kind != RefKind::Edge) return invalid("angle dimension needs edge references");
    const auto pieces = piecesOfEdge(v, r.index);
    if (pieces.empty()) return dangling(refLabel(r) + " is not shown in the view");
    LineOnView out;
    bool have = false;
    for (const ViewEdge* e : pieces) {
        const auto* l = std::get_if<LineSeg>(&e->curve);
        if (!l) continue;
        if (!have) {
            out.origin = l->a;
            out.dir = unit(sub(l->b, l->a));
            have = true;
        }
        out.points.push_back(l->a);
        out.points.push_back(l->b);
    }
    if (!have) return dangling(refLabel(r) + " is not a straight line in the view");
    return out;
}

// --- Компоновка надписи размера ---

struct TextPiece {
    double x = 0.0, y = 0.0;  // начало базовой линии в системе надписи
    std::string text;
    double height = 3.5;
};

struct TextBlock {
    std::vector<TextPiece> pieces;
    double width = 0.0;
    double minY = 0.0;  // низ блока относительно базовой линии основного текста
};

double width(const std::string& s, double h, const AnnotationStyle& st) { return estimateTextWidth(s, h, st.font); }

TextBlock composeText(const std::string& main, const std::optional<ToleranceText>& tol, const std::string& suffix,
                      const AnnotationStyle& st) {
    TextBlock b;
    const double h = st.textHeight, hd = st.toleranceTextHeight;
    const std::string first = tol ? tol->main : main;
    b.pieces.push_back({0.0, 0.0, first, h});
    double x = width(first, h, st);
    if (tol && (tol->upper || tol->lower)) {
        if (!tol->suffixOpen.empty()) {
            b.pieces.push_back({x, 0.0, tol->suffixOpen, h});
            x += width(tol->suffixOpen, h, st);
        }
        x += 0.2 * h;
        const double upY = h / 2.0 + 0.5, lowY = h / 2.0 - 0.5 - hd;
        double wd = 0.0;
        if (tol->upper) {
            b.pieces.push_back({x, upY, *tol->upper, hd});
            wd = std::max(wd, width(*tol->upper, hd, st));
        }
        if (tol->lower) {
            b.pieces.push_back({x, lowY, *tol->lower, hd});
            wd = std::max(wd, width(*tol->lower, hd, st));
            b.minY = lowY;
        }
        x += wd;
        if (!tol->suffixClose.empty()) {
            x += 0.2 * h;
            b.pieces.push_back({x, 0.0, tol->suffixClose, h});
            x += width(tol->suffixClose, h, st);
        }
    }
    if (!suffix.empty()) {
        b.pieces.push_back({x, 0.0, suffix, h});
        x += width(suffix, h, st);
    }
    b.width = x;
    return b;
}

// Размещает блок: center — середина основания блока на линии, r — направление строки,
// up — нормаль «вверх» строки; блок приподнят на textGap над линией.
void placeBlock(DimensionGeometry& g, const TextBlock& b, Point2 center, Point2 r, Point2 up,
                const AnnotationStyle& st, double extraLift = 0.0) {
    const double lift = st.textGap - b.minY + extraLift;
    const double rot = angleDeg(r);
    for (const TextPiece& p : b.pieces) {
        const double lx = p.x - b.width / 2.0, ly = p.y + lift;
        g.labels.push_back({add(center, add(mul(lx, r), mul(ly, up))), rot, p.text, p.height, HAlign::Left});
    }
}

// Направление строки, читаемое слева направо или снизу вверх (угол в (−90°, 90°]).
Point2 readable(Point2 u) {
    const double a = angleDeg(u);
    if (a > 90.0 + 1e-9 || a <= -90.0 + 1e-9) return mul(-1.0, u);
    return u;
}

void addArrow(DimensionGeometry& g, Point2 tip, Point2 dir, const AnnotationStyle& st) {
    g.arrows.push_back(arrowHead(tip, dir, st.arrowLength, st.arrowAngleDeg));
}

// Линейный размер между точками листа a и b: измерение вдоль u, размерная линия — на
// координате t по нормали n.
void buildLinear(DimensionGeometry& g, Point2 a, Point2 b, Point2 u, Point2 n, double t, const TextBlock& text,
                 double textShift, const AnnotationStyle& st) {
    auto foot = [&](Point2 p) { return add(p, mul(t - dot2(p, n), n)); };
    for (Point2 p : {a, b}) {
        const double d = t - dot2(p, n);
        if (std::abs(d) <= st.extensionGap + 1e-9) continue;
        const double s = d > 0 ? 1.0 : -1.0;
        g.extensionLines.push_back({add(p, mul(s * st.extensionGap, n)), add(p, mul(d + s * st.extensionOvershoot, n))});
    }
    Point2 fa = foot(a), fb = foot(b);
    if (dot2(sub(fb, fa), u) < 0) std::swap(fa, fb);
    const double L = norm(sub(fb, fa));
    const bool inside = L >= 2.0 * st.arrowLength + 1.0;
    Point2 lineA = fa, lineB = fb;
    if (inside) {
        addArrow(g, fa, mul(-1.0, u), st);
        addArrow(g, fb, u, st);
    } else {
        // Мало места: стрелки снаружи, размерная линия продолжена за выносные линии.
        lineA = sub(fa, mul(st.arrowLength + 3.0, u));
        lineB = add(fb, mul(st.arrowLength + 3.0, u));
        addArrow(g, fa, u, st);
        addArrow(g, fb, mul(-1.0, u), st);
    }
    const Point2 r = readable(u);
    const Point2 up = perp(r);
    Point2 center = add(mul(0.5, add(fa, fb)), mul(textShift, u));
    if (textShift == 0.0 && text.width + 2.0 > L) {
        // Число не помещается между выносными линиями — на продолжении размерной линии.
        const double out = (inside ? 2.0 : st.arrowLength + 4.0) + text.width / 2.0;
        center = add(fb, mul(out, u));
        const Point2 end = add(center, mul(text.width / 2.0 + 1.0, u));
        if (dot2(sub(end, lineB), u) > 0) lineB = end;
    }
    g.dimensionLines.push_back({lineA, lineB});
    placeBlock(g, text, center, r, up, st);
}

// Полка с надписью от точки from (конец наклонной линии).
void buildShelf(DimensionGeometry& g, Point2 from, bool toRight, const TextBlock& text, const AnnotationStyle& st) {
    const double len = text.width + 2.0;
    const Point2 to{from.x + (toRight ? len : -len), from.y};
    g.dimensionLines.push_back({from, to});
    placeBlock(g, text, {(from.x + to.x) / 2.0, from.y}, {1, 0}, {0, 1}, st);
}

void buildCircleDim(DimensionGeometry& g, Point2 c, double r, bool diameter, double angle, const TextBlock& text,
                    const AnnotationStyle& st) {
    const Point2 dir{std::cos(angle * kPi / 180.0), std::sin(angle * kPi / 180.0)};
    const Point2 p2 = add(c, mul(r, dir));
    const Point2 p1 = diameter ? sub(c, mul(r, dir)) : c;
    const double room = r - st.arrowLength - 1.0 - text.width;
    addArrow(g, p2, dir, st);
    if (diameter) addArrow(g, p1, mul(-1.0, dir), st);
    if (room >= 1.0) {
        g.dimensionLines.push_back({p1, p2});
        const Point2 rr = readable(dir);
        const Point2 center = add(c, mul(r - st.arrowLength - 1.0 - text.width / 2.0, dir));
        placeBlock(g, text, center, rr, perp(rr), st);
    } else {
        const Point2 knee = add(p2, mul(st.arrowLength + 3.0, dir));
        g.dimensionLines.push_back({p1, knee});
        buildShelf(g, knee, dir.x >= 0.0, text, st);
    }
}

Status checkAnchorKind(const Anchor& a) {
    switch (a.kind) {
        case AnchorKind::Start:
        case AnchorKind::End:
            if (a.ref.kind != RefKind::Edge) return invalid("anchor Start/End needs an edge reference");
            break;
        case AnchorKind::Center:
            if (a.ref.kind == RefKind::Vertex) return invalid("anchor Center needs an edge or a face reference");
            break;
        case AnchorKind::Point:
            if (a.ref.kind != RefKind::Vertex) return invalid("anchor Point needs a vertex reference");
            break;
    }
    return okStatus();
}

Status validateDef(const DimensionDef& d) {
    const bool linear = d.type == DimensionType::Horizontal || d.type == DimensionType::Vertical ||
                        d.type == DimensionType::Aligned;
    if (linear && d.anchors.size() != 2) return invalid("linear dimension needs 2 anchors");
    if ((d.type == DimensionType::Radius || d.type == DimensionType::Diameter) && d.anchors.size() != 1) {
        return invalid("radius/diameter dimension needs 1 reference");
    }
    if (d.type == DimensionType::Angle) {
        if (d.anchors.size() != 2) return invalid("angle dimension needs 2 edges");
        for (const auto& a : d.anchors) {
            if (a.ref.kind != RefKind::Edge) return invalid("angle dimension needs edge references");
        }
        if (!(d.offset > 0)) return invalid("angle dimension: arc radius (offset) must be positive");
        if (d.tolerance) return invalid("angle dimension: ISO 286 tolerance classes apply to linear sizes only");
    }
    if (d.type == DimensionType::Radius || d.type == DimensionType::Diameter) {
        if (d.anchors[0].ref.kind == RefKind::Vertex) return invalid("radius/diameter needs an edge or a face");
    }
    if (linear) {
        for (const auto& a : d.anchors) {
            if (auto s = checkAnchorKind(a); !s) return s;
        }
    }
    for (const auto& a : d.offsetFrom) {
        if (auto s = checkAnchorKind(a); !s) return s;
    }
    if (!std::isfinite(d.offset) || !std::isfinite(d.textShift) || !std::isfinite(d.leaderAngleDeg)) {
        return invalid("dimension: non-finite parameter");
    }
    return okStatus();
}

std::string substitute(const std::string& pattern, const std::string& value) {
    std::string out = pattern;
    for (std::size_t p = out.find("<>"); p != std::string::npos; p = out.find("<>", p + value.size())) {
        out.replace(p, 2, value);
    }
    return out;
}

}  // namespace

// --- ModelBinding ---

Result<ModelBinding> ModelBinding::fromNamedShape(const modeling::NamedShape& named) {
    if (named.isNull()) return invalid("model binding: empty shape");
    ModelBinding b;
    b.shape_ = std::make_shared<kernel::Shape>(named.shape());
    auto topo = kernel::TopologyIndex::build(*b.shape_);
    if (!topo) return topo.error();
    b.topology_ = std::make_shared<kernel::TopologyIndex>(std::move(topo).value());
    if (b.topology_->faces().size() != named.count(modeling::ElementKind::Face) ||
        b.topology_->edges().size() != named.count(modeling::ElementKind::Edge)) {
        return invalid("model binding: element map does not match the topology index");
    }
    b.named_ = std::make_shared<modeling::NamedShape>(named);
    b.historyNames_ = true;
    return b;
}

Result<ModelBinding> ModelBinding::fromShape(const kernel::Shape& shape) {
    if (shape.isNull()) return invalid("model binding: empty shape");
    modeling::NamingRequest req;
    req.featureId = "Model";
    req.result = shape.occt();
    auto named = modeling::buildNamedShape(req);
    if (!named) return named.error();
    auto b = fromNamedShape(named.value());
    if (!b) return b.error();
    b.value().historyNames_ = false;
    return b;
}

Result<ModelRef> ModelBinding::ref(RefKind kind, std::uint32_t index) const {
    const auto ek = elementKind(kind);
    if (index >= named_->count(ek)) {
        return invalid(std::string("model binding: ") + kindName(kind) + " index " + std::to_string(index) +
                       " out of range");
    }
    auto tr = modeling::makeReference(*named_, named_->element(ek, index));
    if (!tr) return tr.error();
    ModelRef r;
    r.kind = kind;
    r.index = index;
    r.persistent = std::move(tr).value();
    if (!historyNames_) r.persistent.name.clear();
    return r;
}

// --- Точки привязки ---

Result<Point2> anchorPoint(const Anchor& a, const ModelBinding& m, const ProjectedView& v) {
    if (auto s = checkAnchorKind(a); !s) return s.error();
    if (auto c = checkIndex(a.ref, m); !c) return c.error();
    const double tol = viewTolerance(v);
    try {
        switch (a.kind) {
            case AnchorKind::Start:
            case AnchorKind::End: {
                const TopoDS_Edge& edge = m.topology().edges()[a.ref.index].edge;
                const TopoDS_Vertex vx = a.kind == AnchorKind::Start ? TopExp::FirstVertex(edge, false)
                                                                     : TopExp::LastVertex(edge, false);
                if (vx.IsNull()) return dangling(refLabel(a.ref) + " has no end vertex");
                const Point2 p = v.frame.project(toVec(BRep_Tool::Pnt(vx)));
                const auto pieces = piecesOfEdge(v, a.ref.index);
                if (pieces.empty()) return dangling(refLabel(a.ref) + " is not shown in the view");
                double best = 1e300;
                Point2 snap{};
                for (const ViewEdge* e : pieces) {
                    for (Point2 q : endpoints(e->curve)) {
                        const double d = norm(sub(q, p));
                        if (d < best) {
                            best = d;
                            snap = q;
                        }
                    }
                }
                if (best > tol) {
                    return dangling(std::string(a.kind == AnchorKind::Start ? "start" : "end") + " of " +
                                    refLabel(a.ref) + " is not shown in the view");
                }
                return snap;
            }
            case AnchorKind::Center: {
                if (a.ref.kind == RefKind::Edge) {
                    auto c = circleOf(a.ref, m, v);
                    if (!c) return c.error();
                    return c.value().center;
                }
                BRepAdaptor_Surface s(m.topology().faces()[a.ref.index].face, false);
                if (s.GetType() != GeomAbs_Cylinder) return dangling(refLabel(a.ref) + " is not a cylinder");
                if (!faceShown(m, v, a.ref.index)) return dangling(refLabel(a.ref) + " is not shown in the view");
                const gp_Cylinder cyl = s.Cylinder();
                double u0, u1, v0, v1;
                BRepTools::UVBounds(m.topology().faces()[a.ref.index].face, u0, u1, v0, v1);
                const gp_Pnt mid = cyl.Location().Translated(gp_Vec(cyl.Axis().Direction()) * (0.5 * (v0 + v1)));
                return v.frame.project(toVec(mid));
            }
            case AnchorKind::Point: {
                const TopoDS_Vertex& vx = m.topology().vertices()[a.ref.index].vertex;
                const Point2 p = v.frame.project(toVec(BRep_Tool::Pnt(vx)));
                for (const ViewEdge& e : v.edges) {
                    for (Point2 q : endpoints(e.curve)) {
                        if (norm(sub(q, p)) <= tol) return q;
                    }
                }
                return dangling(refLabel(a.ref) + " is not shown in the view");
            }
        }
    } catch (const Standard_Failure& f) {
        return dangling(std::string("OCCT: ") + f.what());
    }
    return invalid("anchor: unknown kind");
}

DimensionGeometry labelledLinearGeometry(Point2 a, Point2 b, Point2 side, double offset, const std::string& text,
                                         double textShift, const AnnotationStyle& st) {
    DimensionGeometry g;
    const Point2 u = unit(sub(b, a));
    const Point2 n = unit(side);
    const double out = std::max(dot2(a, n), dot2(b, n));
    buildLinear(g, a, b, u, n, out + offset, composeText(text, std::nullopt, "", st), textShift, st);
    return g;
}

DimensionGeometry labelledDiameterGeometry(Point2 center, double radius, double angleDeg, const std::string& text,
                                           const AnnotationStyle& st) {
    DimensionGeometry g;
    buildCircleDim(g, center, radius, true, angleDeg, composeText(text, std::nullopt, "", st), st);
    return g;
}

// --- Вычисление размера ---

Result<DimensionResult> evaluateDimension(const DimensionDef& def, const ModelBinding& m, const SheetView& sv,
                                          const AnnotationStyle& st) {
    if (auto s = validateDef(def); !s) return s.error();
    if (!(sv.scale > 0) || !std::isfinite(sv.scale)) return invalid("dimension: view scale must be positive");
    const ProjectedView& v = sv.view;
    DimensionResult res;
    auto fail = [&](const Error& e) -> Result<DimensionResult> {
        if (e.code == ErrorCode::InvalidInput) return e;
        res.state = DimensionState::Dangling;
        res.reason = e.message;
        res.geometry = {};
        return res;
    };

    // Надпись: значение, знак, допуск, ручное переопределение.
    auto makeText = [&](const std::string& symbol, double value, bool angle) -> Result<TextBlock> {
        std::string number = angle ? formatAngle(value) : formatNumber(value, def.decimals);
        std::string main = def.prefix + symbol + number;
        std::optional<ToleranceText> tol;
        if (def.tolerance) {
            auto dev = limitDeviations(value, *def.tolerance);
            if (!dev) return dev.error();
            res.deviations = dev.value();
            tol = toleranceText(def.prefix + symbol + number, *def.tolerance, dev.value(), def.toleranceDisplay,
                                st.omitZeroDeviation);
            main = tol->main;
        }
        if (def.textOverride) {
            const bool keep = def.textOverride->find("<>") != std::string::npos;
            const std::string replaced = substitute(*def.textOverride, keep ? main : std::string());
            res.overridden = !keep;
            if (tol) {
                tol->main = keep ? replaced : *def.textOverride;
                if (!keep) tol.reset();
            }
            main = keep ? replaced : *def.textOverride;
        }
        res.text = main;
        return composeText(main, tol, def.suffix, st);
    };

    const double s = sv.scale;
    switch (def.type) {
        case DimensionType::Horizontal:
        case DimensionType::Vertical:
        case DimensionType::Aligned: {
            auto pa = anchorPoint(def.anchors[0], m, v);
            if (!pa) return fail(pa.error());
            auto pb = anchorPoint(def.anchors[1], m, v);
            if (!pb) return fail(pb.error());
            Point2 u, n;
            if (def.type == DimensionType::Horizontal) {
                u = {1, 0};
                n = {0, 1};
                res.value = std::abs(pb.value().x - pa.value().x);
            } else if (def.type == DimensionType::Vertical) {
                u = {0, 1};
                n = {-1, 0};
                res.value = std::abs(pb.value().y - pa.value().y);
            } else {
                const Point2 d = sub(pb.value(), pa.value());
                res.value = norm(d);
                if (res.value < 1e-9) return fail(dangling("aligned dimension: anchors coincide"));
                u = unit(d);
                n = perp(u);
            }
            const Point2 a = sv.toSheet(pa.value()), b = sv.toSheet(pb.value());
            std::vector<Point2> ref{a, b};
            for (const Anchor& o : def.offsetFrom) {
                auto p = anchorPoint(o, m, v);
                if (!p) return fail(p.error());
                ref.push_back(sv.toSheet(p.value()));
            }
            double out = def.offset >= 0 ? -1e300 : 1e300;
            for (Point2 p : ref) out = def.offset >= 0 ? std::max(out, dot2(p, n)) : std::min(out, dot2(p, n));
            auto text = makeText("", res.value, false);
            if (!text) return text.error();
            buildLinear(res.geometry, a, b, u, n, out + def.offset, text.value(), def.textShift, st);
            break;
        }
        case DimensionType::Radius:
        case DimensionType::Diameter: {
            const bool dia = def.type == DimensionType::Diameter;
            const ModelRef& r = def.anchors[0].ref;
            if (auto c = checkIndex(r, m); !c) return fail(c.error());
            bool side = false;
            if (r.kind == RefKind::Face) {
                BRepAdaptor_Surface surf(m.topology().faces()[r.index].face, false);
                if (surf.GetType() != GeomAbs_Cylinder) return fail(dangling(refLabel(r) + " is not a cylinder"));
                side = !parallel(toVec(surf.Cylinder().Axis().Direction()), v.frame.toward);
            }
            if (side) {
                if (!dia) return fail(dangling(refLabel(r) + ": a radius of a cylinder seen from the side is not dimensioned (use a diameter)"));
                auto sc = sideCylinder(r, m, v);
                if (!sc) return fail(sc.error());
                res.value = sc.value().diameter;
                auto text = makeText("⌀", res.value, false);
                if (!text) return text.error();
                // Выносные линии продолжают очерковые линии за их конец со стороны знака offset.
                Point2 ld = sc.value().dir;
                const LineSeg& l1 = sc.value().first;
                if (def.offset < 0) ld = mul(-1.0, ld);
                const Point2 endA = dot2(l1.a, ld) >= dot2(l1.b, ld) ? l1.a : l1.b;
                const Point2 nrm = perp(sc.value().dir);
                const double off = dot2(sc.value().second.a, nrm) - dot2(endA, nrm);
                const Point2 endB = add(endA, mul(off, nrm));
                const Point2 a = sv.toSheet(endA), b = sv.toSheet(endB);
                const Point2 ldS = ld;  // масштаб равномерный — направление то же
                const double out = std::max({dot2(a, ldS), dot2(b, ldS), dot2(sv.toSheet(sc.value().second.a), ldS),
                                             dot2(sv.toSheet(sc.value().second.b), ldS)});
                const Point2 u = unit(sub(b, a));
                buildLinear(res.geometry, a, b, u, ldS, out + std::abs(def.offset), text.value(), def.textShift, st);
                break;
            }
            auto c = circleOf(r, m, v);
            if (!c) return fail(c.error());
            res.value = dia ? 2.0 * c.value().radius : c.value().radius;
            auto text = makeText(dia ? "⌀" : "R", res.value, false);
            if (!text) return text.error();
            buildCircleDim(res.geometry, sv.toSheet(c.value().center), s * c.value().radius, dia, def.leaderAngleDeg,
                           text.value(), st);
            break;
        }
        case DimensionType::Angle: {
            auto l1 = lineOf(def.anchors[0].ref, m, v);
            if (!l1) return fail(l1.error());
            auto l2 = lineOf(def.anchors[1].ref, m, v);
            if (!l2) return fail(l2.error());
            const Point2 d1 = l1.value().dir, d2 = l2.value().dir;
            const double cr = cross2(d1, d2);
            if (std::abs(cr) < 1e-9) return fail(dangling("angle dimension: the lines are parallel"));
            // Пересечение прямых (координаты листа).
            const Point2 o1 = sv.toSheet(l1.value().origin), o2 = sv.toSheet(l2.value().origin);
            const double t = cross2(sub(o2, o1), d2) / cross2(d1, d2);
            const Point2 O = add(o1, mul(t, d1));
            struct Ray {
                Point2 v;
                double smin, smax;
            };
            auto ray = [&](const LineOnView& l, Point2 d) {
                double mn = 1e300, mx = -1e300;
                for (Point2 p : l.points) {
                    const double sp = dot2(sub(sv.toSheet(p), O), d);
                    mn = std::min(mn, sp);
                    mx = std::max(mx, sp);
                }
                if (std::abs(mn) > std::abs(mx)) return Ray{mul(-1.0, d), -mx, -mn};
                return Ray{d, mn, mx};
            };
            const Ray r1 = ray(l1.value(), d1), r2 = ray(l2.value(), d2);
            res.value = std::acos(std::clamp(dot2(r1.v, r2.v), -1.0, 1.0)) * 180.0 / kPi;
            auto text = makeText("", res.value, true);
            if (!text) return text.error();
            const double R = def.offset;
            double a0 = angleDeg(r1.v), a1 = angleDeg(r2.v);
            if (cross2(r1.v, r2.v) < 0) std::swap(a0, a1);
            const double sweep = std::fmod(a1 - a0 + 720.0, 360.0);
            res.geometry.dimensionArcs.push_back(Arc{O, R, a0, a1});
            for (const Ray& r : {r1, r2}) {
                if (R > r.smax) {
                    res.geometry.extensionLines.push_back(
                        {add(O, mul(r.smax, r.v)), add(O, mul(R + st.extensionOvershoot, r.v))});
                } else if (R < r.smin && R - st.extensionOvershoot > 0) {
                    res.geometry.extensionLines.push_back(
                        {add(O, mul(r.smin, r.v)), add(O, mul(R - st.extensionOvershoot, r.v))});
                }
            }
            auto at = [&](double deg) { return Point2{std::cos(deg * kPi / 180.0), std::sin(deg * kPi / 180.0)}; };
            addArrow(res.geometry, add(O, mul(R, at(a0))), mul(-1.0, perp(at(a0))), st);
            addArrow(res.geometry, add(O, mul(R, at(a1))), perp(at(a1)), st);
            const double am = a0 + sweep / 2.0;
            const Point2 w = at(am);
            const Point2 rr = readable(perp(w));
            const Point2 up = perp(rr);
            if (dot2(up, w) > 0) {
                placeBlock(res.geometry, text.value(), add(O, mul(R, w)), rr, up, st);
            } else {
                // Строка перевёрнута к центру: основание дальше от дуги на высоту текста.
                placeBlock(res.geometry, text.value(), add(O, mul(R + st.textGap + st.textHeight, w)), rr, up, st,
                           -st.textGap);
            }
            break;
        }
    }
    res.state = DimensionState::Ok;
    return res;
}

Status renderDimension(Drawing& d, const DimensionResult& r, const AnnotationStyle& st) {
    if (r.state != DimensionState::Ok) return invalid("dimension is dangling: " + r.reason);
    if (auto s = ensureLayer(d, st.layer, st.layerColor); !s) return s;
    const auto& g = r.geometry;
    for (const LineSeg& l : g.extensionLines) {
        if (auto s = d.add(Entity{l, st.layer, LineType::SolidThin}); !s) return s;
    }
    for (const LineSeg& l : g.dimensionLines) {
        if (auto s = d.add(Entity{l, st.layer, LineType::SolidThin}); !s) return s;
    }
    for (const Arc& a : g.dimensionArcs) {
        if (auto s = d.add(Entity{a, st.layer, LineType::SolidThin}); !s) return s;
    }
    for (const SolidFill& f : g.arrows) {
        if (auto s = d.add(Entity{f, st.layer, LineType::SolidThin}); !s) return s;
    }
    for (const DimensionLabel& l : g.labels) {
        Text t;
        t.position = l.position;
        t.utf8 = l.text;
        t.height = l.height;
        t.rotationDeg = l.rotationDeg;
        t.slanted = st.slantedText;
        t.hAlign = l.hAlign;
        t.vAlign = VAlign::Baseline;
        if (auto s = d.add(Entity{t, st.layer, LineType::SolidThin}); !s) return s;
    }
    return okStatus();
}

// --- Ассоциативность ---

RebindResult rebindRef(ModelRef& ref, const ModelBinding& m) {
    const auto res = modeling::resolveReference(ref.persistent, m.named());
    RebindResult out;
    out.method = res.method;
    out.detail = res.detail;
    if (res.method == modeling::ResolveMethod::Broken) return out;
    if (res.indices.size() != 1) {
        out.method = modeling::ResolveMethod::Broken;
        out.detail = refLabel(ref) + " resolved to " + std::to_string(res.indices.size()) + " elements";
        return out;
    }
    ref.index = std::uint32_t(res.indices[0]);
    auto tr = modeling::makeReference(m.named(), m.named().element(elementKind(ref.kind), res.indices[0]));
    if (tr) {
        // Ссылка «следует» за элементом: имя и сигнатура — по новой форме.
        ref.persistent = std::move(tr).value();
        if (!m.hasHistoryNames()) ref.persistent.name.clear();
    }
    out.ok = true;
    return out;
}

std::size_t DimensionSet::add(DimensionDef def) {
    defs_.push_back(std::move(def));
    results_.emplace_back();
    return defs_.size() - 1;
}

Status DimensionSet::evaluate(const ModelBinding& m, const SheetView& v, const AnnotationStyle& st) {
    results_.assign(defs_.size(), DimensionResult{});
    for (std::size_t i = 0; i < defs_.size(); ++i) {
        auto r = evaluateDimension(defs_[i], m, v, st);
        if (!r) return r.error();
        results_[i] = std::move(r).value();
    }
    return okStatus();
}

Result<std::vector<DimensionUpdate>> DimensionSet::update(const ModelBinding& m, const SheetView& v,
                                                          const AnnotationStyle& st) {
    std::vector<DimensionUpdate> report(defs_.size());
    for (std::size_t i = 0; i < defs_.size(); ++i) {
        DimensionUpdate& u = report[i];
        if (results_[i].state == DimensionState::Ok) u.oldValue = results_[i].value;
        DimensionDef& d = defs_[i];
        std::string notes;
        bool broken = false;
        for (std::vector<Anchor>* list : {&d.anchors, &d.offsetFrom}) {
            for (Anchor& a : *list) {
                const std::string before = refLabel(a.ref);
                const RebindResult rb = rebindRef(a.ref, m);
                if (!rb.ok) {
                    broken = true;
                    notes += (notes.empty() ? "" : "; ") + before + ": " + rb.detail;
                } else if (rb.method != modeling::ResolveMethod::Exact) {
                    notes += (notes.empty() ? "" : "; ") + before + " re-resolved by " +
                             modeling::toString(rb.method) + (rb.detail.empty() ? "" : " (" + rb.detail + ")");
                }
            }
        }
        if (broken) {
            results_[i] = DimensionResult{};
            results_[i].state = DimensionState::Dangling;
            results_[i].reason = notes;
        } else {
            auto r = evaluateDimension(d, m, v, st);
            if (!r) return r.error();
            results_[i] = std::move(r).value();
            if (results_[i].state == DimensionState::Ok) results_[i].reason = notes;
        }
        u.state = results_[i].state;
        u.newValue = results_[i].value;
        u.reason = results_[i].reason;
    }
    return report;
}

Result<std::size_t> DimensionSet::render(Drawing& drawing, const AnnotationStyle& st) const {
    std::size_t n = 0;
    for (const DimensionResult& r : results_) {
        if (r.state != DimensionState::Ok) continue;
        if (auto s = renderDimension(drawing, r, st); !s) return s.error();
        ++n;
    }
    return n;
}

Result<std::vector<DimensionDef>> chainDimensions(DimensionType type, const std::vector<Anchor>& points,
                                                  const ModelBinding& m, const SheetView& v, double offset) {
    if (type != DimensionType::Horizontal && type != DimensionType::Vertical) {
        return invalid("chain dimensions: type must be Horizontal or Vertical");
    }
    if (points.size() < 2) return invalid("chain dimensions: need at least 2 points");
    std::vector<std::pair<double, Anchor>> sorted;
    for (const Anchor& a : points) {
        auto p = anchorPoint(a, m, v.view);
        if (!p) return p.error();
        sorted.emplace_back(type == DimensionType::Horizontal ? p.value().x : p.value().y, a);
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    std::vector<DimensionDef> out;
    for (std::size_t i = 0; i + 1 < sorted.size(); ++i) {
        DimensionDef d;
        d.type = type;
        d.anchors = {sorted[i].second, sorted[i + 1].second};
        d.offset = offset;
        d.offsetFrom = points;
        out.push_back(std::move(d));
    }
    return out;
}

Result<std::vector<DimensionDef>> baselineDimensions(DimensionType type, const Anchor& base,
                                                     const std::vector<Anchor>& points, const ModelBinding& m,
                                                     const SheetView& v, double firstOffset, double spacing) {
    if (type != DimensionType::Horizontal && type != DimensionType::Vertical) {
        return invalid("baseline dimensions: type must be Horizontal or Vertical");
    }
    if (points.empty()) return invalid("baseline dimensions: no points");
    if (!(spacing > 0)) return invalid("baseline dimensions: spacing must be positive");
    auto pb = anchorPoint(base, m, v.view);
    if (!pb) return pb.error();
    const double b = type == DimensionType::Horizontal ? pb.value().x : pb.value().y;
    std::vector<std::pair<double, Anchor>> sorted;
    for (const Anchor& a : points) {
        auto p = anchorPoint(a, m, v.view);
        if (!p) return p.error();
        sorted.emplace_back(std::abs((type == DimensionType::Horizontal ? p.value().x : p.value().y) - b), a);
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    std::vector<Anchor> all{base};
    all.insert(all.end(), points.begin(), points.end());
    const double sign = firstOffset >= 0 ? 1.0 : -1.0;
    std::vector<DimensionDef> out;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        DimensionDef d;
        d.type = type;
        d.anchors = {base, sorted[i].second};
        d.offset = sign * (std::abs(firstOffset) + double(i) * spacing);
        d.offsetFrom = all;
        out.push_back(std::move(d));
    }
    return out;
}

Result<DimensionDef> moveDimensionTo(const DimensionDef& def, const ModelBinding& model, const SheetView& view,
                                     Point2 target, const AnnotationStyle& style) {
    using R = Result<DimensionDef>;
    if (!std::isfinite(target.x) || !std::isfinite(target.y)) return R::failure(ErrorCode::InvalidInput, "move dimension: target is not finite");
    auto labelOf = [&](const DimensionDef& d) -> Result<Point2> {
        auto r = evaluateDimension(d, model, view, style);
        if (!r) return r.error();
        if (r.value().state != DimensionState::Ok)
            return Result<Point2>::failure(ErrorCode::InvalidInput, "move dimension: the dimension is dangling: " + r.value().reason);
        if (r.value().geometry.labels.empty()) return Result<Point2>::failure(ErrorCode::InvalidInput, "move dimension: no label");
        return r.value().geometry.labels.front().position;
    };
    auto l0 = labelOf(def);
    if (!l0) return l0.error();
    const Point2 L0 = l0.value();
    DimensionDef out = def;
    if (def.type == DimensionType::Radius || def.type == DimensionType::Diameter) {
        DimensionDef turned = def;
        turned.leaderAngleDeg += 15.0;
        auto lt = labelOf(turned);
        if (!lt) return lt.error();
        if (std::hypot(lt.value().x - L0.x, lt.value().y - L0.y) > 1e-6) {
            // По окружности: направление от центра к target.
            if (def.anchors.empty()) return R::failure(ErrorCode::InvalidInput, "move dimension: no anchor");
            auto c = anchorPoint(def.anchors.front(), model, view.view);
            if (!c) return c.error();
            const Point2 cs = view.toSheet(c.value());
            if (std::hypot(target.x - cs.x, target.y - cs.y) < 1e-9) return out;
            out.leaderAngleDeg = std::atan2(target.y - cs.y, target.x - cs.x) * 180.0 / std::numbers::pi;
            return out;
        }
    }
    DimensionDef dOff = def, dShift = def;
    dOff.offset += 1.0;
    dShift.textShift += 1.0;
    auto lo = labelOf(dOff);
    if (!lo) return lo.error();
    auto ls = labelOf(dShift);
    if (!ls) return ls.error();
    const Point2 n{lo.value().x - L0.x, lo.value().y - L0.y}, t{ls.value().x - L0.x, ls.value().y - L0.y};
    const Point2 d{target.x - L0.x, target.y - L0.y};
    const double det = n.x * t.y - n.y * t.x;
    double a = 0, b = 0;
    if (std::abs(det) > 1e-9) {
        a = (d.x * t.y - d.y * t.x) / det;
        b = (n.x * d.y - n.y * d.x) / det;
    } else if (n.x * n.x + n.y * n.y > 1e-18) {
        a = (d.x * n.x + d.y * n.y) / (n.x * n.x + n.y * n.y);
    }
    out.offset = def.offset + a;
    out.textShift = def.textShift + b;
    if (def.offset != 0.0 && (out.offset > 0) != (def.offset > 0)) out.offset = std::copysign(2.0, def.offset);
    return out;
}

}  // namespace bendcore::drawing_v2::annotations
