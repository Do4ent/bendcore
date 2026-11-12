// Проверка пересечений (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 6).

#include <bendcore/assembly_v2/interference.hpp>

#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_List.hxx>
#include <gp_Pln.hxx>
#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

const char* toString(ContactKind kind) noexcept {
    switch (kind) {
        case ContactKind::Interference: return "Interference";
        case ContactKind::Touching: return "Touching";
    }
    return "Unknown";
}


namespace {

// Детали с числом граней больше этого обрезаются габаритом пары перед Common и расстоянием.
constexpr int kClipFaceCount = 32;

// Габариты граней детали (вычисляются один раз на деталь, по требованию).
struct PartFaces {
    bool ready = false;
    bool onlySolids = false;  // каждая грань принадлежит телу (классификация точки корректна)
    int count = 0;
    std::vector<Bnd_Box> boxes;
};

void prepareFaces(const TopoDS_Shape& shape, double enlarge, PartFaces& pf) {
    if (pf.ready) return;
    pf.ready = true;
    int inSolids = 0;
    for (TopExp_Explorer s(shape, TopAbs_SOLID); s.More(); s.Next())
        for (TopExp_Explorer f(s.Current(), TopAbs_FACE); f.More(); f.Next()) ++inSolids;
    for (TopExp_Explorer f(shape, TopAbs_FACE); f.More(); f.Next()) {
        Bnd_Box b;
        BRepBndLib::AddOptimal(f.Current(), b, false, false);
        b.Enlarge(enlarge);
        pf.boxes.push_back(b);
    }
    pf.count = static_cast<int>(pf.boxes.size());
    pf.onlySolids = pf.count > 0 && inSolids == pf.count;
}

bool anyFaceTouches(const PartFaces& pf, const Bnd_Box& box) {
    for (const Bnd_Box& b : pf.boxes)
        if (!b.IsVoid() && !b.IsOut(box)) return true;
    return false;
}

// Точка формы (первая вершина) внутри тел other (или на границе).
bool pointInside(const TopoDS_Shape& shape, const TopoDS_Shape& other) {
    TopExp_Explorer v(shape, TopAbs_VERTEX);
    if (!v.More()) return true;  // нечего классифицировать — считать возможным пересечением
    BRepClass3d_SolidClassifier cls(other, BRep_Tool::Pnt(TopoDS::Vertex(v.Current())), Precision::Confusion());
    return cls.State() != TopAbs_OUT;
}

// Крупная деталь, разрезанная сеткой плоскостей на куски (BRepAlgoAPI_Splitter, один раз на
// деталь): объединение кусков — сама деталь, куски не перекрываются. Для пары берутся только
// куски, габарит которых пересекает габарит другой детали.
struct Piece {
    TopoDS_Shape shape;
    Bnd_Box box;
};

std::vector<Piece> splitByGrid(const TopoDS_Shape& shape, const Bnd_Box& box, int faceCount, double enlarge) {
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    double ext[3] = {x1 - x0, y1 - y0, z1 - z0};
    const double lo[3] = {x0, y0, z0};
    // Около kClipFaceCount / 8 граней на ячейку; тонкие направления не делятся.
    const double cells = std::max(1.0, 8.0 * faceCount / kClipFaceCount);
    int cnt[3] = {1, 1, 1};
    bool thin[3] = {false, false, false};
    for (int pass = 0; pass < 3; ++pass) {
        double vol = 1.0;
        int dims = 0;
        for (int k = 0; k < 3; ++k)
            if (!thin[k]) { vol *= ext[k]; ++dims; }
        if (dims == 0 || !(vol > 0)) break;
        const double step = std::pow(vol / cells, 1.0 / dims);
        bool changed = false;
        for (int k = 0; k < 3; ++k)
            if (!thin[k] && ext[k] < step) { thin[k] = true; changed = true; }
        if (changed) continue;
        for (int k = 0; k < 3; ++k) cnt[k] = thin[k] ? 1 : std::max(1, int(std::lround(ext[k] / step)));
        break;
    }
    std::vector<Piece> pieces;
    if (cnt[0] * cnt[1] * cnt[2] < 2) return pieces;
    NCollection_List<TopoDS_Shape> args, tools;
    args.Append(shape);
    const double pad = 0.1 * std::sqrt(box.SquareExtent()) + 1.0;
    for (int k = 0; k < 3; ++k) {
        for (int c = 1; c < cnt[k]; ++c) {
            gp_Pnt o(x0, y0, z0);
            o.SetCoord(k + 1, lo[k] + ext[k] * c / cnt[k]);
            const gp_Dir n(k == 0 ? 1 : 0, k == 1 ? 1 : 0, k == 2 ? 1 : 0);
            const gp_Pln pln(o, n);
            // Плоскость с запасом за габарит детали.
            BRepBuilderAPI_MakeFace mf(pln, -pad - 2 * std::max({ext[0], ext[1], ext[2]}),
                                       pad + 2 * std::max({ext[0], ext[1], ext[2]}),
                                       -pad - 2 * std::max({ext[0], ext[1], ext[2]}),
                                       pad + 2 * std::max({ext[0], ext[1], ext[2]}));
            if (!mf.IsDone()) return {};
            tools.Append(mf.Face());
        }
    }
    BRepAlgoAPI_Splitter splitter;
    splitter.SetArguments(args);
    splitter.SetTools(tools);
    splitter.SetRunParallel(true);
    splitter.SetNonDestructive(true);
    splitter.SetToFillHistory(false);
    splitter.Build();
    if (!splitter.IsDone() || splitter.HasErrors()) return {};
    for (TopExp_Explorer ex(splitter.Shape(), TopAbs_SOLID); ex.More(); ex.Next()) {
        Piece pc{ex.Current(), {}};
        BRepBndLib::AddOptimal(pc.shape, pc.box, false, false);
        pc.box.Enlarge(enlarge);
        pieces.push_back(std::move(pc));
    }
    // Объём кусков должен совпасть с объёмом детали, иначе куски не используются.
    GProp_GProps whole, parts;
    BRepGProp::VolumeProperties(shape, whole);
    for (const Piece& pc : pieces) {
        GProp_GProps g;
        BRepGProp::VolumeProperties(pc.shape, g);
        parts.Add(g);
    }
    if (pieces.size() < 2 || std::abs(whole.Mass() - parts.Mass()) > 1e-9 * std::max(1.0, std::abs(whole.Mass())))
        return {};
    return pieces;
}

}  // namespace

