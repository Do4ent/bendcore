#include "feature_common.hpp"

#include <BOPAlgo_CheckResult.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools_History.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <GeomLib_IsPlanarSurface.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>

namespace bendcore::modeling {

const char* toString(FeatureMode mode) noexcept {
    switch (mode) {
        case FeatureMode::Boss: return "Boss";
        case FeatureMode::Cut: return "Cut";
        case FeatureMode::NewBody: return "NewBody";
    }
    return "Unknown";
}

std::optional<FeatureMode> featureModeFromString(const std::string& text) noexcept {
    if (text == "Boss" || text == "boss") return FeatureMode::Boss;
    if (text == "Cut" || text == "cut") return FeatureMode::Cut;
    if (text == "NewBody" || text == "newBody") return FeatureMode::NewBody;
    return std::nullopt;
}

std::vector<TopoDS_Shape> solidsOf(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Shape> out;
    if (shape.IsNull()) return out;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> m;
    TopExp::MapShapes(shape, TopAbs_SOLID, m);
    for (int i = 1; i <= m.Extent(); ++i) out.push_back(m(i));
    return out;
}

TopoDS_Shape makeCompound(const std::vector<TopoDS_Shape>& shapes) {
    if (shapes.size() == 1) return shapes.front();
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : shapes)
        if (!s.IsNull()) b.Add(c, s);
    return c;
}

const char* toString(ExtentType type) noexcept {
    switch (type) {
        case ExtentType::Blind: return "Blind";
        case ExtentType::Symmetric: return "Symmetric";
        case ExtentType::TwoSided: return "TwoSided";
        case ExtentType::ThroughAll: return "ThroughAll";
        case ExtentType::ThroughAllBoth: return "ThroughAllBoth";
        case ExtentType::UpToFace: return "UpToFace";
        case ExtentType::UpToSurface: return "UpToSurface";
    }
    return "Unknown";
}

}  // namespace bendcore::modeling

