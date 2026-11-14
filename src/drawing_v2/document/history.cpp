// Отмена и повтор правок документа чертежа (транзакции: листы, виды, аннотации, основная
// надпись, настройки, свойства изделия). docs/drawing/DRAWING_DOCUMENT.md, «Отмена и повтор».

#include <bendcore/drawing_v2/drawing_document.hpp>

namespace bendcore::drawing_v2::document {

DrawingEditState captureEditState(const DrawingDocument& doc) {
    return DrawingEditState{doc.sheets(), doc.settings(), doc.model().properties};
}

void restoreEditState(DrawingDocument& doc, const DrawingEditState& state) {
    doc.sheets() = state.sheets;
    doc.settings() = state.settings;
    doc.model().properties = state.properties;
}

void DrawingHistory::record(DrawingEditState before, std::string label) {
    undo_.push_back(Entry{std::move(before), std::move(label)});
    if (undo_.size() > limit_) undo_.erase(undo_.begin(), undo_.begin() + std::ptrdiff_t(undo_.size() - limit_));
    redo_.clear();
}

Result<std::string> DrawingHistory::undo(DrawingDocument& doc) {
    if (undo_.empty()) return Result<std::string>::failure(ErrorCode::InvalidInput, "drawing history: nothing to undo");
    Entry e = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back(Entry{captureEditState(doc), e.label});
    restoreEditState(doc, e.state);
    return e.label;
}

Result<std::string> DrawingHistory::redo(DrawingDocument& doc) {
    if (redo_.empty()) return Result<std::string>::failure(ErrorCode::InvalidInput, "drawing history: nothing to redo");
    Entry e = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back(Entry{captureEditState(doc), e.label});
    restoreEditState(doc, e.state);
    return e.label;
}

void DrawingHistory::clear() {
    undo_.clear();
    redo_.clear();
}

}  // namespace bendcore::drawing_v2::document
