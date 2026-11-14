// Поиск отверстий детали и постановка пакета крепежа (docs/assembly/FASTENERS.md, раздел 5).

#include <bendcore/assembly_v2/fasteners.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <set>

namespace bendcore::assembly_v2::fasteners {

using kernel::ErrorCode;

namespace {

template <typename T>
kernel::Result<T> failure(ErrorCode code, std::string message) {
    return kernel::Result<T>::failure(code, std::move(message));
}

// Плоскость или ось элемента детали по имени роли крепежа (СК детали).
kernel::Result<Entity> namedEntity(const Definition& def, const std::string& role, EntityUse use) {
    if (def.kind != DefinitionKind::Part || !def.topology) {
        return failure<Entity>(ErrorCode::InvalidInput, "definition '" + def.name + "' is not a part with geometry");
    }
    if (!def.elementMap) {
        return failure<Entity>(ErrorCode::InvalidInput,
                               "definition '" + def.name + "' has no element map (not a fastener part)");
    }
    const std::string name = faceName(role);
    auto idx = def.elementMap->find(ElementKind::Face, name);
    if (!idx) return failure<Entity>(ErrorCode::InvalidInput, "definition '" + def.name + "' has no face " + name);
    return entityFromIndex(*def.topology, ElementKind::Face, static_cast<std::uint32_t>(*idx), use);
}

GeometryRef named(OccurrenceId occ, const std::string& role) {
    return GeometryRef{{occ}, ElementRef::named(ElementKind::Face, faceName(role)), EntityUse::Auto};
}

// Точка пересечения прямой (p, d) с плоскостью (q, n).
Vec3 linePlane(const Vec3& p, const Vec3& d, const Vec3& q, const Vec3& n) {
    return p + d * ((q - p).dot(n) / d.dot(n));
}

struct Placed {
    OccurrenceId occurrence = 0;
    Vec3 top = Vec3::Zero();  // точка оси на противоположном торце (шайба), мировая СК
};

// Ставит деталь def опорной поверхностью Bearing на плоскость (point, outward) так, что её ось
// (axisRole) совпадает с осью отверстия; добавляет соосность и совпадение плоскостей.
kernel::Result<Placed> placeOn(AssemblyDocument& doc, DefinitionId defId, const std::string& axisRole,
                               bool needTop, const Vec3& point, const Vec3& outward, const GeometryRef& hole,
                               const GeometryRef& support, std::vector<MateId>& mates) {
    if (!doc.hasDefinition(defId)) {
        return failure<Placed>(ErrorCode::InvalidInput, "unknown definition " + std::to_string(defId));
    }
    const Definition& def = doc.definition(defId);
    auto bearing = namedEntity(def, roles::kBearing, EntityUse::Auto);
    if (!bearing) return bearing.error();
    auto axis = namedEntity(def, axisRole, EntityUse::Axis);
    if (!axis) return axis.error();
    const Entity& b = bearing.value();
    const Entity& a = axis.value();
    if (b.type != EntityType::Plane || std::abs(std::abs(a.direction.dot(b.direction)) - 1) > 1e-9) {
        return failure<Placed>(ErrorCode::InvalidInput,
                               "definition '" + def.name + "': bearing face is not perpendicular to its axis");
    }
    const Vec3 c = linePlane(a.point, a.direction, b.point, b.direction);
    Transform t;
    t.rotation = Quat::FromTwoVectors(b.direction, -outward).normalized();
    t.position = point - t.rotation * c;
    auto occ = doc.addOccurrence(defId, t);
    if (!occ) return occ.error();
    const OccurrenceId id = occ.value();

    auto m1 = doc.addMate(MateSpec::concentric(named(id, axisRole), hole));
    if (!m1) return failure<Placed>(m1.error().code, "concentric mate of '" + def.name + "': " + m1.error().message);
    mates.push_back(m1.value());
    auto m2 = doc.addMate(MateSpec::coincident(named(id, roles::kBearing), support, Alignment::AntiAligned));
    if (!m2) return failure<Placed>(m2.error().code, "bearing mate of '" + def.name + "': " + m2.error().message);
    mates.push_back(m2.value());

    Placed out;
    out.occurrence = id;
    if (needTop) {
        auto top = namedEntity(def, roles::kTop, EntityUse::Auto);
        if (!top) return top.error();
        const Entity tw = transformEntity(top.value(), t);
        out.top = linePlane(point, outward, tw.point, tw.direction);
    }
    return out;
}

// Точка оси экземпляра на плоскости элемента по имени (мировая СК).
kernel::Result<Vec3> axisPointOn(const AssemblyDocument& doc, OccurrenceId occ, const std::string& role,
                                 const Vec3& axisPoint, const Vec3& axisDir) {
    auto e = doc.worldEntity(named(occ, role));
    if (!e) return e.error();
    return linePlane(axisPoint, axisDir, e.value().point, e.value().direction);
}

}  // namespace

kernel::Result<std::vector<HoleSite>> findHoleSites(const AssemblyDocument& document, const OccurrencePath& part,
                                                    const Vec3& insertion, double radius, double tolerance) {
    using R = kernel::Result<std::vector<HoleSite>>;
    if (part.empty()) return R::failure(ErrorCode::InvalidInput, "empty occurrence path");
    if (!(insertion.norm() > 1e-12)) return R::failure(ErrorCode::InvalidInput, "zero insertion direction");
    if (!(radius > 0) || !(tolerance > 0)) {
        return R::failure(ErrorCode::InvalidInput, "hole radius and tolerance must be positive");
    }
    auto world = document.worldPlacement(part);
    if (!world) return world.error();
    const Occurrence* leaf = document.findOccurrence(part.back());
    if (!leaf) return R::failure(ErrorCode::InvalidInput, "unknown occurrence " + std::to_string(part.back()));
    const Definition& def = document.definition(leaf->definition);
    if (def.kind != DefinitionKind::Part || !def.topology) {
        return R::failure(ErrorCode::InvalidInput, "occurrence " + std::to_string(part.back()) + " is not a part");
    }
    const kernel::TopologyIndex& topo = *def.topology;
    const Transform& W = world.value();
    const Vec3 ins = insertion.normalized();
    const double rtol = tolerance * std::max(1.0, radius);

    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> edgeFaces;
    TopExp::MapShapesAndAncestors(def.shape, TopAbs_EDGE, TopAbs_FACE, edgeFaces);

    struct Group {
        std::vector<std::uint32_t> faces;
        Entity axis;  // мировая СК
    };
    std::vector<Group> groups;
    const auto nFaces = static_cast<std::uint32_t>(elementCount(topo, ElementKind::Face));
    for (std::uint32_t i = 0; i < nFaces; ++i) {
        auto e = entityFromIndex(topo, ElementKind::Face, i, EntityUse::Auto);
        if (!e || e.value().type != EntityType::Cylinder) continue;
        if (std::abs(e.value().radius - radius) > rtol) continue;
        const Entity we = transformEntity(e.value(), W);
        if (we.direction.cross(ins).norm() > tolerance) continue;
        // Отверстие: наружная нормаль грани направлена к оси.
        const TopoDS_Face face = TopoDS::Face(elementAt(topo, ElementKind::Face, i));
        BRepAdaptor_Surface s(face, true);
        double u0, u1, v0, v1;
        BRepTools::UVBounds(face, u0, u1, v0, v1);
        gp_Pnt p;
        gp_Vec du, dv;
        s.D1((u0 + u1) / 2, (v0 + v1) / 2, p, du, dv);
        gp_Vec n = du.Crossed(dv);
        if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
        const Vec3 pl(p.X(), p.Y(), p.Z());
        const Vec3 ap = e.value().point, ad = e.value().direction;
        Vec3 radial = (pl - ap) - ad * (pl - ap).dot(ad);
        if (Vec3(n.X(), n.Y(), n.Z()).dot(radial) >= 0) continue;  // вал, а не отверстие
        bool merged = false;
        for (Group& g : groups) {
            const Vec3 off = we.point - g.axis.point;
            if ((off - g.axis.direction * off.dot(g.axis.direction)).norm() <= rtol) {
                g.faces.push_back(i);
                merged = true;
                break;
            }
        }
        if (!merged) groups.push_back({{i}, we});
    }

    std::vector<HoleSite> sites;
    for (const Group& g : groups) {
        // Смежные плоские грани, перпендикулярные оси: положение вдоль оси и наружная нормаль.
        struct Cap {
            std::uint32_t index;
            double t;
            double dir;  // n·ins
        };
        std::vector<Cap> caps;
        std::set<std::uint32_t> seen(g.faces.begin(), g.faces.end());
        for (std::uint32_t fi : g.faces) {
            const TopoDS_Shape face = elementAt(topo, ElementKind::Face, fi);
            for (TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
                if (!edgeFaces.Contains(ex.Current())) continue;
                for (NCollection_List<TopoDS_Shape>::Iterator it(edgeFaces.FindFromKey(ex.Current())); it.More();
                     it.Next()) {
                    auto idx = topo.faceIndex(it.Value());
                    if (!idx || !seen.insert(*idx).second) continue;
                    auto pe = entityFromIndex(topo, ElementKind::Face, *idx, EntityUse::Auto);
                    if (!pe || pe.value().type != EntityType::Plane) continue;
                    const Entity wp = transformEntity(pe.value(), W);
                    const double dn = wp.direction.dot(ins);
                    if (std::abs(std::abs(dn) - 1) > tolerance) continue;
                    caps.push_back({*idx, (wp.point - g.axis.point).dot(ins), dn});
                }
            }
        }
        if (caps.empty()) continue;
        const auto [mn, mx] = std::minmax_element(caps.begin(), caps.end(),
                                                  [](const Cap& a, const Cap& b) { return a.t < b.t; });
        if (!(mn->dir < 0)) continue;  // вход не с плоскости, обращённой навстречу крепежу
        HoleSite site;
        site.hole = GeometryRef{part, ElementRef::face(g.faces.front()), EntityUse::Auto};
        site.entryFace = GeometryRef{part, ElementRef::face(mn->index), EntityUse::Auto};
        site.radius = radius;
        if (mx->dir > 0 && mx->t > mn->t) {
            site.exitFace = GeometryRef{part, ElementRef::face(mx->index), EntityUse::Auto};
            site.depth = mx->t - mn->t;
        }
        sites.push_back(site);
    }
    if (sites.empty()) {
        return R::failure(ErrorCode::InvalidInput, "no hole of radius " + std::to_string(radius) +
                                                       " with an entry face along the insertion direction");
    }
    // Порядок — по точке входа.
    std::vector<std::pair<Vec3, HoleSite>> keyed;
    for (HoleSite& s : sites) {
        auto h = document.worldEntity(s.hole);
        auto en = document.worldEntity(s.entryFace);
        if (!h) return h.error();
        if (!en) return en.error();
        keyed.emplace_back(linePlane(h.value().point, h.value().direction, en.value().point, en.value().direction), s);
    }
    std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) {
        const double eps = 1e-9;
        for (int k = 0; k < 3; ++k) {
            if (std::abs(a.first[k] - b.first[k]) > eps) return a.first[k] < b.first[k];
        }
        return false;
    });
    std::vector<HoleSite> out;
    for (auto& [p, s] : keyed) out.push_back(std::move(s));
    return out;
}