Result<InterferenceReport> checkInterference(const AssemblyDocument& document, const InterferenceOptions& options) {
    auto parts = document.placedParts();
    if (!parts) return parts.error();
    return checkInterference(std::move(parts).value(), options);
}

Result<InterferenceReport> checkInterference(std::vector<PlacedOccurrence> parts, const InterferenceOptions& options) {
    if (!(options.volumeTolerance >= 0.0) || !(options.contactTolerance >= 0.0))
        return Error{ErrorCode::InvalidInput, "tolerances must be >= 0"};
    InterferenceReport rep;
    rep.parts = std::move(parts);
    const std::size_t n = rep.parts.size();
    auto sameTop = [&](std::size_t i, std::size_t j) {
        return !rep.parts[i].path.empty() && !rep.parts[j].path.empty() &&
               rep.parts[i].path[0] == rep.parts[j].path[0];
    };
    try {
        // Габариты, расширенные на допуск касания: касающиеся детали остаются кандидатами.
        std::vector<Bnd_Box> boxes(n);
        for (std::size_t i = 0; i < n; ++i) {
            BRepBndLib::AddOptimal(rep.parts[i].shape, boxes[i], false, false);
            boxes[i].Enlarge(std::max(options.contactTolerance, 1e-9));
        }
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                if (options.includeSameTopLevel || !sameTop(i, j)) ++rep.totalPairs;

        // Сортировка по xmin и проход: пара — кандидат, если отрезки по X и габариты пересекаются.
        std::vector<std::size_t> order(n);
        std::iota(order.begin(), order.end(), 0);
        std::vector<double> xmin(n), xmax(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (boxes[i].IsVoid()) {
                xmin[i] = 1.0;
                xmax[i] = 0.0;
                continue;
            }
            double y0, z0, x1, y1, z1;
            boxes[i].Get(xmin[i], y0, z0, x1, y1, z1);
            xmax[i] = x1;
        }
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return xmin[a] < xmin[b]; });
        std::vector<std::pair<std::size_t, std::size_t>> candidates;
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t i = order[k];
            if (boxes[i].IsVoid()) continue;
            for (std::size_t m = k + 1; m < n; ++m) {
                const std::size_t j = order[m];
                if (xmin[j] > xmax[i]) break;
                if (boxes[j].IsVoid() || boxes[i].IsOut(boxes[j])) continue;
                if (!options.includeSameTopLevel && sameTop(i, j)) continue;
                candidates.emplace_back(std::min(i, j), std::max(i, j));
            }
        }
        std::sort(candidates.begin(), candidates.end());
        rep.candidatePairs = candidates.size();

        // Для каждого кандидата (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 6):
        //  1) отбор по габаритам граней: если ни одна грань одной детали не подходит к габариту
        //     другой ближе допуска, границы далеко — пара либо вложена (точка внутри), либо пары нет;
        //  2) крупная деталь (больше kClipFaceCount граней, не меньше двух пар) один раз
        //     разрезается сеткой плоскостей на куски; для пары берутся куски, чей габарит
        //     пересекает габарит другой детали: A ∩ B лежит в них, а остальные куски дальше допуска;
        //  3) объём — сумма Common по парам кусков с пересекающимися габаритами (куски не
        //     перекрываются), касание — минимум BRepExtrema_DistShapeShape по тем же парам.
        const double faceEnlarge = std::max(options.contactTolerance, 1e-9);
        std::vector<PartFaces> faces(n);
        std::vector<std::size_t> pairsOf(n, 0);
        for (const auto& [i, j] : candidates) { ++pairsOf[i]; ++pairsOf[j]; }
        std::vector<std::optional<std::vector<Piece>>> grids(n);
        std::vector<Piece> whole(n);
        for (const auto& [i, j] : candidates) {
            const TopoDS_Shape& A = rep.parts[i].shape;
            const TopoDS_Shape& B = rep.parts[j].shape;
            prepareFaces(A, faceEnlarge, faces[i]);
            prepareFaces(B, faceEnlarge, faces[j]);
            if (faces[i].onlySolids && faces[j].onlySolids) {
                const bool aNearB = anyFaceTouches(faces[i], boxes[j]);
                const bool bNearA = anyFaceTouches(faces[j], boxes[i]);
                // Граница A далеко от B: B целиком внутри A или вне его (и дальше допуска).
                if (!aNearB && !pointInside(B, A)) continue;
                if (!bNearA && !pointInside(A, B)) continue;
            }

            // Куски обеих деталей рядом с другой деталью (мелкая деталь — один кусок, она сама).
            auto local = [&](std::size_t p, const Bnd_Box& other) {
                if (faces[p].count > kClipFaceCount && pairsOf[p] > 1 && !grids[p]) {
                    grids[p] = splitByGrid(rep.parts[p].shape, boxes[p], faces[p].count, faceEnlarge);
                }
                std::vector<const Piece*> out;
                if (grids[p] && !grids[p]->empty()) {
                    for (const Piece& pc : *grids[p])
                        if (!pc.box.IsOut(other)) out.push_back(&pc);
                } else {
                    whole[p] = Piece{rep.parts[p].shape, boxes[p]};
                    out.push_back(&whole[p]);
                }
                return out;
            };
            const auto pa = local(i, boxes[j]);
            const auto pb = local(j, boxes[i]);
            double volume = 0.0;
            std::vector<std::pair<const Piece*, const Piece*>> near;
            for (const Piece* x : pa)
                for (const Piece* y : pb) {
                    if (x->box.IsOut(y->box)) continue;  // дальше допуска: ни объёма, ни касания
                    near.emplace_back(x, y);
                    auto common = kernel::booleanOp(kernel::BooleanType::Common, x->shape, y->shape, options.boolean);
                    if (!common)
                        return Error{common.error().code, "intersection of '" + rep.parts[i].pathName + "' and '" +
                                                              rep.parts[j].pathName + "': " + common.error().message};
                    volume += common.value().volume;
                }
            if (volume > options.volumeTolerance) {
                rep.pairs.push_back({i, j, ContactKind::Interference, volume, 0.0});
                rep.totalVolume += volume;
                continue;
            }
            if (!options.reportTouching || near.empty()) continue;
            double distance = std::numeric_limits<double>::infinity();
            for (const auto& [x, y] : near) {
                BRepExtrema_DistShapeShape dist;
                dist.SetMultiThread(true);
                dist.LoadS1(x->shape);
                dist.LoadS2(y->shape);
                dist.Perform();
                if (!dist.IsDone())
                    return Error{ErrorCode::OperationFailed, "distance between '" + rep.parts[i].pathName + "' and '" +
                                                                 rep.parts[j].pathName + "' failed"};
                distance = std::min(distance, dist.Value());
            }
            if (distance <= options.contactTolerance)
                rep.pairs.push_back({i, j, ContactKind::Touching, std::max(volume, 0.0), distance});
        }
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception in interference check: ") + e.what()};
    }
    return rep;
}

}  // namespace bendcore::assembly_v2
