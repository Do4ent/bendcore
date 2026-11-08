#pragma once

// Отмена и повтор изменений документа сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 11).
//
// Каждое изменение — транзакция над AssemblyDocument: до неё запоминается снимок документа
// (определения, вхождения с положениями, сопряжения, массивы, виды; формы OCCT и карты
// имён — общие дескрипторы, не копируются). Ошибка внутри транзакции возвращает документ к
// снимку. undo() восстанавливает состояние до транзакции целиком, включая положения,
// которые записало решение сопряжений; redo() — состояние после неё.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace bendcore::assembly_v2 {

struct HistoryOptions {
    std::size_t maxDepth = 200;  // старейшие шаги отбрасываются
    // Решать сопряжения в конце каждой транзакции, меняющей состав или сопряжения
    // (вставка, удаление, замена, сопряжения, подавление, массивы).
    bool solveAfterEdit = true;
    SolverOptions solver;
};

class AssemblyHistory {
public:
    explicit AssemblyHistory(AssemblyDocument document, HistoryOptions options = {});

    const AssemblyDocument& document() const noexcept { return doc_; }

    // Произвольное изменение одной транзакцией. Ошибка edit — документ не меняется, шаг не
    // записывается. solve — решать ли сопряжения после edit.
    Status apply(std::string label, const std::function<Status(AssemblyDocument&)>& edit, bool solve = false);

    kernel::Result<OccurrenceId> insertComponent(DefinitionId definition, const Transform& placement,
                                                 std::string name = {}, std::optional<DefinitionId> parent = {});
    Status removeComponent(OccurrenceId id);
    kernel::Result<ReplaceReport> replaceComponent(OccurrenceId id, DefinitionId definition);
    Status moveComponent(OccurrenceId id, const Transform& placement);
    Status setFixed(OccurrenceId id, bool fixed);
    Status setComponentSuppressed(OccurrenceId id, bool suppressed);

    kernel::Result<MateId> addMate(MateSpec spec);
    Status editMate(MateId id, MateSpec spec);
    Status removeMate(MateId id);
    Status setMateSuppressed(MateId id, bool suppressed);

    kernel::Result<PatternId> addLinearPattern(const LinearPatternSpec& spec);
    kernel::Result<PatternId> addCircularPattern(const CircularPatternSpec& spec);
    kernel::Result<PatternId> addFeaturePattern(const FeaturePatternSpec& spec);
    Status removePattern(PatternId id);

    kernel::Result<DocumentSolveReport> solve();

    bool canUndo() const noexcept { return !undo_.empty(); }
    bool canRedo() const noexcept { return !redo_.empty(); }
    // Название шага, который отменит undo() / повторит redo(); пусто, если шага нет.
    std::string undoLabel() const { return undo_.empty() ? std::string() : undo_.back().label; }
    std::string redoLabel() const { return redo_.empty() ? std::string() : redo_.back().label; }
    std::size_t undoDepth() const noexcept { return undo_.size(); }
    // Ошибка InvalidInput — нечего отменять / повторять.
    Status undo();
    Status redo();
    void clear();

private:
    struct Step {
        std::string label;
        AssemblyDocument state;
    };
    void record(std::string label, AssemblyDocument before);
    Status solveNow();

    AssemblyDocument doc_;
    HistoryOptions options_;
    std::vector<Step> undo_;
    std::vector<Step> redo_;
};

}  // namespace bendcore::assembly_v2