kernel::Result<std::vector<StackPlacement>> insertFastenerStacks(AssemblyDocument& document,
                                                                 const FastenerStack& stack,
                                                                 const std::vector<HoleSite>& sites) {
    using R = kernel::Result<std::vector<StackPlacement>>;
    AssemblyDocument work = document;
    std::vector<StackPlacement> result;
    for (std::size_t si = 0; si < sites.size(); ++si) {
        const HoleSite& site = sites[si];
        const std::string where = "hole " + std::to_string(si + 1) + ": ";
        auto hole = work.worldEntity(site.hole);
        if (!hole) return R::failure(hole.error().code, where + hole.error().message);
        if (!hole.value().isLineLike()) return R::failure(ErrorCode::InvalidInput, where + "hole face has no axis");
        auto entry = work.worldEntity(site.entryFace);
        if (!entry) return R::failure(entry.error().code, where + entry.error().message);
        if (entry.value().type != EntityType::Plane) {
            return R::failure(ErrorCode::InvalidInput, where + "entry face is not planar");
        }
        const Vec3 a0 = hole.value().point;
        const Vec3 u = entry.value().direction.normalized();  // наружу из детали, к головке
        if (hole.value().direction.cross(u).norm() > 1e-6) {
            return R::failure(ErrorCode::InvalidInput, where + "hole axis is not perpendicular to the entry face");
        }
        const Vec3 ins = -u;
        StackPlacement sp;

        // Сторона головки: шайбы от детали к головке (в спецификации — от головки к детали).
        Vec3 point = linePlane(a0, u, entry.value().point, u);
        GeometryRef support = site.entryFace;
        for (auto it = stack.headWashers.rbegin(); it != stack.headWashers.rend(); ++it) {
            auto w = placeOn(work, *it, roles::kBore, true, point, u, site.hole, support, sp.mates);
            if (!w) return R::failure(w.error().code, where + w.error().message);
            sp.headWashers.insert(sp.headWashers.begin(), w.value().occurrence);
            point = w.value().top;
            support = named(w.value().occurrence, roles::kTop);
        }
        auto screw = placeOn(work, stack.screw, roles::kShank, false, point, u, site.hole, support, sp.mates);
        if (!screw) return R::failure(screw.error().code, where + screw.error().message);
        sp.screw = screw.value().occurrence;
        const Vec3 bearingPoint = point;
        auto tip = axisPointOn(work, sp.screw, roles::kTip, a0, ins);
        if (!tip) return R::failure(tip.error().code, where + tip.error().message);

        // Сторона гайки.
        Vec3 last = linePlane(a0, u, entry.value().point, u);
        if (site.exitFace) {
            auto exit = work.worldEntity(*site.exitFace);
            if (!exit) return R::failure(exit.error().code, where + exit.error().message);
            const Vec3 w = exit.value().direction.normalized();
            if (exit.value().type != EntityType::Plane || w.cross(ins).norm() > 1e-6 || w.dot(ins) <= 0) {
                return R::failure(ErrorCode::InvalidInput,
                                  where + "exit face is not a plane perpendicular to the hole axis facing the nut");
            }
            Vec3 p = linePlane(a0, ins, exit.value().point, w);
            GeometryRef sup = *site.exitFace;
            for (DefinitionId wd : stack.nutWashers) {
                auto pw = placeOn(work, wd, roles::kBore, true, p, w, site.hole, sup, sp.mates);
                if (!pw) return R::failure(pw.error().code, where + pw.error().message);
                sp.nutWashers.push_back(pw.value().occurrence);
                p = pw.value().top;
                sup = named(pw.value().occurrence, roles::kTop);
            }
            if (stack.nut) {
                auto nut = placeOn(work, *stack.nut, roles::kBore, false, p, w, site.hole, sup, sp.mates);
                if (!nut) return R::failure(nut.error().code, where + nut.error().message);
                sp.nut = nut.value().occurrence;
                auto top = axisPointOn(work, *sp.nut, roles::kTop, a0, ins);
                if (!top) return R::failure(top.error().code, where + top.error().message);
                p = top.value();
            }
            last = p;
        } else if (stack.nut || !stack.nutWashers.empty()) {
            return R::failure(ErrorCode::InvalidInput, where + "blind hole: no exit face for the nut side");
        }
        sp.grip = (last - bearingPoint).dot(ins);
        sp.protrusion = (tip.value() - last).dot(ins);
        if (stack.nut && sp.protrusion < -1e-9) {
            return R::failure(ErrorCode::InvalidInput,
                              where + "screw is too short: grip " + std::to_string(sp.grip) + " mm, shank ends " +
                                  std::to_string(-sp.protrusion) + " mm inside the nut face");
        }
        result.push_back(std::move(sp));
    }
    document = std::move(work);
    return result;
}

}  // namespace bendcore::assembly_v2::fasteners
