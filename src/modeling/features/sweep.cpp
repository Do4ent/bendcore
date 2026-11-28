// Кинематика (BRepOffsetAPI_MakePipeShell) и тело по сечениям (BRepOffsetAPI_ThruSections)
// (docs/modeling/FEATURES.md, разделы «Кинематика» и «По сечениям»).

#include "feature_common.hpp"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>
#include <map>

namespace bendcore::modeling {

using namespace detail;

namespace {

// Контуры грани: внешний первым, затем внутренние в порядке обхода грани.
std::vector<TopoDS_Wire> faceWires(const TopoDS_Face& face) {
    std::vector<TopoDS_Wire> out;
    const TopoDS_Wire outer = BRepTools::OuterWire(face);
    out.push_back(outer);
    for (TopExp_Explorer ex(face, TopAbs_WIRE); ex.More(); ex.Next())
        if (!ex.Current().IsSame(outer)) out.push_back(TopoDS::Wire(ex.Current()));
    return out;
}

gp_Pnt centroidOf(const TopoDS_Shape& s) {
    GProp_GProps g;
    BRepGProp::LinearProperties(s, g);
    return g.CentreOfMass();
}

// Записи истории алгоритма заметания для элементов исходного профиля: Generated(ребро) →
// боковые грани, Generated(вершина) → боковые рёбра (образы копий в result).
template <class Algo>
void addSweepGenerations(History& h, Algo& algo, const std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>>& pairs,
                         const ShapeMap& wireElements, const ShapeMap& resultMap, ShapeMap* lateral) {
    for (const auto& [orig, cp] : pairs) {
        if (cp.ShapeType() != TopAbs_EDGE && cp.ShapeType() != TopAbs_VERTEX) continue;
        if (!wireElements.Contains(cp)) continue;
        for (const TopoDS_Shape& g : algo.Generated(cp)) {
            if (g.IsNull() || !resultMap.Contains(g)) continue;
            h->AddGenerated(orig, g);
            if (lateral && g.ShapeType() == TopAbs_FACE) lateral->Add(g);
        }
    }
}

// Торцы тела заметания: грани, не порождённые рёбрами профиля, по близости к точкам start/end.
void addCaps(History& h, const TopoDS_Shape& solid, const ShapeMap& lateral, const TopoDS_Shape& profileFace,
             const gp_Pnt& start, const std::optional<gp_Pnt>& end, const TopoDS_Shape& endSource) {
    std::vector<TopoDS_Shape> caps;
    for (TopExp_Explorer ex(solid, TopAbs_FACE); ex.More(); ex.Next())
        if (!lateral.Contains(ex.Current())) caps.push_back(ex.Current());
    if (caps.empty()) return;
    auto nearest = [&](const gp_Pnt& p) {
        std::size_t best = 0;
        double bd = 1e300;
        for (std::size_t i = 0; i < caps.size(); ++i) {
            const double d = centroidOf(caps[i]).Distance(p);
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    };
    const std::size_t first = nearest(start);
    h->AddModified(profileFace, caps[first]);
    if (end && caps.size() > 1) {
        std::size_t last = nearest(*end);
        if (last == first) {
            // Торцы равноудалены от конца (короткая траектория): второй — оставшийся.
            last = first == 0 ? 1 : 0;
        }
        h->AddGenerated(endSource, caps[last]);
    }
}

// Тонкостенное тело: стенки вдоль контуров без торцов.
Result<TopoDS_Shape> thicken(TopoDS_Shape& tool, History& history, const ThinWall& thin, const ShapeMap& lateral) {
    if (!(std::isfinite(thin.thickness) && thin.thickness > 0.0))
        return Error{ErrorCode::InvalidInput, "thin wall thickness must be > 0"};
    ShapeList caps;
    for (TopExp_Explorer ex(tool, TopAbs_FACE); ex.More(); ex.Next())
        if (!lateral.Contains(ex.Current())) caps.Append(ex.Current());
    if (caps.IsEmpty()) return Error{ErrorCode::Unsupported, "thin wall needs an open path (end faces to remove)"};
    const double offset = thin.side == ThinSide::Inside ? -thin.thickness : thin.thickness;
    BRepOffsetAPI_MakeThickSolid thick;
    thick.MakeThickSolidByJoin(tool, caps, offset, 1e-6, BRepOffset_Skin, false, false, GeomAbs_Intersection);
    if (!thick.IsDone()) return Error{ErrorCode::OperationFailed, "thin wall: BRepOffsetAPI_MakeThickSolid failed"};
    History th = algoHistory(tool, thick);
    mergeInto(history, th);
    tool = thick.Shape();
    return tool;
}

bool samePoint(const gp_Pnt& a, const gp_Pnt& b, double tol) { return a.Distance(b) <= tol; }

}  // namespace

// ---------------------------------------------------------------- цепочка рёбер

Result<TopoDS_Wire> chainEdges(const std::vector<TopoDS_Shape>& input) {
    if (input.empty()) return Error{ErrorCode::InvalidInput, "path has no edges"};
    try {
        std::vector<TopoDS_Edge> edges;
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i].IsNull() || input[i].ShapeType() != TopAbs_EDGE)
                return Error{ErrorCode::InvalidInput, "path element #" + std::to_string(i) + " is not an edge"};
            const TopoDS_Edge e = TopoDS::Edge(input[i]);
            if (BRep_Tool::Degenerated(e)) continue;
            if (std::any_of(edges.begin(), edges.end(), [&](const TopoDS_Edge& x) { return x.IsSame(e); })) continue;
            edges.push_back(e);
        }
        if (edges.empty()) return Error{ErrorCode::InvalidInput, "path has no edges"};
        // Концы рёбер и их совпадения (по положению: рёбра модели и эскиза могут не делить вершины).
        const double tol = 1e-6 * std::max(1.0, boundingDiagonal(std::vector<TopoDS_Shape>(edges.begin(), edges.end())));
        std::vector<gp_Pnt> nodes;
        auto node = [&](const gp_Pnt& p) {
            for (std::size_t i = 0; i < nodes.size(); ++i)
                if (samePoint(nodes[i], p, tol)) return i;
            nodes.push_back(p);
            return nodes.size() - 1;
        };
        std::vector<std::pair<std::size_t, std::size_t>> ends;
        for (const auto& e : edges) {
            TopoDS_Vertex v1, v2;
            TopExp::Vertices(e, v1, v2);
            if (v1.IsNull() || v2.IsNull()) return Error{ErrorCode::InvalidInput, "path edge without end vertices"};
            ends.emplace_back(node(BRep_Tool::Pnt(v1)), node(BRep_Tool::Pnt(v2)));
        }
        std::vector<int> degree(nodes.size(), 0);
        for (const auto& [a, b] : ends) {
            ++degree[a];
            ++degree[b];
        }
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (degree[i] > 2) return Error{ErrorCode::InvalidInput, "path branches: more than two edges meet at a point"};
        std::size_t startNode = ends.front().first;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (degree[i] == 1) { startNode = i; break; }
        // Обход цепочки от свободного конца (или от любого узла замкнутой цепочки).
        std::vector<bool> used(edges.size(), false);
        BRepBuilderAPI_MakeWire mw;
        std::size_t at = startNode;
        for (std::size_t step = 0; step < edges.size(); ++step) {
            std::optional<std::size_t> next;
            for (std::size_t k = 0; k < edges.size() && !next; ++k)
                if (!used[k] && (ends[k].first == at || ends[k].second == at)) next = k;
            if (!next) return Error{ErrorCode::InvalidInput, "path is not connected"};
            used[*next] = true;
            mw.Add(edges[*next]);
            if (!mw.IsDone()) return Error{ErrorCode::InvalidInput, "path edges do not form a wire"};
            at = ends[*next].first == at ? ends[*next].second : ends[*next].first;
        }
        return mw.Wire();
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("chainEdges", e.what())};
    }
}

