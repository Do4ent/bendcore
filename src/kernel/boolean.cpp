#include <bendcore/kernel/boolean.hpp>

#include <BOPAlgo_CheckResult.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Check.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools_History.hxx>
#include <GProp_GProps.hxx>
#include <Message_Alert.hxx>
#include <Message_Report.hxx>
#include <NCollection_List.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>

namespace bendcore::kernel {

namespace {

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

std::unique_ptr<BRepAlgoAPI_BooleanOperation> makeOperation(BooleanType type) {
    switch (type) {
        case BooleanType::Fuse: return std::make_unique<BRepAlgoAPI_Fuse>();
        case BooleanType::Cut: return std::make_unique<BRepAlgoAPI_Cut>();
        case BooleanType::Common: return std::make_unique<BRepAlgoAPI_Common>();
    }
    return nullptr;
}

std::string alertsText(const occ::handle<Message_Report>& report, Message_Gravity gravity) {
    std::string out;
    for (const auto& alert : report->GetAlerts(gravity)) {
        if (!out.empty()) out += "; ";
        out += alert->GetMessageKey();
    }
    return out;
}

// Независимая копия истории (слияние в пустую историю переносит Modified, Generated, Removed).
occ::handle<BRepTools_History> copyHistory(const occ::handle<BRepTools_History>& h) {
    if (h.IsNull()) return {};
    occ::handle<BRepTools_History> copy = new BRepTools_History();
    copy->Merge(h);
    return copy;
}

bool hasSolids(const TopoDS_Shape& s) { return TopExp_Explorer(s, TopAbs_SOLID).More(); }

int countSolids(const TopoDS_Shape& s) {
    int n = 0;
    for (TopExp_Explorer ex(s, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

struct MassInfo {
    double volume = 0.0;
    double area = 0.0;
};

MassInfo massOf(const TopoDS_Shape& s) {
    MassInfo m;
    if (s.IsNull()) return m;
    if (hasSolids(s)) {
        GProp_GProps vp;
        BRepGProp::VolumeProperties(s, vp);
        m.volume = vp.Mass();
    }
    GProp_GProps sp;
    BRepGProp::SurfaceProperties(s, sp);
    m.area = sp.Mass();
    return m;
}

std::string checkInput(const TopoDS_Shape& s, const char* role, std::size_t index) {
    BRepAlgoAPI_Check check(s, true, true);
    if (check.IsValid()) return {};
    std::ostringstream msg;
    msg << role << " #" << index << " failed BRepAlgoAPI_Check:";
    for (const BOPAlgo_CheckResult& r : check.Result()) msg << ' ' << checkStatusName(r.GetCheckStatus());
    return msg.str();
}

// Returns an empty string if the attempt result is acceptable, otherwise the reason.
std::string judge(BooleanType type, const TopoDS_Shape& result, const BooleanOptions& opt,
                  const std::vector<MassInfo>& args, const std::vector<MassInfo>& tools, bool solidInputs,
                  double fuzzy, double& volumeOut) {
    if (result.IsNull()) return "result is null";
    if (!BRepCheck_Analyzer(result).IsValid()) return "result is invalid (BRepCheck_Analyzer)";
    const bool empty = !TopExp_Explorer(result, TopAbs_FACE).More();
    if (type == BooleanType::Fuse && empty) return "fuse result is empty";
    const MassInfo mass = massOf(result);
    volumeOut = mass.volume;

    if (opt.checkVolumeBounds && solidInputs) {
        double va = 0.0, vt = 0.0, aa = 0.0, at = 0.0, vaMax = 0.0, vtMax = 0.0;
        for (const auto& m : args) { va += m.volume; aa += m.area; vaMax = std::max(vaMax, m.volume); }
        for (const auto& m : tools) { vt += m.volume; at += m.area; vtMax = std::max(vtMax, m.volume); }
        // Fuzzy merging may move boundaries by up to the fuzzy value plus element tolerances.
        const double slack = (fuzzy + 1e-6) * (aa + at) + 1e-9 * (va + vt);
        double lo = 0.0, hi = 0.0;
        switch (type) {
            case BooleanType::Fuse: lo = std::max(vaMax, vtMax); hi = va + vt; break;
            case BooleanType::Cut: lo = std::max(0.0, va - vt); hi = va; break;
            case BooleanType::Common: lo = 0.0; hi = std::min(va, vt); break;
        }
        if (mass.volume < lo - slack || mass.volume > hi + slack) {
            std::ostringstream msg;
            msg << "volume " << mass.volume << " outside [" << lo << ", " << hi << "] +- " << slack;
            return msg.str();
        }
    }
    if (opt.expectedSolidCount) {
        const int n = countSolids(result);
        if (n != *opt.expectedSolidCount) {
            std::ostringstream msg;
            msg << "result has " << n << " solids, expected " << *opt.expectedSolidCount;
            return msg.str();
        }
    }
    if (opt.minSolidThickness > 0.0) {
        for (TopExp_Explorer ex(result, TopAbs_SOLID); ex.More(); ex.Next()) {
            const MassInfo m = massOf(ex.Current());
            const double thickness = m.area > 0.0 ? 2.0 * m.volume / m.area : 0.0;
            if (thickness < opt.minSolidThickness) {
                std::ostringstream msg;
                msg << "result contains a sliver solid (2V/A = " << thickness << " < " << opt.minSolidThickness << ")";
                return msg.str();
            }
        }
    }
    return {};
}

std::optional<Error> validateOptions(const BooleanOptions& o) {
    if (!(o.maxFuzzyValue >= 0.0) || !std::isfinite(o.maxFuzzyValue))
        return Error{ErrorCode::InvalidInput, "maxFuzzyValue must be finite and >= 0"};
    if (o.fuzzyLadder.empty()) {
        if (!(o.fuzzyStart > 0.0) || !std::isfinite(o.fuzzyStart))
            return Error{ErrorCode::InvalidInput, "fuzzyStart must be positive"};
        if (!(o.fuzzyFactor > 1.0) || !std::isfinite(o.fuzzyFactor))
            return Error{ErrorCode::InvalidInput, "fuzzyFactor must be > 1"};
    }
    for (double v : o.fuzzyLadder)
        if (!(v >= 0.0) || !std::isfinite(v)) return Error{ErrorCode::InvalidInput, "fuzzyLadder values must be >= 0"};
    if (o.expectedSolidCount && *o.expectedSolidCount < 0)
        return Error{ErrorCode::InvalidInput, "expectedSolidCount must be >= 0"};
    if (!(o.minSolidThickness >= 0.0)) return Error{ErrorCode::InvalidInput, "minSolidThickness must be >= 0"};
    return std::nullopt;
}

}  // namespace

const char* toString(BooleanType type) noexcept {
    switch (type) {
        case BooleanType::Fuse: return "Fuse";
        case BooleanType::Cut: return "Cut";
        case BooleanType::Common: return "Common";
    }
    return "Unknown";
}

std::vector<double> fuzzyLadder(const BooleanOptions& o) {
    std::vector<double> ladder;
    if (!o.fuzzyLadder.empty()) {
        for (double v : o.fuzzyLadder)
            if (v >= 0.0 && v <= o.maxFuzzyValue) ladder.push_back(v);
        std::sort(ladder.begin(), ladder.end());
        ladder.erase(std::unique(ladder.begin(), ladder.end()), ladder.end());
        return ladder;
    }
    ladder.push_back(0.0);
    if (!(o.fuzzyStart > 0.0) || !(o.fuzzyFactor > 1.0)) return ladder;
    for (double v = o.fuzzyStart; v < o.maxFuzzyValue * (1.0 - 1e-12); v *= o.fuzzyFactor) ladder.push_back(v);
    if (o.maxFuzzyValue > 0.0) ladder.push_back(o.maxFuzzyValue);
    return ladder;
}

Result<BooleanResult> booleanOp(BooleanType type, const TopoDS_Shape& a, const TopoDS_Shape& b,
                                const BooleanOptions& options) {
    return booleanOp(type, std::vector<TopoDS_Shape>{a}, std::vector<TopoDS_Shape>{b}, options);
}

Result<BooleanResult> booleanOp(BooleanType type, const std::vector<TopoDS_Shape>& arguments,
                                const std::vector<TopoDS_Shape>& tools, const BooleanOptions& options) {
    if (arguments.empty()) return Error{ErrorCode::InvalidInput, "no arguments"};
    if (tools.empty()) return Error{ErrorCode::InvalidInput, "no tools"};
    if (auto err = validateOptions(options)) return *err;
    for (std::size_t i = 0; i < arguments.size(); ++i)
        if (arguments[i].IsNull() || !TopExp_Explorer(arguments[i], TopAbs_EDGE).More())
            return Error{ErrorCode::InvalidInput, "argument #" + std::to_string(i) + " is null or empty"};
    for (std::size_t i = 0; i < tools.size(); ++i)
        if (tools[i].IsNull() || !TopExp_Explorer(tools[i], TopAbs_EDGE).More())
            return Error{ErrorCode::InvalidInput, "tool #" + std::to_string(i) + " is null or empty"};

    try {
        if (options.checkInputs) {
            std::string problems;
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                const std::string p = checkInput(arguments[i], "argument", i);
                if (!p.empty()) problems += (problems.empty() ? "" : "; ") + p;
            }
            for (std::size_t i = 0; i < tools.size(); ++i) {
                const std::string p = checkInput(tools[i], "tool", i);
                if (!p.empty()) problems += (problems.empty() ? "" : "; ") + p;
            }
            if (!problems.empty()) return Error{ErrorCode::InvalidInput, problems};
        }

        NCollection_List<TopoDS_Shape> argList, toolList;
        std::vector<MassInfo> argMass, toolMass;
        bool solidInputs = true;
        for (const auto& s : arguments) {
            argList.Append(s);
            argMass.push_back(massOf(s));
            solidInputs = solidInputs && hasSolids(s);
        }
        for (const auto& s : tools) {
            toolList.Append(s);
            toolMass.push_back(massOf(s));
            solidInputs = solidInputs && hasSolids(s);
        }

        BooleanResult out;
        std::vector<std::string> failures;
        for (double fuzzy : fuzzyLadder(options)) {
            ++out.attempts;
            std::ostringstream tag;
            tag << "fuzzy " << fuzzy << ": ";
            auto op = makeOperation(type);
            op->SetArguments(argList);
            op->SetTools(toolList);
            op->SetFuzzyValue(fuzzy);
            op->SetRunParallel(options.runParallel);
            op->SetNonDestructive(true);
            op->SetToFillHistory(true);
            try {
                op->Build();
            } catch (const Standard_Failure& e) {
                failures.push_back(tag.str() + "OCCT exception: " + e.what());
                continue;
            }
            if (op->HasErrors() || !op->IsDone()) {
                failures.push_back(tag.str() + "errors: " + alertsText(op->GetReport(), Message_Fail));
                continue;
            }
            std::string warnings;
            if (op->HasWarnings()) warnings = alertsText(op->GetReport(), Message_Warning);

            double volume = 0.0;
            TopoDS_Shape shape = op->Shape();
            // SimplifyResult объединяет историю упрощения с историей операции на месте
            // (myHistory->Merge), поэтому история до упрощения сохраняется копией на случай,
            // если упрощённый результат будет отвергнут.
            occ::handle<BRepTools_History> history = copyHistory(op->History());
            std::string reason = judge(type, shape, options, argMass, toolMass, solidInputs, fuzzy, volume);
            if (!reason.empty()) {
                failures.push_back(tag.str() + reason + (warnings.empty() ? "" : " [warnings: " + warnings + "]"));
                continue;
            }

            if (options.simplifyResult) {
                op->SimplifyResult(true, true);
                TopoDS_Shape unified = op->Shape();
                double unifiedVolume = 0.0;
                const std::string sreason =
                    judge(type, unified, options, argMass, toolMass, solidInputs, fuzzy, unifiedVolume);
                if (sreason.empty()) {
                    shape = unified;
                    volume = unifiedVolume;
                    history = op->History();
                    out.simplifyApplied = true;
                } else {
                    out.warnings.push_back(tag.str() + "SimplifyResult rejected: " + sreason);
                }
            }

            out.warnings.insert(out.warnings.begin(), failures.begin(), failures.end());
            if (!warnings.empty()) out.warnings.push_back(tag.str() + "OCCT warnings: " + warnings);
            out.shape = shape;
            out.fuzzyValue = fuzzy;
            out.volume = volume;
            if (TopExp_Explorer(shape, TopAbs_FACE).More()) {
                if (history.IsNull())
                    return Error{ErrorCode::OperationFailed,
                                 std::string(toString(type)) + ": OCCT returned no history for the result"};
                out.history = history;
            }
            return out;
        }

        std::string message = std::string(toString(type)) + " failed for every fuzzy value:";
        for (const auto& f : failures) message += "\n  " + f;
        return Error{ErrorCode::OperationFailed, message};
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception in booleanOp: ") + e.what()};
    } catch (const std::exception& e) {
        return Error{ErrorCode::OperationFailed, std::string("exception in booleanOp: ") + e.what()};
    }
}

}  // namespace bendcore::kernel
