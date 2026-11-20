// Условное упрощённое изображение подшипников качения на сборочных чертежах (ГОСТ 2.420-69,
// ISO 8826-1). Документация — docs/drawing/VIEWS.md, «Подшипники качения».

#include <bendcore/drawing_v2/bearings.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Cylinder.hxx>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace bendcore::drawing_v2::annotations {

namespace {

constexpr double kPi = 3.14159265358979323846;

Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 mul(double k, Point2 a) { return {k * a.x, k * a.y}; }

// Строчные буквы (ASCII и кириллица UTF-8 А–Я).
std::string lower(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(char(std::tolower(c)));
            continue;
        }
        if (c == 0xD0 && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[++i]);
            if (d >= 0x90 && d <= 0x9F) {
                out += {char(0xD0), char(d + 0x20)};
            } else if (d >= 0xA0 && d <= 0xAF) {
                out += {char(0xD1), char(d - 0x20)};
            } else {
                out += {char(c), char(d)};
            }
            continue;
        }
        out.push_back(char(c));
    }
    return out;
}

bool startsWithWord(const std::string& text, const std::string& word) {
    const std::string t = lower(text), w = lower(word);
    if (t.compare(0, w.size(), w) != 0) return false;
    if (t.size() == w.size()) return true;
    const unsigned char next = static_cast<unsigned char>(t[w.size()]);
    return !(std::isalpha(next) || next >= 0x80);
}

double segDist(Point2 p, Point2 a, Point2 b) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

}  // namespace

Result<BearingInfo> bearingGeometry(const TopoDS_Shape& part, double tol) {
    if (part.IsNull()) return Error{ErrorCode::InvalidInput, "bearing: empty shape"};
    try {
        struct Cyl {
            gp_Ax1 axis;
            double r, area;
        };
        std::vector<Cyl> cyls;
        std::vector<gp_Pln> planes;
        bool rolling = false;
        for (TopExp_Explorer ex(part, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face f = TopoDS::Face(ex.Current());
            BRepAdaptor_Surface s(f, true);
            switch (s.GetType()) {
                case GeomAbs_Cylinder: {
                    GProp_GProps g;
                    BRepGProp::SurfaceProperties(f, g);
                    cyls.push_back({s.Cylinder().Axis(), s.Cylinder().Radius(), g.Mass()});
                    break;
                }
                case GeomAbs_Plane: planes.push_back(s.Plane()); break;
                case GeomAbs_Sphere:
                case GeomAbs_Torus: rolling = true; break;
                default: break;
            }
        }
        if (cyls.empty()) return Error{ErrorCode::InvalidInput, "bearing: the part has no cylindrical faces"};
        const Cyl& main = *std::max_element(cyls.begin(), cyls.end(), [](const Cyl& a, const Cyl& b) { return a.area < b.area; });
        const gp_Dir a = main.axis.Direction();
        const gp_Pnt o = main.axis.Location();
        double rIn = std::numeric_limits<double>::max(), rOut = 0.0;
        for (const Cyl& c : cyls) {
            if (!c.axis.Direction().IsParallel(a, 1e-6)) continue;
            const gp_Vec v(o, c.axis.Location());
            const double off = (v - gp_Vec(a) * v.Dot(gp_Vec(a))).Magnitude();
            if (off > tol + 1e-9 * c.r) continue;
            rIn = std::min(rIn, c.r);
            rOut = std::max(rOut, c.r);
        }
        if (!(rOut - rIn > tol)) return Error{ErrorCode::InvalidInput, "bearing: no inner and outer cylinders on one axis"};
        double s0 = std::numeric_limits<double>::max(), s1 = -s0;
        auto axial = [&](const gp_Pnt& p) { return gp_Vec(o, p).Dot(gp_Vec(a)); };
        for (TopExp_Explorer ex(part, TopAbs_VERTEX); ex.More(); ex.Next()) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(ex.Current()));
            const double s = axial(p);
            const gp_Vec v(o, p);
            const double r = (v - gp_Vec(a) * s).Magnitude();
            if (r < rIn - tol - 1e-9 * rOut || r > rOut + tol + 1e-9 * rOut)
                return Error{ErrorCode::InvalidInput, "bearing: a vertex lies outside the ring d…D"};
            s0 = std::min(s0, s);
            s1 = std::max(s1, s);
        }
        for (const gp_Pln& pl : planes) {
            if (!pl.Axis().Direction().IsParallel(a, 1e-6)) continue;
            const double s = axial(pl.Location());
            s0 = std::min(s0, s);
            s1 = std::max(s1, s);
        }
        if (!(s1 - s0 > tol)) return Error{ErrorCode::InvalidInput, "bearing: zero width"};
        BearingInfo info;
        const gp_Pnt start = o.Translated(gp_Vec(a) * s0);
        info.origin = {start.X(), start.Y(), start.Z()};
        info.axis = {a.X(), a.Y(), a.Z()};
        info.d = 2.0 * rIn;
        info.D = 2.0 * rOut;
        info.B = s1 - s0;
        int solids = 0;
        for (TopExp_Explorer ex(part, TopAbs_SOLID); ex.More(); ex.Next()) ++solids;
        info.rollingElements = rolling && solids >= 3;
        return info;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("bearing: ") + e.what()};
    }
}