namespace bendcore::modeling::detail {

namespace {

bool hasSolid(const TopoDS_Shape& s) { return !s.IsNull() && TopExp_Explorer(s, TopAbs_SOLID).More(); }

struct FactsCache {
    std::mutex mutex;
    std::deque<std::pair<TopoDS_Shape, ShapeFacts>> entries;  // most recent last
};

FactsCache& factsCache() {
    static FactsCache cache;
    return cache;
}

const char* checkStatusName(BOPAlgo_CheckStatus status) {
    switch (status) {
        case BOPAlgo_CheckUnknown: return "CheckUnknown";
        case BOPAlgo_BadType: return "BadType";
        case BOPAlgo_SelfIntersect: return "SelfIntersect";
        case BOPAlgo_TooSmallEdge: return "TooSmallEdge";
        case BOPAlgo_NonRecoverableFace: return "NonRecoverableFace";
        case BOPAlgo_IncompatibilityOfVertex: return "IncompatibilityOfVertex";
        case BOPAlgo_IncompatibilityOfEdge: return "IncompatibilityOfEdge";
        case BOPAlgo_IncompatibilityOfFace: return "IncompatibilityOfFace";
        case BOPAlgo_OperationAborted: return "OperationAborted";
        case BOPAlgo_GeomAbs_C0: return "GeomAbs_C0";
        case BOPAlgo_InvalidCurveOnSurface: return "InvalidCurveOnSurface";
        case BOPAlgo_NotValid: return "NotValid";
    }
    return "Unknown";
}

// BRepAlgoAPI_Check (малые рёбра и самопересечения) — то же, что kernel::booleanOp делает при
// BooleanOptions::checkInputs; формы booleanReady пропускаются.
std::string checkBooleanInput(const TopoDS_Shape& s, const char* role, std::size_t index) {
    if (s.IsNull() || !TopExp_Explorer(s, TopAbs_EDGE).More()) return {};  // kernel::booleanOp reports it
    if (auto f = knownFacts(s); f && f->booleanReady) return {};
    BRepAlgoAPI_Check check(s, true, true);
    if (check.IsValid()) return {};
    std::ostringstream msg;
    msg << role << " #" << index << " failed BRepAlgoAPI_Check:";
    for (const BOPAlgo_CheckResult& r : check.Result()) msg << ' ' << checkStatusName(r.GetCheckStatus());
    return msg.str();
}

}  // namespace

std::optional<ShapeFacts> knownFacts(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return std::nullopt;
    FactsCache& c = factsCache();
    std::lock_guard<std::mutex> lock(c.mutex);
    for (auto it = c.entries.rbegin(); it != c.entries.rend(); ++it)
        if (it->first.IsEqual(shape)) return it->second;
    return std::nullopt;
}

void rememberFacts(const TopoDS_Shape& shape, const ShapeFacts& facts) {
    if (shape.IsNull()) return;
    FactsCache& c = factsCache();
    std::lock_guard<std::mutex> lock(c.mutex);
    ShapeFacts merged = facts;
    for (auto it = c.entries.begin(); it != c.entries.end(); ++it) {
        if (!it->first.IsEqual(shape)) continue;
        merged.valid = merged.valid || it->second.valid;
        merged.booleanReady = merged.booleanReady || it->second.booleanReady;
        if (!merged.volume) merged.volume = it->second.volume;
        if (!merged.mass) merged.mass = it->second.mass;
        c.entries.erase(it);
        break;
    }
    c.entries.emplace_back(shape, merged);
    while (c.entries.size() > kShapeFactsCapacity) c.entries.pop_front();
}

bool isValidShape(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return false;
    if (auto f = knownFacts(shape); f && f->valid) return true;
    if (!BRepCheck_Analyzer(shape, true, true).IsValid()) return false;
    rememberFacts(shape, ShapeFacts{true, false, std::nullopt});
    return true;
}

std::string occtMessage(const char* where, const char* what) {
    return std::string("OCCT exception in ") + where + ": " + (what ? what : "");
}

double volumeOf(const TopoDS_Shape& shape) {
    if (!hasSolid(shape)) return 0.0;
    if (auto f = knownFacts(shape); f && f->volume) return *f->volume;
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    rememberFacts(shape, ShapeFacts{false, false, props.Mass()});
    return props.Mass();
}

int countSolids(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return 0;
    ShapeMap m;
    TopExp::MapShapes(shape, TopAbs_SOLID, m);
    return m.Extent();
}

double boundingDiagonal(const std::vector<TopoDS_Shape>& shapes) {
    Bnd_Box box;
    for (const auto& s : shapes)
        if (!s.IsNull()) BRepBndLib::Add(s, box);
    if (box.IsVoid()) return 0.0;
    return std::sqrt(box.SquareExtent());
}

void projectionRange(const TopoDS_Shape& shape, const gp_Pnt& origin, const gp_Dir& dir, double& lo, double& hi) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    lo = 0.0;
    hi = 0.0;
    if (box.IsVoid()) return;
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    bool first = true;
    for (int i = 0; i < 8; ++i) {
        const gp_Pnt c((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
        const double t = gp_Vec(origin, c).Dot(gp_Vec(dir));
        if (first) { lo = hi = t; first = false; }
        lo = std::min(lo, t);
        hi = std::max(hi, t);
    }
}

bool containsSubShape(const TopoDS_Shape& whole, const TopoDS_Shape& s) {
    if (whole.IsNull() || s.IsNull()) return false;
    ShapeMap m;
    TopExp::MapShapes(whole, s.ShapeType(), m);
    return m.Contains(s);
}

History newHistory() { return new BRepTools_History(); }

History copyHistory(const History& h) {
    History out = newHistory();
    if (!h.IsNull()) out->Merge(h);
    return out;
}

void mergeInto(History& a, const History& b) {
    if (a.IsNull()) a = newHistory();
    if (!b.IsNull()) a->Merge(b);
}

Result<gp_Pln> validateProfile(const Profile& profile) {
    const TopoDS_Face& face = profile.face;
    if (face.IsNull()) return Error{ErrorCode::InvalidInput, "profile face is null"};
    try {
        if (face.ShapeType() != TopAbs_FACE) return Error{ErrorCode::InvalidInput, "profile is not a face"};
        opencascade::handle<Geom_Surface> surf = BRep_Tool::Surface(face);
        if (surf.IsNull()) return Error{ErrorCode::InvalidInput, "profile face has no surface"};
        GeomLib_IsPlanarSurface planar(surf, 1e-7);
        if (!planar.IsPlanar()) return Error{ErrorCode::InvalidInput, "profile face is not planar"};
        const gp_Pln pln = planar.Plan();

        const gp_Dir n = profile.placement.Direction();
        if (!pln.Axis().Direction().IsParallel(n, 1e-7))
            return Error{ErrorCode::InvalidInput, "profile face is not parallel to the sketch plane"};
        if (pln.Distance(profile.placement.Location()) > 1e-6)
            return Error{ErrorCode::InvalidInput, "profile face does not lie in the sketch plane"};

        int wires = 0;
        for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
            ++wires;
            const TopoDS_Wire wire = TopoDS::Wire(wex.Current());
            std::map<int, int> uses;
            ShapeMap vertices;
            int edges = 0;
            for (TopExp_Explorer eex(wire, TopAbs_EDGE); eex.More(); eex.Next()) {
                const TopoDS_Edge e = TopoDS::Edge(eex.Current());
                if (BRep_Tool::Degenerated(e)) continue;
                ++edges;
                TopoDS_Vertex v1, v2;
                TopExp::Vertices(e, v1, v2);
                if (v1.IsNull() || v2.IsNull())
                    return Error{ErrorCode::InvalidInput, "profile edge without end vertices"};
                ++uses[vertices.Add(v1)];
                ++uses[vertices.Add(v2)];
            }
            if (edges == 0) return Error{ErrorCode::InvalidInput, "profile contour has no edges"};
            for (const auto& [index, count] : uses)
                if (count % 2 != 0) {
                    std::ostringstream msg;
                    msg << "profile contour #" << wires << " is not closed (vertex used " << count << " time(s))";
                    return Error{ErrorCode::InvalidInput, msg.str()};
                }
        }
        if (wires == 0) return Error{ErrorCode::InvalidInput, "profile face has no contours"};

        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        if (!(std::abs(props.Mass()) > 1e-12)) return Error{ErrorCode::InvalidInput, "profile face has zero area"};
        if (!BRepCheck_Analyzer(face).IsValid())
            return Error{ErrorCode::InvalidInput, "profile face is invalid (BRepCheck_Analyzer)"};
        return pln;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::InvalidInput, occtMessage("validateProfile", e.what())};
    }
}