// ---------------------------------------------------------------- кинематика

Result<FeatureResult> sweep(const Profile& profile, const TopoDS_Wire& pathIn, const TopoDS_Shape& target,
                            const SweepParams& p) {
    auto pln = validateProfile(profile);
    if (!pln) return pln.error();
    if (pathIn.IsNull() || !TopExp_Explorer(pathIn, TopAbs_EDGE).More())
        return Error{ErrorCode::InvalidInput, "sweep path is empty"};
    if (p.orientation == SweepOrientation::Guide && (p.guide.IsNull() || !TopExp_Explorer(p.guide, TopAbs_EDGE).More()))
        return Error{ErrorCode::InvalidInput, "guide orientation needs a guide curve"};
    try {
        // Траектория, которая кончается в плоскости профиля (цепочка рёбер модели обходится от
        // любого свободного конца), обходится в обратную сторону.
        TopoDS_Wire path = pathIn;
        {
            BRepTools_WireExplorer first(pathIn);
            if (!first.More()) return Error{ErrorCode::InvalidInput, "sweep path is not a connected wire"};
            const gp_Pnt s0 = BRep_Tool::Pnt(first.CurrentVertex());
            gp_Pnt s1 = s0;
            for (; first.More(); first.Next()) {
                TopoDS_Vertex a, b;
                TopExp::Vertices(first.Current(), a, b, true);
                s1 = BRep_Tool::Pnt(b);
            }
            const double tolPlane = 1e-6 * std::max(1.0, boundingDiagonal({profile.face, pathIn}));
            if (pln.value().Distance(s0) > tolPlane && pln.value().Distance(s1) <= tolPlane)
                path = TopoDS::Wire(pathIn.Reversed());
        }
        // Начало и конец траектории в порядке обхода проволоки.
        BRepTools_WireExplorer wex(path);
        if (!wex.More()) return Error{ErrorCode::InvalidInput, "sweep path is not a connected wire"};
        const gp_Pnt start = BRep_Tool::Pnt(wex.CurrentVertex());
        TopoDS_Vertex firstV, lastV;
        TopExp::Vertices(path, firstV, lastV);
        const bool closedPath = path.Closed() || (!firstV.IsNull() && !lastV.IsNull() && firstV.IsSame(lastV));
        gp_Pnt end = start;
        for (; wex.More(); wex.Next()) {
            TopoDS_Vertex a, b;
            TopExp::Vertices(wex.Current(), a, b, true);
            end = BRep_Tool::Pnt(b);
        }
        const double diag = boundingDiagonal({profile.face, path});
        if (pln.value().Distance(start) > 1e-6 * std::max(1.0, diag))
            return Error{ErrorCode::InvalidInput, "sweep path does not start in the profile plane"};

        auto base = transformCopy(profile.face, gp_Trsf());
        if (!base) return base.error();
        const TopoDS_Face copyFace = TopoDS::Face(base.value().shape);
        const std::vector<TopoDS_Wire> wires = faceWires(copyFace);

        History history = newHistory();
        std::vector<TopoDS_Shape> solids;
        ShapeMap lateralAll;
        for (std::size_t w = 0; w < wires.size(); ++w) {
            std::optional<BRepOffsetAPI_MakePipeShell> ps;
            for (BRepBuilderAPI_TransitionMode transition : {BRepBuilderAPI_RightCorner, BRepBuilderAPI_Transformed}) {
                ps.emplace(path);
                switch (p.orientation) {
                    case SweepOrientation::CorrectedFrenet: ps->SetMode(false); break;
                    case SweepOrientation::Frenet: ps->SetMode(true); break;
                    case SweepOrientation::Fixed:
                        ps->SetMode(gp_Ax2(start, profile.placement.Direction(), profile.placement.XDirection()));
                        break;
                    case SweepOrientation::Binormal: ps->SetMode(p.binormal); break;
                    case SweepOrientation::Guide: ps->SetMode(p.guide, true, BRepFill_NoContact); break;
                }
                ps->SetTransitionMode(transition);
                ps->Add(wires[w], false, false);
                ps->Build();
                if (ps->IsDone() && ps->MakeSolid()) break;
                ps.reset();
            }
            if (!ps) {
                return Error{ErrorCode::OperationFailed,
                             "BRepOffsetAPI_MakePipeShell failed for profile contour #" + std::to_string(w + 1)};
            }
            const TopoDS_Shape solid = ps->Shape();
            if (countSolids(solid) != 1)
                return Error{ErrorCode::OperationFailed, "sweep of contour #" + std::to_string(w + 1) + " is not a solid"};
            ShapeMap wireMap, resMap, lateral;
            TopExp::MapShapes(wires[w], wireMap);
            TopExp::MapShapes(solid, resMap);
            addSweepGenerations(history, *ps, base.value().pairs, wireMap, resMap, &lateral);
            if (w == 0) addCaps(history, solid, lateral, profile.face, start,
                                closedPath ? std::nullopt : std::optional<gp_Pnt>(end), profile.face);
            for (int i = 1; i <= lateral.Extent(); ++i) lateralAll.Add(lateral(i));
            if (!isValidShape(solid))
                return Error{ErrorCode::OperationFailed, "sweep of contour #" + std::to_string(w + 1) +
                                                             " is invalid (self-intersecting path or profile)"};
            if (!(volumeOf(solid) > 0.0))
                return Error{ErrorCode::OperationFailed, "sweep of contour #" + std::to_string(w + 1) +
                                                             " has no volume"};
            solids.push_back(solid);
        }
        TopoDS_Shape tool = solids.front();
        std::vector<std::string> warnings;
        if (solids.size() > 1) {
            std::vector<TopoDS_Shape> inner(solids.begin() + 1, solids.end());
            auto cut = booleanWithHistory(kernel::BooleanType::Cut, {tool}, inner, true, true);
            if (!cut) return Error{cut.error().code, "sweep: profile holes: " + cut.error().message};
            mergeInto(history, cut.value().history);
            tool = cut.value().shape;
            warnings = cut.value().warnings;
            ShapeMap images;
            for (int i = 1; i <= lateralAll.Extent(); ++i) {
                if (containsSubShape(tool, lateralAll(i))) images.Add(lateralAll(i));
                for (const TopoDS_Shape& m : cut.value().history->Modified(lateralAll(i))) images.Add(m);
            }
            lateralAll = images;
        }
        if (p.thin) {
            auto th = thicken(tool, history, *p.thin, lateralAll);
            if (!th) return th.error();
        }
        return detail::applyTool(p.mode, target, tool, history, std::move(warnings));
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("sweep", e.what())};
    }
}

