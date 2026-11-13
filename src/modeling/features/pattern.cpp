// Линейный и круговой массивы, зеркало операции и тела (docs/modeling/FEATURES.md, раздел 6).

#include "feature_common.hpp"

#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>

#include <cmath>

namespace bendcore::modeling {

using namespace detail;

namespace {

// Копии инструмента по преобразованиям и одна булева операция mode с target.
Result<FeatureResult> applyCopies(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                  const std::vector<gp_Trsf>& transforms) {
    if (std::string e = checkSolidInput(target, "target body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (std::string e = checkSolidInput(tool, "tool body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (transforms.empty()) return Error{ErrorCode::InvalidInput, "pattern has no instances besides the seed"};
    try {
        std::vector<TopoDS_Shape> copies;
        std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> pairs;
        BRep_Builder bb;
        TopoDS_Compound compound;
        bb.MakeCompound(compound);
        for (const auto& t : transforms) {
            auto c = transformCopy(tool, t);
            if (!c) return c.error();
            copies.push_back(c.value().shape);
            bb.Add(compound, c.value().shape);
            pairs.insert(pairs.end(), c.value().pairs.begin(), c.value().pairs.end());
        }
        if (mode == FeatureMode::NewBody || countSolids(target) > 1) {
            // Копии — новые тела или многотельная цель: applyTool (тела не сливаются между собой).
            auto applied = applyTool(mode, target, compound, History(), {});
            if (!applied) {
                if (mode == FeatureMode::Cut && applied.error().message.find("does not intersect") != std::string::npos)
                    return Error{ErrorCode::OperationFailed, "pattern instances do not intersect the target body"};
                return applied.error();
            }
            FeatureResult out = std::move(applied).value();
            addCopyGenerations(out.history, pairs, out.shape, out.history);
            return out;
        }
        const kernel::BooleanType type = mode == FeatureMode::Boss ? kernel::BooleanType::Fuse : kernel::BooleanType::Cut;
        auto step = booleanWithHistory(type, {target}, copies, true, true);
        if (!step) return step.error();
        BooleanStep& st = step.value();
        if (std::string pr = checkSolidResult(st.shape); !pr.empty()) return Error{ErrorCode::OperationFailed, pr};
        if (mode == FeatureMode::Cut) {
            const double vt = volumeOf(target), vr = volumeOf(st.shape);
            if (std::abs(vt - vr) <= 1e-9 * std::max(1.0, vt))
                return Error{ErrorCode::OperationFailed, "pattern instances do not intersect the target body"};
        }
        FeatureResult out;
        out.shape = st.shape;
        out.tool = compound;
        out.history = st.history;
        out.warnings = st.warnings;
        addCopyGenerations(out.history, pairs, out.shape, st.history);
        const int solids = countSolids(out.shape);
        if (solids > 1) out.warnings.push_back("result has " + std::to_string(solids) + " solids");
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("pattern", e.what())};
    }
}

bool finiteNonZero(double v) { return std::isfinite(v) && v != 0.0; }

}  // namespace

Result<FeatureResult> linearPattern(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                    const LinearPatternParams& p) {
    if (p.count1 < 1 || p.count2 < 1) return Error{ErrorCode::InvalidInput, "pattern counts must be >= 1"};
    if (p.count1 > 1 && !finiteNonZero(p.spacing1)) return Error{ErrorCode::InvalidInput, "spacing1 must be non-zero"};
    const int n2 = p.direction2 ? p.count2 : 1;
    if (n2 > 1 && !finiteNonZero(p.spacing2)) return Error{ErrorCode::InvalidInput, "spacing2 must be non-zero"};
    if (p.direction2 && p.direction1.IsParallel(*p.direction2, 1e-9))
        return Error{ErrorCode::InvalidInput, "pattern directions must not be parallel"};
    std::vector<gp_Trsf> transforms;
    for (int j = 0; j < n2; ++j)
        for (int i = 0; i < p.count1; ++i) {
            if (i == 0 && j == 0) continue;
            gp_Vec v = gp_Vec(p.direction1) * (p.spacing1 * i);
            if (p.direction2) v += gp_Vec(*p.direction2) * (p.spacing2 * j);
            gp_Trsf t;
            t.SetTranslation(v);
            transforms.push_back(t);
        }
    return applyCopies(target, tool, mode, transforms);
}

Result<FeatureResult> circularPattern(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                      const CircularPatternParams& p) {
    if (p.count < 2) return Error{ErrorCode::InvalidInput, "circular pattern count must be >= 2"};
    if (!finiteNonZero(p.angle)) return Error{ErrorCode::InvalidInput, "circular pattern angle must be non-zero"};
    double step = p.angle;
    if (p.equalSpacing) {
        if (std::abs(p.angle) > 2.0 * M_PI + 1e-12)
            return Error{ErrorCode::InvalidInput, "total pattern angle must not exceed 2*pi"};
        step = std::abs(std::abs(p.angle) - 2.0 * M_PI) < 1e-12 ? p.angle / p.count : p.angle / (p.count - 1);
    } else if (std::abs(step) * (p.count - 1) >= 2.0 * M_PI - 1e-12) {
        return Error{ErrorCode::InvalidInput, "circular pattern instances overlap after a full turn"};
    }
    std::vector<gp_Trsf> transforms;
    for (int i = 1; i < p.count; ++i) {
        gp_Trsf t;
        t.SetRotation(p.axis, step * i);
        transforms.push_back(t);
    }
    return applyCopies(target, tool, mode, transforms);
}

Result<FeatureResult> mirrorFeature(const TopoDS_Shape& target, const TopoDS_Shape& tool, FeatureMode mode,
                                    const gp_Ax2& plane) {
    gp_Trsf t;
    t.SetMirror(plane);
    return applyCopies(target, tool, mode, {t});
}

Result<FeatureResult> mirrorBody(const TopoDS_Shape& body, const gp_Ax2& plane, bool merge) {
    if (std::string e = checkSolidInput(body, "body"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    try {
        gp_Trsf t;
        t.SetMirror(plane);
        auto copy = transformCopy(body, t);
        if (!copy) return copy.error();
        FeatureResult out;
        if (!merge) {
            BRep_Builder bb;
            TopoDS_Compound compound;
            bb.MakeCompound(compound);
            bb.Add(compound, body);
            bb.Add(compound, copy.value().shape);
            if (std::string pr = checkSolidResult(compound); !pr.empty()) return Error{ErrorCode::OperationFailed, pr};
            out.shape = compound;
            out.history = newHistory();
            for (const auto& [orig, cp] : copy.value().pairs) out.history->AddGenerated(orig, cp);
            return out;
        }
        auto step = booleanWithHistory(kernel::BooleanType::Fuse, {body}, {copy.value().shape}, true, true);
        if (!step) return step.error();
        if (std::string pr = checkSolidResult(step.value().shape); !pr.empty())
            return Error{ErrorCode::OperationFailed, pr};
        out.shape = step.value().shape;
        out.history = step.value().history;
        out.warnings = step.value().warnings;
        addCopyGenerations(out.history, copy.value().pairs, out.shape, step.value().history);
        const int solids = countSolids(out.shape);
        if (solids > 1) out.warnings.push_back("mirrored body does not touch the original: " + std::to_string(solids) +
                                               " solids");
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("mirrorBody", e.what())};
    }
}

}  // namespace bendcore::modeling