std::string checkSolidInput(const TopoDS_Shape& shape, const char* role) {
    if (shape.IsNull()) return std::string(role) + " is null";
    if (!hasSolid(shape)) return std::string(role) + " contains no solid";
    if (!isValidShape(shape)) return std::string(role) + " is invalid (BRepCheck_Analyzer)";
    return {};
}

std::string checkSolidResult(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return "result is null";
    if (!hasSolid(shape)) return "result contains no solid";
    if (!isValidShape(shape)) return "result is invalid (BRepCheck_Analyzer)";
    // Одно тело: его объём равен объёму всей формы (возможно, уже известному).
    if (countSolids(shape) == 1) {
        // ... if the shape holds nothing but that solid (no free faces besides it).
        TopExp_Explorer ex(shape, TopAbs_SOLID);
        int faces = 0, solidFaces = 0;
        for (TopExp_Explorer f(shape, TopAbs_FACE); f.More(); f.Next()) ++faces;
        for (TopExp_Explorer f(ex.Current(), TopAbs_FACE); f.More(); f.Next()) ++solidFaces;
        const double v = faces == solidFaces ? volumeOf(shape) : volumeOf(ex.Current());
        if (!(v > 0.0)) return "result contains a solid with non-positive volume";
        return {};
    }
    for (TopExp_Explorer ex(shape, TopAbs_SOLID); ex.More(); ex.Next())
        if (!(volumeOf(ex.Current()) > 0.0)) return "result contains a solid with non-positive volume";
    return {};
}

