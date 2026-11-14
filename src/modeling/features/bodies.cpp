// Тела многотельной детали: булевы операции между телами, удаление, копии (массив и зеркало
// тел) (docs/modeling/FEATURES.md, раздел «Многотельная деталь»).

#include "feature_common.hpp"

#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <cmath>

namespace bendcore::modeling {

using namespace detail;

namespace {

bool sameAs(const TopoDS_Shape& a, const TopoDS_Shape& b) { return a.IsSame(b); }

std::optional<std::size_t> indexIn(const std::vector<TopoDS_Shape>& list, const TopoDS_Shape& s) {
    for (std::size_t i = 0; i < list.size(); ++i)
        if (sameAs(list[i], s)) return i;
    return std::nullopt;
}

// Тело из списка тел детали: сама форма — тело (solid) или составное из одного тела.
Result<TopoDS_Shape> bodyOf(const std::vector<TopoDS_Shape>& bodies, const TopoDS_Shape& s, const char* role) {
    if (s.IsNull()) return Error{ErrorCode::InvalidInput, std::string(role) + " is null"};
    std::vector<TopoDS_Shape> solids = solidsOf(s);
    if (solids.size() != 1) return Error{ErrorCode::InvalidInput, std::string(role) + " must be one solid"};
    auto i = indexIn(bodies, solids.front());
    if (!i) return Error{ErrorCode::InvalidInput, std::string(role) + " is not a body of the part"};
    return bodies[*i];
}

TopoDS_Shape compoundOf(const std::vector<TopoDS_Shape>& parts) {
    TopoDS_Compound c;
    BRep_Builder b;
    b.MakeCompound(c);
    for (const auto& s : parts) b.Add(c, s);
    return c;
}

Result<FeatureResult> finish(FeatureResult out, const std::vector<TopoDS_Shape>& parts) {
    out.shape = compoundOf(parts);
    double v = 0.0;
    for (const auto& s : parts) v += volumeOf(s);
    rememberFacts(out.shape, ShapeFacts{false, false, v, std::nullopt});
    if (std::string p = checkSolidResult(out.shape); !p.empty()) return Error{ErrorCode::OperationFailed, p};
    return out;
}

}  // namespace

Result<FeatureResult> bodyBoolean(const TopoDS_Shape& bodiesShape, const TopoDS_Shape& target,
                                  const std::vector<TopoDS_Shape>& tools, BodyBooleanType type, bool keepTools) {
    if (std::string e = checkSolidInput(bodiesShape, "part"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (tools.empty()) return Error{ErrorCode::InvalidInput, "body boolean needs at least one tool body"};
    try {
        const std::vector<TopoDS_Shape> bodies = solidsOf(bodiesShape);
        auto t = bodyOf(bodies, target, "target body");
        if (!t) return t.error();
        std::vector<TopoDS_Shape> toolBodies;
        for (std::size_t i = 0; i < tools.size(); ++i) {
            auto b = bodyOf(bodies, tools[i], "tool body");
            if (!b) return Error{ErrorCode::InvalidInput, "tool #" + std::to_string(i) + ": " + b.error().message};
            if (sameAs(b.value(), t.value()))
                return Error{ErrorCode::InvalidInput, "tool #" + std::to_string(i) + " is the target body"};
            if (indexIn(toolBodies, b.value()))
                return Error{ErrorCode::InvalidInput, "tool #" + std::to_string(i) + " is listed twice"};
            toolBodies.push_back(b.value());
        }
        // Оставляемые инструменты: в булеву операцию идут их копии, чтобы элементы тел-инструментов
        // перешли в результат без изменений, а грани, внесённые ими в цель, были Generated.
        std::vector<TopoDS_Shape> operands = toolBodies;
        std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> pairs;
        if (keepTools) {
            operands.clear();
            for (const auto& b : toolBodies) {
                auto c = transformCopy(b, gp_Trsf());
                if (!c) return c.error();
                operands.push_back(c.value().shape);
                pairs.insert(pairs.end(), c.value().pairs.begin(), c.value().pairs.end());
            }
        }
        const kernel::BooleanType op = type == BodyBooleanType::Union      ? kernel::BooleanType::Fuse
                                       : type == BodyBooleanType::Subtract ? kernel::BooleanType::Cut
                                                                           : kernel::BooleanType::Common;
        // Копии инструментов отслеживаются: по их истории грани, внесённые в цель, становятся
        // Generated из граней оставляемых тел; сами копии в имена не входят.
        auto step = booleanWithHistory(op, {t.value()}, operands, true, true);
        if (!step) return step.error();
        const std::vector<TopoDS_Shape> produced = solidsOf(step.value().shape);
        if (produced.empty()) {
            return Error{ErrorCode::OperationFailed, type == BodyBooleanType::Intersect
                                                         ? "the bodies do not intersect"
                                                         : "the operation removes the whole target body"};
        }
        if (type == BodyBooleanType::Subtract) {
            const double before = volumeOf(t.value()), after = volumeOf(step.value().shape);
            if (std::abs(before - after) <= 1e-9 * std::max(1.0, before))
                return Error{ErrorCode::OperationFailed, "the tool bodies do not intersect the target body"};
        }
        FeatureResult out;
        out.history = step.value().history;
        out.warnings = step.value().warnings;
        std::vector<TopoDS_Shape> parts;
        for (const auto& b : bodies)
            if (!sameAs(b, t.value()) && (keepTools || !indexIn(toolBodies, b))) parts.push_back(b);
        for (const auto& s : produced) {
            rememberFacts(s, ShapeFacts{true, step.value().fuzzyValue == 0.0, std::nullopt, std::nullopt});
            parts.push_back(s);
        }
        if (keepTools) {
            // Generated(элемент инструмента) → образ элемента копии в результате.
            History gen = newHistory();
            addCopyGenerations(gen, pairs, compoundOf(produced), step.value().history);
            mergeInto(out.history, gen);
        }
        if (type == BodyBooleanType::Union && produced.size() > 1)
            out.warnings.push_back("union: " + std::to_string(produced.size()) + " bodies do not touch");
        return finish(std::move(out), parts);
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("bodyBoolean", e.what())};
    }
}

Result<FeatureResult> deleteBodies(const TopoDS_Shape& bodiesShape, const std::vector<TopoDS_Shape>& removed) {
    if (std::string e = checkSolidInput(bodiesShape, "part"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (removed.empty()) return Error{ErrorCode::InvalidInput, "no body to delete"};
    try {
        const std::vector<TopoDS_Shape> bodies = solidsOf(bodiesShape);
        std::vector<TopoDS_Shape> gone;
        for (std::size_t i = 0; i < removed.size(); ++i) {
            auto b = bodyOf(bodies, removed[i], "deleted body");
            if (!b) return Error{ErrorCode::InvalidInput, "body #" + std::to_string(i) + ": " + b.error().message};
            if (!indexIn(gone, b.value())) gone.push_back(b.value());
        }
        std::vector<TopoDS_Shape> parts;
        for (const auto& b : bodies)
            if (!indexIn(gone, b)) parts.push_back(b);
        if (parts.empty()) return Error{ErrorCode::InvalidInput, "cannot delete every body of the part"};
        FeatureResult out;
        out.history = newHistory();
        ShapeMap keep;
        for (const auto& p : parts) TopExp::MapShapes(p, keep);
        for (const auto& g : gone) {
            ShapeMap m;
            TopExp::MapShapes(g, m);
            for (int i = 1; i <= m.Extent(); ++i)
                if (BRepTools_History::IsSupportedType(m(i)) && !keep.Contains(m(i))) out.history->Remove(m(i));
        }
        return finish(std::move(out), parts);
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("deleteBodies", e.what())};
    }
}

Result<FeatureResult> copyBodies(const TopoDS_Shape& bodiesShape, const std::vector<TopoDS_Shape>& selected,
                                 const std::vector<gp_Trsf>& transforms, bool merge) {
    if (std::string e = checkSolidInput(bodiesShape, "part"); !e.empty()) return Error{ErrorCode::InvalidInput, e};
    if (selected.empty()) return Error{ErrorCode::InvalidInput, "no body to copy"};
    if (transforms.empty()) return Error{ErrorCode::InvalidInput, "no copies requested"};
    try {
        const std::vector<TopoDS_Shape> bodies = solidsOf(bodiesShape);
        std::vector<TopoDS_Shape> sources;
        for (std::size_t i = 0; i < selected.size(); ++i) {
            auto b = bodyOf(bodies, selected[i], "copied body");
            if (!b) return Error{ErrorCode::InvalidInput, "body #" + std::to_string(i) + ": " + b.error().message};
            if (!indexIn(sources, b.value())) sources.push_back(b.value());
        }
        FeatureResult out;
        out.history = newHistory();
        std::vector<TopoDS_Shape> parts;
        for (const auto& b : bodies)
            if (!merge || !indexIn(sources, b)) parts.push_back(b);
        std::vector<TopoDS_Shape> tools;
        for (const auto& src : sources) {
            std::vector<TopoDS_Shape> copies;
            std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> pairs;
            for (const auto& t : transforms) {
                auto c = transformCopy(src, t);
                if (!c) return c.error();
                copies.push_back(c.value().shape);
                pairs.insert(pairs.end(), c.value().pairs.begin(), c.value().pairs.end());
            }
            tools.insert(tools.end(), copies.begin(), copies.end());
            if (!merge) {
                parts.insert(parts.end(), copies.begin(), copies.end());
                for (const auto& [orig, cp] : pairs) out.history->AddGenerated(orig, cp);
                continue;
            }
            auto step = booleanWithHistory(kernel::BooleanType::Fuse, {src}, copies, true, true);
            if (!step) return step.error();
            const std::vector<TopoDS_Shape> produced = solidsOf(step.value().shape);
            for (const auto& s : produced) {
                rememberFacts(s, ShapeFacts{true, step.value().fuzzyValue == 0.0, std::nullopt, std::nullopt});
                parts.push_back(s);
            }
            if (produced.size() > 1)
                out.warnings.push_back("merged copies do not touch the body: " + std::to_string(produced.size()) +
                                       " solids");
            ShapeMap srcMap;
            TopExp::MapShapes(src, srcMap);
            History filtered = newHistory();
            // История слияния для элементов исходного тела; копии — только как Generated.
            for (int i = 1; i <= srcMap.Extent(); ++i) {
                const TopoDS_Shape& s = srcMap(i);
                if (!BRepTools_History::IsSupportedType(s)) continue;
                for (const TopoDS_Shape& m : step.value().history->Modified(s)) filtered->AddModified(s, m);
                for (const TopoDS_Shape& g : step.value().history->Generated(s)) filtered->AddGenerated(s, g);
                if (step.value().history->IsRemoved(s)) filtered->Remove(s);
            }
            addCopyGenerations(filtered, pairs, step.value().shape, step.value().history);
            mergeInto(out.history, filtered);
            out.warnings.insert(out.warnings.end(), step.value().warnings.begin(), step.value().warnings.end());
        }
        out.tool = compoundOf(tools);
        return finish(std::move(out), parts);
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("copyBodies", e.what())};
    }
}

}  // namespace bendcore::modeling