bool bearingName(const std::string& name) {
    return startsWithWord(name, "Подшипник") || startsWithWord(name, "Bearing");
}

std::optional<BearingInfo> recognizeBearing(const TopoDS_Shape& part, const std::vector<std::string>& names) {
    auto g = bearingGeometry(part);
    if (!g) return std::nullopt;
    if (g.value().rollingElements) return g.value();
    for (const std::string& n : names)
        if (bearingName(n)) return g.value();
    return std::nullopt;
}

std::vector<BearingImage> bearingImages(const std::vector<BearingInfo>& bearings, const views::ViewFrame& frame,
                                        const views::CuttingPlane& plane, double angularTolerance) {
    std::vector<BearingImage> out;
    const double n = views::length(plane.sight);
    if (!(n > 0)) return out;
    for (std::size_t i = 0; i < bearings.size(); ++i) {
        const BearingInfo& b = bearings[i];
        if (std::abs(views::dot(b.axis, frame.toward)) > angularTolerance) continue;
        const double dist = views::dot(b.origin - plane.point, plane.sight) / n;
        if (std::abs(dist) > 1e-6 || std::abs(views::dot(b.axis, plane.sight)) > angularTolerance * n) continue;
        BearingImage img;
        img.bearing = i;
        img.info = b;
        img.origin = frame.project(b.origin);
        const Point2 ax = frame.project(b.axis);
        const double len = std::hypot(ax.x, ax.y);
        img.axis = {ax.x / len, ax.y / len};
        out.push_back(img);
    }
    return out;
}

std::vector<BearingLine> bearingLines(const BearingImage& img, const views::SheetView& view, const views::RenderOptions& o,
                                      BearingSymbol symbol) {
    std::vector<BearingLine> out;
    const Point2 u = img.axis, w{-img.axis.y, img.axis.x};
    const Point2 c = view.toSheet(img.origin);
    const double k = view.scale, B = img.info.B * k, ri = img.info.d / 2.0 * k, ro = img.info.D / 2.0 * k;
    for (double side : {1.0, -1.0}) {
        auto P = [&](double s, double t) { return add(c, add(mul(s, u), mul(side * t, w))); };
        const Point2 p00 = P(0, ri), p10 = P(B, ri), p11 = P(B, ro), p01 = P(0, ro);
        for (const LineSeg& s : {LineSeg{p00, p10}, LineSeg{p10, p11}, LineSeg{p11, p01}, LineSeg{p01, p00}})
            out.push_back({BearingLine::Role::Contour, s, LineType::SolidThick, o.visibleLayer});
        if (symbol == BearingSymbol::Diagonals) {
            out.push_back({BearingLine::Role::Diagonal, LineSeg{p00, p11}, LineType::SolidThin, o.thinLayer});
            out.push_back({BearingLine::Role::Diagonal, LineSeg{p10, p01}, LineType::SolidThin, o.thinLayer});
        } else {
            const double dw = std::min(0.3 * (img.info.D - img.info.d), 0.9 * img.info.B) * k;
            out.push_back({BearingLine::Role::RollingElement, Circle{P(B / 2.0, (ri + ro) / 2.0), dw / 2.0},
                           LineType::SolidThick, o.visibleLayer});
        }
    }
    return out;
}

