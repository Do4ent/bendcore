#include "bendcore/occt/geometry_backend.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <map>
#include <sstream>
#include <vector>

namespace bendcore::occt {
namespace {

constexpr double kTwoPi = 2.0 * M_PI;
constexpr double kRadiusTol = 1e-6;

struct FacePiece {
    enum Kind { Cylinder, Torus } kind;
    const Geom_Surface* surface;  // грани одного участка (разрезанные швом) делят одну поверхность
    double radius;                // радиус трубы на этой грани
    double bend_radius;           // для тора — радиус гиба по оси
    double extent;                // длина (цилиндр) или угол, рад (тор) с учётом доли обхода
};

}  // namespace

geometry::BRepTubeFeatures analyze_brep(const std::string& brep_text) {
    geometry::BRepTubeFeatures out;
    TopoDS_Shape shape;
    try {
        std::istringstream in(brep_text);
        BRep_Builder builder;
        BRepTools::Read(shape, in, builder);
    } catch (const Standard_Failure& e) {
        out.error = std::string("B-rep не читается: ") + e.what();
        return out;
    }
    if (shape.IsNull()) {
        out.error = "B-rep не читается или пуст";
        return out;
    }

    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    if (!box.IsVoid()) {
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        out.bbox_size[0] = x1 - x0;
        out.bbox_size[1] = y1 - y0;
        out.bbox_size[2] = z1 - z0;
    }

    std::vector<FacePiece> pieces;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face face = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface sa(face, true);
        double u1, u2, v1, v2;
        BRepTools::UVBounds(face, u1, u2, v1, v2);
        const Geom_Surface* surf = BRep_Tool::Surface(face).get();
        switch (sa.GetType()) {
            case GeomAbs_Plane:
                break;  // торцы
            case GeomAbs_Cylinder:
                // U — угол вокруг оси, V — длина вдоль оси (мм).
                pieces.push_back({FacePiece::Cylinder, surf, sa.Cylinder().Radius(), 0.0,
                                  (v2 - v1) * (u2 - u1) / kTwoPi});
                break;
            case GeomAbs_Torus:
                // U — угол вокруг главной оси (угол гиба), V — вокруг сечения трубы.
                pieces.push_back({FacePiece::Torus, surf, sa.Torus().MinorRadius(), sa.Torus().MajorRadius(),
                                  (u2 - u1) * (v2 - v1) / kTwoPi});
                break;
            default: {
                std::ostringstream msg;
                msg << "неподдерживаемый тип поверхности " << static_cast<int>(sa.GetType())
                    << " (ожидаются цилиндры, торы и плоские торцы; спиральные участки не поддержаны)";
                out.error = msg.str();
                return out;
            }
        }
    }
    if (pieces.empty()) {
        out.error = "нет цилиндрических и тороидальных граней: форма не похожа на трубу";
        return out;
    }

    for (const auto& p : pieces) out.outer_radius = std::max(out.outer_radius, p.radius);
    for (const auto& p : pieces) {
        if (p.radius < out.outer_radius - kRadiusTol) out.inner_radius = std::max(out.inner_radius, p.radius);
    }

    // Участки по наружной поверхности: доли граней одной поверхности складываются.
    std::map<const Geom_Surface*, FacePiece> segments;
    std::vector<const Geom_Surface*> order;
    for (const auto& p : pieces) {
        if (std::abs(p.radius - out.outer_radius) > kRadiusTol) continue;
        auto [it, inserted] = segments.try_emplace(p.surface, p);
        if (inserted) {
            order.push_back(p.surface);
        } else {
            it->second.extent += p.extent;
        }
    }
    for (const Geom_Surface* key : order) {
        const FacePiece& seg = segments.at(key);
        if (seg.kind == FacePiece::Cylinder) {
            out.straight_lengths.push_back(seg.extent);
        } else {
            out.bends.push_back({seg.extent * 180.0 / M_PI, seg.bend_radius});
        }
    }
    out.ok = true;
    return out;
}

void install_geometry_backend() { geometry::set_geometry_backend(&analyze_brep); }

}  // namespace bendcore::occt