Result<TransformedCopy> transformCopy(const TopoDS_Shape& shape, const gp_Trsf& trsf) {
    try {
        BRepBuilderAPI_Transform tr(shape, trsf, true);
        if (!tr.IsDone()) return Error{ErrorCode::OperationFailed, "BRepBuilderAPI_Transform failed"};
        TransformedCopy out;
        out.shape = tr.Shape();
        for (TopAbs_ShapeEnum type : {TopAbs_SOLID, TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
            ShapeMap m;
            TopExp::MapShapes(shape, type, m);
            for (int i = 1; i <= m.Extent(); ++i) {
                TopoDS_Shape img = tr.ModifiedShape(m(i));
                if (!img.IsNull()) out.pairs.emplace_back(m(i), img);
            }
        }
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("transformCopy", e.what())};
    }
}

// ---------------------------------------------------------------- история булевой операции

Result<BooleanStep> booleanWithHistory(kernel::BooleanType type, const std::vector<TopoDS_Shape>& arguments,
                                       const std::vector<TopoDS_Shape>& tools, bool trackArguments,
                                       bool trackTools) {
    // Входы проверяются BRepAlgoAPI_Check здесь, а не в ядре: результат предыдущей булевой
    // операции (booleanReady) повторно не проверяется.
    {
        std::string problems;
        try {
            for (std::size_t i = 0; i < arguments.size(); ++i)
                if (std::string p = checkBooleanInput(arguments[i], "argument", i); !p.empty())
                    problems += (problems.empty() ? "" : "; ") + p;
            for (std::size_t i = 0; i < tools.size(); ++i)
                if (std::string p = checkBooleanInput(tools[i], "tool", i); !p.empty())
                    problems += (problems.empty() ? "" : "; ") + p;
        } catch (const Standard_Failure& e) {
            return Error{ErrorCode::OperationFailed, occtMessage("BRepAlgoAPI_Check", e.what())};
        }
        if (!problems.empty()) return Error{ErrorCode::InvalidInput, problems};
    }
    kernel::BooleanOptions options;
    options.checkInputs = false;
    // Объём и площадь входов, уже известные по кэшу фактов (результат предыдущей булевой
    // операции, BooleanResult::mass), ядро не пересчитывает.
    auto knownMass = [](const std::vector<TopoDS_Shape>& shapes) {
        std::vector<std::optional<kernel::BooleanInputMass>> out;
        bool any = false;
        for (const auto& s : shapes) {
            auto f = knownFacts(s);
            out.push_back(f ? f->mass : std::nullopt);
            any = any || out.back().has_value();
        }
        if (!any) out.clear();
        return out;
    };
    options.argumentMass = knownMass(arguments);
    options.toolMass = knownMass(tools);
    auto res = kernel::booleanOp(type, arguments, tools, options);
    if (!res) return res.error();
    // kernel::booleanOp принимает результат только после BRepCheck_Analyzer и вычисляет его объём.
    if (TopExp_Explorer(res.value().shape, TopAbs_SOLID).More()) {
        ShapeFacts facts{true, res.value().fuzzyValue == 0.0, res.value().volume, res.value().mass};
        rememberFacts(res.value().shape, facts);
    }
    BooleanStep step;
    step.shape = res.value().shape;
    step.warnings = res.value().warnings;
    step.fuzzyValue = res.value().fuzzyValue;
    step.history = newHistory();
    const History& kernelHistory = res.value().history;

    try {
        ShapeMap trackedMap, untrackedMap, resMap;
        for (const auto& s : arguments) TopExp::MapShapes(s, trackArguments ? trackedMap : untrackedMap);
        for (const auto& s : tools) TopExp::MapShapes(s, trackTools ? trackedMap : untrackedMap);
        TopExp::MapShapes(step.shape, resMap);

        // История ядра, ограниченная элементами отслеживаемых входов. Пустой результат
        // (booleanOp возвращает его без истории) — все элементы входов удалены.
        ShapeMap fromTracked;
        for (int i = 1; i <= trackedMap.Extent(); ++i) {
            const TopoDS_Shape& s = trackedMap(i);
            if (!BRepTools_History::IsSupportedType(s)) continue;
            if (kernelHistory.IsNull()) {
                if (!resMap.Contains(s)) step.history->Remove(s);
                else fromTracked.Add(s);
                continue;
            }
            if (kernelHistory->IsRemoved(s)) step.history->Remove(s);
            for (const TopoDS_Shape& m : kernelHistory->Modified(s)) {
                step.history->AddModified(s, m);
                fromTracked.Add(m);
            }
            for (const TopoDS_Shape& g : kernelHistory->Generated(s)) {
                step.history->AddGenerated(s, g);
                fromTracked.Add(g);
            }
            if (resMap.Contains(s)) fromTracked.Add(s);
        }

        // Грани результата, происходящие только из неотслеживаемых входов.
        ShapeMap untrackedFaces;
        for (int i = 1; i <= untrackedMap.Extent(); ++i) {
            const TopoDS_Shape& s = untrackedMap(i);
            if (s.ShapeType() != TopAbs_FACE) continue;
            ShapeList images;
            if (resMap.Contains(s)) images.Append(s);
            if (!kernelHistory.IsNull())
                for (const TopoDS_Shape& m : kernelHistory->Modified(s)) images.Append(m);
            for (const TopoDS_Shape& f : images)
                if (!fromTracked.Contains(f)) untrackedFaces.Add(f);
        }
        for (int i = 1; i <= untrackedFaces.Extent(); ++i) step.untrackedImages.push_back(untrackedFaces(i));
        return step;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("boolean history", e.what())};
    }
}

