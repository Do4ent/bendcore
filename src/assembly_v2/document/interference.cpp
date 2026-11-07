// Проверка пересечений (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 6).

#include <bendcore/assembly_v2/interference.hpp>

#include <BRepBndLib.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Bnd_Box.hxx>
#include <Standard_Failure.hxx>

#include <algorithm>
#include <cmath>
#include <numeric>

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

        for (const auto& [i, j] : candidates) {
            auto common = kernel::booleanOp(kernel::BooleanType::Common, rep.parts[i].shape, rep.parts[j].shape,
                                            options.boolean);
            if (!common)
                return Error{common.error().code, "intersection of '" + rep.parts[i].pathName + "' and '" +
                                                      rep.parts[j].pathName + "': " + common.error().message};
            const double volume = common.value().volume;
            if (volume > options.volumeTolerance) {
                rep.pairs.push_back({i, j, ContactKind::Interference, volume, 0.0});
                rep.totalVolume += volume;
                continue;
            }
            if (!options.reportTouching) continue;
            BRepExtrema_DistShapeShape dist(rep.parts[i].shape, rep.parts[j].shape);
            if (!dist.IsDone())
                return Error{ErrorCode::OperationFailed, "distance between '" + rep.parts[i].pathName + "' and '" +
                                                             rep.parts[j].pathName + "' failed"};
            if (dist.Value() <= options.contactTolerance)
                rep.pairs.push_back({i, j, ContactKind::Touching, std::max(volume, 0.0), dist.Value()});
        }
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception in interference check: ") + e.what()};
    }
    return rep;
}

}  // namespace bendcore::assembly_v2