// ---------------------------------------------------------------- по сечениям

Result<FeatureResult> loft(const std::vector<Profile>& sections, const TopoDS_Shape& target, const LoftParams& p) {
    if (sections.size() < 2) return Error{ErrorCode::InvalidInput, "loft needs at least two sections"};
    if (p.closed && sections.size() < 3) return Error{ErrorCode::InvalidInput, "closed loft needs at least three sections"};
    std::vector<gp_Pln> planes;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        auto pln = validateProfile(sections[i]);
        if (!pln) return Error{pln.error().code, "section #" + std::to_string(i + 1) + ": " + pln.error().message};
        planes.push_back(pln.value());
    }
    const double diag = [&] {
        std::vector<TopoDS_Shape> faces;
        for (const auto& s : sections) faces.push_back(s.face);
        return boundingDiagonal(faces);
    }();
    for (std::size_t i = 0; i + 1 < sections.size(); ++i) {
        const gp_Pln& a = planes[i];
        const gp_Pln& b = planes[i + 1];
        if (a.Axis().Direction().IsParallel(b.Axis().Direction(), 1e-9) &&
            a.Distance(b.Location()) <= 1e-6 * std::max(1.0, diag))
            return Error{ErrorCode::InvalidInput, "sections #" + std::to_string(i + 1) + " and #" +
                                                      std::to_string(i + 2) + " lie in the same plane"};
    }
    try {
        std::vector<TransformedCopy> copies;
        std::vector<std::vector<TopoDS_Wire>> wires;
        for (const auto& s : sections) {
            auto c = transformCopy(s.face, gp_Trsf());
            if (!c) return c.error();
            wires.push_back(faceWires(TopoDS::Face(c.value().shape)));
            copies.push_back(std::move(c).value());
        }
        const std::size_t contours = wires.front().size();
        for (std::size_t i = 1; i < wires.size(); ++i)
            if (wires[i].size() != contours)
                return Error{ErrorCode::InvalidInput, "section #" + std::to_string(i + 1) + " has " +
                                                          std::to_string(wires[i].size()) + " contours, section #1 has " +
                                                          std::to_string(contours)};
        // Ориентация контуров: обход против часовой стрелки, если смотреть по направлению
        // от сечения к следующему (нормаль грани, обращённая вдоль хода, иначе контур
        // переворачивается), — иначе ThruSections закручивает тело.
        std::vector<gp_Pnt> centers;
        for (const auto& s : sections) centers.push_back(centroidOf(s.face));
        for (std::size_t i = 0; i < sections.size(); ++i) {
            const gp_Vec along = i + 1 < sections.size() ? gp_Vec(centers[i], centers[i + 1]) : gp_Vec(centers[i - 1], centers[i]);
            gp_Dir n = planes[i].Axis().Direction();
            if (sections[i].face.Orientation() == TopAbs_REVERSED) n.Reverse();
            if (along.Magnitude() > 1e-12 && gp_Vec(n).Dot(along) < 0)
                for (auto& w : wires[i]) w.Reverse();
        }

        History history = newHistory();
        std::vector<TopoDS_Shape> solids;
        ShapeMap lateralAll;
        for (std::size_t w = 0; w < contours; ++w) {
            BRepOffsetAPI_ThruSections ts(true, p.ruled, 1e-6);
            ts.CheckCompatibility(true);
            for (std::size_t i = 0; i < wires.size(); ++i) ts.AddWire(wires[i][w]);
            if (p.closed) ts.AddWire(wires.front()[w]);
            ts.Build();
            if (!ts.IsDone())
                return Error{ErrorCode::OperationFailed, "BRepOffsetAPI_ThruSections failed for contour #" + std::to_string(w + 1)};
            const TopoDS_Shape solid = ts.Shape();
            if (countSolids(solid) != 1)
                return Error{ErrorCode::OperationFailed, "loft of contour #" + std::to_string(w + 1) + " is not a solid"};
            ShapeMap resMap, lateral;
            TopExp::MapShapes(solid, resMap);
            for (std::size_t i = 0; i < wires.size(); ++i) {
                ShapeMap wireMap;
                TopExp::MapShapes(wires[i][w], wireMap);
                addSweepGenerations(history, ts, copies[i].pairs, wireMap, resMap, &lateral);
            }
            if (w == 0 && !p.closed) {
                const TopoDS_Shape first = ts.FirstShape(), last = ts.LastShape();
                if (!first.IsNull() && resMap.Contains(first)) history->AddModified(sections.front().face, first);
                if (!last.IsNull() && resMap.Contains(last)) history->AddGenerated(sections.back().face, last);
            }
            for (int i = 1; i <= lateral.Extent(); ++i) lateralAll.Add(lateral(i));
            if (!isValidShape(solid))
                return Error{ErrorCode::OperationFailed, "loft of contour #" + std::to_string(w + 1) + " is invalid"};
            if (!(volumeOf(solid) > 0.0))
                return Error{ErrorCode::OperationFailed, "loft of contour #" + std::to_string(w + 1) + " has no volume"};
            solids.push_back(solid);
        }
        TopoDS_Shape tool = solids.front();
        std::vector<std::string> warnings;
        if (solids.size() > 1) {
            std::vector<TopoDS_Shape> inner(solids.begin() + 1, solids.end());
            auto cut = booleanWithHistory(kernel::BooleanType::Cut, {tool}, inner, true, true);
            if (!cut) return Error{cut.error().code, "loft: section holes: " + cut.error().message};
            mergeInto(history, cut.value().history);
            tool = cut.value().shape;
            warnings = cut.value().warnings;
        }
        return detail::applyTool(p.mode, target, tool, history, std::move(warnings));
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("loft", e.what())};
    }
}

}  // namespace bendcore::modeling