History normalizeHistory(const TopoDS_Shape& input, const TopoDS_Shape& result, const History& raw) {
    History h = newHistory();
    ShapeMap resMap;
    TopExp::MapShapes(result, resMap);
    for (TopAbs_ShapeEnum type : {TopAbs_SOLID, TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
        ShapeMap m;
        TopExp::MapShapes(input, type, m);
        for (int i = 1; i <= m.Extent(); ++i) {
            const TopoDS_Shape& s = m(i);
            for (const ShapeList* list : {&raw->Modified(s), &raw->Generated(s)})
                for (const TopoDS_Shape& img : *list) {
                    if (img.IsNull() || img.IsSame(s) || !resMap.Contains(img)) continue;
                    if (img.ShapeType() == s.ShapeType()) h->AddModified(s, img);
                    else h->AddGenerated(s, img);
                }
            if (!h->Modified(s).IsEmpty()) continue;
            if (resMap.Contains(s)) continue;
            h->Remove(s);  // нет в результате и нет Modified; Generated-образы допустимы
        }
    }
    return h;
}

void addCopyGenerations(History& history, const std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>>& pairs,
                        const TopoDS_Shape& result, const History& stepHistory) {
    ShapeMap resMap;
    TopExp::MapShapes(result, resMap);
    for (const auto& [orig, cp] : pairs) {
        if (resMap.Contains(cp)) {
            history->AddGenerated(orig, cp);
            continue;
        }
        if (stepHistory.IsNull()) continue;
        for (const TopoDS_Shape& img : stepHistory->Modified(cp))
            if (resMap.Contains(img)) history->AddGenerated(orig, img);
    }
}

std::vector<TopoDS_Shape> bodiesNear(const std::vector<TopoDS_Shape>& bodies, const TopoDS_Shape& tool,
                                     double tolerance) {
    Bnd_Box toolBox;
    BRepBndLib::Add(tool, toolBox);
    toolBox.Enlarge(tolerance);
    std::vector<TopoDS_Shape> out;
    for (const auto& b : bodies) {
        Bnd_Box box;
        BRepBndLib::Add(b, box);
        if (!box.IsOut(toolBox)) out.push_back(b);
    }
    return out;
}

History combineHistories(const std::vector<TopoDS_Shape>& inputs, const std::vector<History>& steps,
                         const TopoDS_Shape& result) {
    History h = newHistory();
    ShapeMap resMap;
    TopExp::MapShapes(result, resMap);
    ShapeMap all;
    for (const auto& s : inputs) TopExp::MapShapes(s, all);
    for (int i = 1; i <= all.Extent(); ++i) {
        const TopoDS_Shape& s = all(i);
        if (!BRepTools_History::IsSupportedType(s)) continue;
        ShapeMap modified, generated;
        bool removed = false;
        for (const History& st : steps) {
            if (st.IsNull()) continue;
            for (const TopoDS_Shape& m : st->Modified(s))
                if (resMap.Contains(m)) modified.Add(m);
            for (const TopoDS_Shape& g : st->Generated(s))
                if (resMap.Contains(g)) generated.Add(g);
            removed = removed || st->IsRemoved(s);
        }
        for (int k = 1; k <= modified.Extent(); ++k) h->AddModified(s, modified(k));
        for (int k = 1; k <= generated.Extent(); ++k) h->AddGenerated(s, generated(k));
        if (modified.IsEmpty() && !resMap.Contains(s) && removed) h->Remove(s);
    }
    return h;
}

namespace {

// Новое тело: составное из тел цели и тел инструмента; элементы цели не меняются.
Result<FeatureResult> addNewBody(const TopoDS_Shape& target, FeatureResult out) {
    std::vector<TopoDS_Shape> parts = solidsOf(target);
    const std::vector<TopoDS_Shape> toolSolids = solidsOf(out.tool);
    parts.insert(parts.end(), toolSolids.begin(), toolSolids.end());
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : parts) b.Add(c, s);
    out.shape = c;
    // Тела не взаимодействуют: объём — сумма, валидность — валидность частей.
    rememberFacts(out.shape, ShapeFacts{true, false, volumeOf(target) + volumeOf(out.tool), std::nullopt});
    if (std::string p = checkSolidResult(out.shape); !p.empty()) return Error{ErrorCode::OperationFailed, p};
    return out;
}

// Многотельная цель: инструмент применяется только к телам, габарит которых он задевает;
// Boss объединяет инструмент со всеми задетыми телами одной операцией, Cut вычитает его из
// каждого задетого тела отдельно (тела не сливаются друг с другом). Прочие тела переходят без
// изменений.
Result<FeatureResult> applyToBodies(FeatureMode mode, const TopoDS_Shape& target, FeatureResult out) {
    const std::vector<TopoDS_Shape> bodies = solidsOf(target);
    const double tol = 1e-6 * std::max(1.0, boundingDiagonal({target, out.tool}));
    const std::vector<TopoDS_Shape> near = bodiesNear(bodies, out.tool, tol);
    std::vector<TopoDS_Shape> kept;
    for (const auto& b : bodies)
        if (std::none_of(near.begin(), near.end(), [&](const TopoDS_Shape& n) { return n.IsSame(b); }))
            kept.push_back(b);
    std::vector<TopoDS_Shape> parts = kept;
    std::vector<History> steps;
    double volume = 0.0;
    for (const auto& k : kept) volume += volumeOf(k);
    auto take = [&](BooleanStep& st) {
        // Тела результата булевой операции с нулевым допуском готовы к следующей (ShapeFacts).
        for (const auto& s : solidsOf(st.shape)) {
            parts.push_back(s);
            rememberFacts(s, ShapeFacts{true, st.fuzzyValue == 0.0, std::nullopt, std::nullopt});
        }
        volume += volumeOf(st.shape);
        steps.push_back(st.history);
        out.warnings.insert(out.warnings.end(), st.warnings.begin(), st.warnings.end());
    };
    if (mode == FeatureMode::Boss) {
        if (near.empty()) {
            for (const auto& s : solidsOf(out.tool)) parts.push_back(s);
            volume += volumeOf(out.tool);
            out.warnings.push_back("tool touches no body of the part: it forms a separate solid");
        } else {
            auto step = booleanWithHistory(kernel::BooleanType::Fuse, near, {out.tool}, true, true);
            if (!step) return step.error();
            take(step.value());
        }
    } else {
        if (near.empty()) return Error{ErrorCode::OperationFailed, "cut tool does not intersect the target body"};
        double before = 0.0, after = 0.0;
        for (const auto& b : near) {
            auto step = booleanWithHistory(kernel::BooleanType::Cut, {b}, {out.tool}, true, true);
            if (!step) return step.error();
            before += volumeOf(b);
            after += volumeOf(step.value().shape);
            take(step.value());
        }
        if (std::abs(before - after) <= 1e-9 * std::max(1.0, before))
            return Error{ErrorCode::OperationFailed, "cut tool does not intersect the target body"};
    }
    if (parts.empty()) return Error{ErrorCode::OperationFailed, "cut removes every body of the part"};
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : parts) b.Add(c, s);
    out.shape = c;
    rememberFacts(out.shape, ShapeFacts{true, false, volume, std::nullopt});
    if (std::string p = checkSolidResult(out.shape); !p.empty()) return Error{ErrorCode::OperationFailed, p};
    std::vector<TopoDS_Shape> inputs = near;
    inputs.push_back(out.tool);
    mergeInto(out.history, combineHistories(inputs, steps, out.shape));
    const int solids = countSolids(out.shape), before = static_cast<int>(bodies.size());
    if (mode == FeatureMode::Cut && solids > before)
        out.warnings.push_back("cut splits a body: the part has " + std::to_string(solids) + " solids");
    return out;
}

}  // namespace

