#include "test_drawing_views_support.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include <cmath>
#include <sstream>

namespace dvtest {

bendcore::kernel::Shape blockWithHole(const BlockSpec& s) {
    TopoDS_Shape box = BRepPrimAPI_MakeBox(s.L, s.W, s.H).Shape();
    gp_Ax2 ax(gp_Pnt(s.cx, s.cy, -1.0), gp_Dir(0, 0, 1));
    TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(ax, s.d / 2, s.H + 2.0).Shape();
    return bendcore::kernel::Shape(BRepAlgoAPI_Cut(box, cyl).Shape());
}

std::vector<LineSeg> segments(const ProjectedView& v, Visibility vis) {
    std::vector<LineSeg> out;
    for (const ViewEdge& e : v.edges)
        if (e.visibility == vis)
            if (auto* l = std::get_if<LineSeg>(&e.curve)) out.push_back(*l);
    return out;
}

bool hasSegment(const std::vector<LineSeg>& segs, Point2 a, Point2 b, double tol) {
    auto near = [tol](Point2 p, Point2 q) { return std::hypot(p.x - q.x, p.y - q.y) <= tol; };
    for (const LineSeg& s : segs)
        if ((near(s.a, a) && near(s.b, b)) || (near(s.a, b) && near(s.b, a))) return true;
    return false;
}

double polygonArea(const std::vector<Point2>& p) {
    double s = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        const Point2& a = p[i];
        const Point2& b = p[(i + 1) % p.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

std::string describe(const ProjectedView& v) {
    std::ostringstream os;
    for (const ViewEdge& e : v.edges) {
        os << (e.visibility == Visibility::Visible ? "V " : "H ") << int(e.cls) << " e"
           << (e.edgeIndex ? int(*e.edgeIndex) : -1) << " f" << (e.faceIndex ? int(*e.faceIndex) : -1)
           << " ";
        if (auto* l = std::get_if<LineSeg>(&e.curve))
            os << "L (" << l->a.x << "," << l->a.y << ")-(" << l->b.x << "," << l->b.y << ")";
        else if (auto* a = std::get_if<Arc>(&e.curve))
            os << "A c(" << a->center.x << "," << a->center.y << ") r" << a->radius << " "
               << a->startDeg << ".." << a->endDeg;
        else if (auto* c = std::get_if<Circle>(&e.curve))
            os << "C c(" << c->center.x << "," << c->center.y << ") r" << c->radius;
        else
            os << "P n" << std::get<Polyline>(e.curve).points.size();
        os << "\n";
    }
    return os.str();
}

}  // namespace dvtest