Status renderBearings(Drawing& drawing, const views::SheetView& view, const std::vector<BearingImage>& images,
                      const views::RenderOptions& options, BearingSymbol symbol) {
    if (auto st = views::ensureViewLayers(drawing, options); !st) return st;
    for (const BearingImage& img : images)
        for (const BearingLine& l : bearingLines(img, view, options, symbol)) {
            Geometry g = std::holds_alternative<LineSeg>(l.curve) ? Geometry(std::get<LineSeg>(l.curve))
                                                                  : Geometry(std::get<Circle>(l.curve));
            if (auto st = drawing.add(Entity{g, l.layer, l.lineType}); !st) return st;
        }
    return okStatus();
}

Point2 edgeMidpoint(const views::Curve2& curve) {
    if (const auto* l = std::get_if<LineSeg>(&curve)) return {0.5 * (l->a.x + l->b.x), 0.5 * (l->a.y + l->b.y)};
    if (const auto* a = std::get_if<Arc>(&curve)) {
        double sweep = a->endDeg - a->startDeg;
        while (sweep <= 0) sweep += 360.0;
        const double m = (a->startDeg + sweep / 2.0) * kPi / 180.0;
        return {a->center.x + a->radius * std::cos(m), a->center.y + a->radius * std::sin(m)};
    }
    if (const auto* c = std::get_if<Circle>(&curve)) return {c->center.x + c->radius, c->center.y};
    const auto& p = std::get<Polyline>(curve).points;
    if (p.empty()) return {};
    if (p.size() == 1) return p[0];
    const std::size_t i = (p.size() - 1) / 2;
    return {0.5 * (p[i].x + p[i + 1].x), 0.5 * (p[i].y + p[i + 1].y)};
}

std::optional<int> sectionEdgeOwner(const views::SectionView& section, const views::ViewEdge& edge, double tol) {
    const Point2 m = edgeMidpoint(edge.curve);
    std::optional<int> best;
    double bestD = tol;
    for (std::size_t i = 0; i < section.regions.size() && i < section.regionOwner.size(); ++i)
        for (const auto& loop : section.regions[i])
            for (std::size_t k = 0; k < loop.size(); ++k) {
                const double d = segDist(m, loop[k], loop[(k + 1) % loop.size()]);
                if (d <= bestD) {
                    bestD = d;
                    best = section.regionOwner[i];
                }
            }
    return best;
}

views::SectionView withConventionalBearings(const views::SectionView& section, const std::vector<BearingImage>& images,
                                            const EdgeChildOf& childOf) {
    if (images.empty()) return section;
    std::vector<std::size_t> children;
    for (const BearingImage& b : images) children.push_back(b.info.child);
    auto isBearing = [&](std::size_t c) { return std::find(children.begin(), children.end(), c) != children.end(); };
    views::SectionView out = section;
    out.regions.clear();
    out.regionOwner.clear();
    for (std::size_t i = 0; i < section.regions.size(); ++i) {
        const int owner = i < section.regionOwner.size() ? section.regionOwner[i] : 0;
        if (owner >= 0 && isBearing(std::size_t(owner))) continue;
        out.regions.push_back(section.regions[i]);
        out.regionOwner.push_back(owner);
    }
    std::erase_if(out.view.edges, [&](const views::ViewEdge& e) {
        const auto c = childOf(e);
        return c && isBearing(*c);
    });
    return out;
}

std::optional<Point2> regionInteriorPoint(const std::vector<std::vector<Point2>>& region) {
    double y0 = std::numeric_limits<double>::max(), y1 = -y0;
    for (const auto& loop : region)
        for (const Point2& p : loop) {
            y0 = std::min(y0, p.y);
            y1 = std::max(y1, p.y);
        }
    if (!(y1 > y0)) return std::nullopt;
    std::optional<Point2> best;
    double bestLen = 0.0;
    for (int k = 1; k <= 7; ++k) {
        const double y = y0 + (y1 - y0) * k / 8.0;
        std::vector<double> xs;
        for (const auto& loop : region)
            for (std::size_t i = 0; i < loop.size(); ++i) {
                const Point2 a = loop[i], b = loop[(i + 1) % loop.size()];
                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) xs.push_back(a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y));
            }
        std::sort(xs.begin(), xs.end());
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double len = xs[i + 1] - xs[i];
            if (len > bestLen) {
                bestLen = len;
                best = Point2{0.5 * (xs[i] + xs[i + 1]), y};
            }
        }
    }
    return best;
}

}  // namespace bendcore::drawing_v2::annotations