Result<FeatureResult> applyTool(FeatureMode mode, const TopoDS_Shape& target, const TopoDS_Shape& tool,
                                const History& toolHistory, std::vector<std::string> warnings) {
    if (std::string p = checkSolidResult(tool); !p.empty())
        return Error{ErrorCode::OperationFailed, "tool body: " + p};
    FeatureResult out;
    out.tool = tool;
    out.history = copyHistory(toolHistory);
    out.warnings = std::move(warnings);
    if (target.IsNull()) {
        if (mode == FeatureMode::Cut) return Error{ErrorCode::InvalidInput, "cut requires a target body"};
        out.shape = tool;
        return out;
    }
    if (std::string p = checkSolidInput(target, "target body"); !p.empty()) return Error{ErrorCode::InvalidInput, p};
    if (mode == FeatureMode::NewBody) return addNewBody(target, std::move(out));
    if (countSolids(target) > 1) return applyToBodies(mode, target, std::move(out));
    const kernel::BooleanType type = mode == FeatureMode::Boss ? kernel::BooleanType::Fuse : kernel::BooleanType::Cut;
    auto step = booleanWithHistory(type, {target}, {tool}, true, true);
    if (!step) return step.error();
    BooleanStep& st = step.value();
    if (mode == FeatureMode::Cut) {
        if (countSolids(st.shape) == 0) return Error{ErrorCode::OperationFailed, "cut removes the whole body"};
        const double vt = volumeOf(target), vr = volumeOf(st.shape);
        if (std::abs(vt - vr) <= 1e-9 * std::max(1.0, vt))
            return Error{ErrorCode::OperationFailed, "cut tool does not intersect the target body"};
    }
    if (std::string p = checkSolidResult(st.shape); !p.empty()) return Error{ErrorCode::OperationFailed, p};
    const int solids = countSolids(st.shape);
    if (solids > 1) out.warnings.push_back("result has " + std::to_string(solids) + " solids");
    mergeInto(out.history, st.history);
    out.warnings.insert(out.warnings.end(), st.warnings.begin(), st.warnings.end());
    out.shape = st.shape;
    return out;
}

}  // namespace bendcore::modeling::detail
