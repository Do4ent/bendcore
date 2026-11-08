// Отмена и повтор изменений документа сборки снимками состояния
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 11).

#include <bendcore/assembly_v2/history.hpp>

#include <utility>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

AssemblyHistory::AssemblyHistory(AssemblyDocument document, HistoryOptions options)
    : doc_(std::move(document)), options_(options) {}

void AssemblyHistory::record(std::string label, AssemblyDocument before) {
    undo_.push_back({std::move(label), std::move(before)});
    if (undo_.size() > options_.maxDepth) undo_.erase(undo_.begin());
    redo_.clear();
}

Status AssemblyHistory::solveNow() {
    auto r = doc_.solve(options_.solver);
    if (!r) return r.error();
    return std::monostate{};
}

Status AssemblyHistory::apply(std::string label, const std::function<Status(AssemblyDocument&)>& edit, bool solve) {
    if (!edit) return Status::failure(ErrorCode::InvalidInput, "edit function is empty");
    AssemblyDocument before = doc_;
    Status s = edit(doc_);
    if (s && solve) s = solveNow();
    if (!s) {
        doc_ = std::move(before);
        return s;
    }
    record(std::move(label), std::move(before));
    return s;
}

Result<OccurrenceId> AssemblyHistory::insertComponent(DefinitionId definition, const Transform& placement,
                                                      std::string name, std::optional<DefinitionId> parent) {
    OccurrenceId id = 0;
    auto s = apply(
        "Insert component",
        [&](AssemblyDocument& d) -> Status {
            auto r = d.addOccurrence(definition, placement, std::move(name), parent);
            if (!r) return r.error();
            id = r.value();
            return std::monostate{};
        },
        options_.solveAfterEdit);
    if (!s) return s.error();
    return id;
}

Status AssemblyHistory::removeComponent(OccurrenceId id) {
    return apply("Remove component", [&](AssemblyDocument& d) { return d.removeOccurrence(id); }, options_.solveAfterEdit);
}

Result<ReplaceReport> AssemblyHistory::replaceComponent(OccurrenceId id, DefinitionId definition) {
    ReplaceReport report;
    auto s = apply(
        "Replace component",
        [&](AssemblyDocument& d) -> Status {
            auto r = d.replaceOccurrenceDefinition(id, definition);
            if (!r) return r.error();
            report = std::move(r).value();
            return std::monostate{};
        },
        options_.solveAfterEdit);
    if (!s) return s.error();
    return report;
}

Status AssemblyHistory::moveComponent(OccurrenceId id, const Transform& placement) {
    return apply("Move component", [&](AssemblyDocument& d) { return d.setPlacement(id, placement); });
}

Status AssemblyHistory::setFixed(OccurrenceId id, bool fixed) {
    return apply(fixed ? "Fix component" : "Float component", [&](AssemblyDocument& d) { return d.setFixed(id, fixed); },
                 options_.solveAfterEdit);
}

Status AssemblyHistory::setComponentSuppressed(OccurrenceId id, bool suppressed) {
    return apply(suppressed ? "Suppress component" : "Unsuppress component",
                 [&](AssemblyDocument& d) { return d.setOccurrenceSuppressed(id, suppressed); }, options_.solveAfterEdit);
}

Result<MateId> AssemblyHistory::addMate(MateSpec spec) {
    MateId id = 0;
    auto s = apply(
        "Add mate",
        [&](AssemblyDocument& d) -> Status {
            auto r = d.addMate(std::move(spec));
            if (!r) return r.error();
            id = r.value();
            return std::monostate{};
        },
        options_.solveAfterEdit);
    if (!s) return s.error();
    return id;
}

Status AssemblyHistory::editMate(MateId id, MateSpec spec) {
    return apply("Edit mate", [&](AssemblyDocument& d) { return d.editMate(id, std::move(spec)); }, options_.solveAfterEdit);
}

Status AssemblyHistory::removeMate(MateId id) {
    return apply("Remove mate", [&](AssemblyDocument& d) { return d.removeMate(id); }, options_.solveAfterEdit);
}

Status AssemblyHistory::setMateSuppressed(MateId id, bool suppressed) {
    return apply(suppressed ? "Suppress mate" : "Unsuppress mate",
                 [&](AssemblyDocument& d) { return d.setMateSuppressed(id, suppressed); }, options_.solveAfterEdit);
}

namespace {

template <typename Spec, typename Add>
Result<PatternId> addPatternStep(AssemblyHistory& h, const char* label, const Spec& spec, Add add, bool solve) {
    PatternId id = 0;
    auto s = h.apply(
        label,
        [&](AssemblyDocument& d) -> Status {
            auto r = add(d, spec);
            if (!r) return r.error();
            id = r.value();
            return std::monostate{};
        },
        solve);
    if (!s) return s.error();
    return id;
}

}  // namespace

Result<PatternId> AssemblyHistory::addLinearPattern(const LinearPatternSpec& spec) {
    return addPatternStep(*this, "Linear pattern", spec,
                          [](AssemblyDocument& d, const LinearPatternSpec& s) { return d.addLinearPattern(s); },
                          options_.solveAfterEdit);
}

Result<PatternId> AssemblyHistory::addCircularPattern(const CircularPatternSpec& spec) {
    return addPatternStep(*this, "Circular pattern", spec,
                          [](AssemblyDocument& d, const CircularPatternSpec& s) { return d.addCircularPattern(s); },
                          options_.solveAfterEdit);
}

Result<PatternId> AssemblyHistory::addFeaturePattern(const FeaturePatternSpec& spec) {
    return addPatternStep(*this, "Feature pattern", spec,
                          [](AssemblyDocument& d, const FeaturePatternSpec& s) { return d.addFeaturePattern(s); },
                          options_.solveAfterEdit);
}

Status AssemblyHistory::removePattern(PatternId id) {
    return apply("Remove pattern", [&](AssemblyDocument& d) { return d.removePattern(id); }, options_.solveAfterEdit);
}

Result<DocumentSolveReport> AssemblyHistory::solve() {
    DocumentSolveReport report;
    auto s = apply("Solve", [&](AssemblyDocument& d) -> Status {
        auto r = d.solve(options_.solver);
        if (!r) return r.error();
        report = std::move(r).value();
        return std::monostate{};
    });
    if (!s) return s.error();
    return report;
}

Status AssemblyHistory::undo() {
    if (undo_.empty()) return Status::failure(ErrorCode::InvalidInput, "nothing to undo");
    Step step = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back({step.label, std::move(doc_)});
    doc_ = std::move(step.state);
    return std::monostate{};
}

Status AssemblyHistory::redo() {
    if (redo_.empty()) return Status::failure(ErrorCode::InvalidInput, "nothing to redo");
    Step step = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back({step.label, std::move(doc_)});
    doc_ = std::move(step.state);
    return std::monostate{};
}

void AssemblyHistory::clear() {
    undo_.clear();
    redo_.clear();
}

}  // namespace bendcore::assembly_v2
