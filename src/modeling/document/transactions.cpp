// Undo / redo of the part document (docs/modeling/PART_DOCUMENT.md, section "Отмена и повтор").
//
// An undo entry stores the definition of the whole document (variables, rollback marker,
// node definitions) before the edit; redo entries store the state after it. Restoring a state
// keeps the node objects with the same id and their result pools, so the rebuild after undo
// or redo takes the earlier results by input hash instead of recomputing them.

#include "part_document_internal.hpp"

#include <algorithm>

namespace bendcore::modeling {

namespace {

Status fail(ErrorCode code, std::string message) { return Status::failure(code, std::move(message)); }

}  // namespace

PartDocument::State PartDocument::captureState() const {
    State s;
    s.variables = variables_;
    s.rollback = rollback_;
    s.nodes.reserve(nodes_.size());
    for (const auto& n : nodes_) s.nodes.push_back(static_cast<const NodeDefinition&>(*n));
    return s;
}

void PartDocument::restoreState(State state) {
    std::vector<std::unique_ptr<Node>> old = std::move(nodes_);
    nodes_.clear();
    for (NodeDefinition& def : state.nodes) {
        auto it = std::find_if(old.begin(), old.end(), [&](const std::unique_ptr<Node>& n) {
            return n && n->id == def.id && n->type == def.type;
        });
        std::unique_ptr<Node> n;
        if (it != old.end()) {
            n = std::move(*it);
        } else {
            n = std::make_unique<Node>();
            n->feature = registry_->create(def.type);
        }
        static_cast<NodeDefinition&>(*n) = std::move(def);
        nodes_.push_back(std::move(n));
    }
    variables_ = std::move(state.variables);
    rollback_ = state.rollback;
}

void PartDocument::recordEdit(const std::string& name, State before) {
    Transactions& t = *transactions_;
    if (t.open) {
        ++t.open->edits;
        return;
    }
    t.undo.push_back(Transactions::Entry{name, 1, std::move(before)});
    if (t.undo.size() > Transactions::kMaxEntries) t.undo.erase(t.undo.begin());
    t.redo.clear();
}

Status PartDocument::beginTransaction(const std::string& name) {
    Transactions& t = *transactions_;
    if (t.open) return fail(ErrorCode::InvalidInput, "transaction '" + t.open->name + "' is already open");
    if (name.empty()) return fail(ErrorCode::InvalidInput, "a transaction needs a name");
    t.open = Transactions::Entry{name, 0, captureState()};
    return std::monostate{};
}

Status PartDocument::commitTransaction() {
    Transactions& t = *transactions_;
    if (!t.open) return fail(ErrorCode::InvalidInput, "no open transaction");
    Transactions::Entry e = std::move(*t.open);
    t.open.reset();
    if (e.edits == 0) return std::monostate{};
    t.undo.push_back(std::move(e));
    if (t.undo.size() > Transactions::kMaxEntries) t.undo.erase(t.undo.begin());
    t.redo.clear();
    return std::monostate{};
}

Status PartDocument::abortTransaction() {
    Transactions& t = *transactions_;
    if (!t.open) return fail(ErrorCode::InvalidInput, "no open transaction");
    State s = std::move(t.open->state);
    t.open.reset();
    restoreState(std::move(s));
    return std::monostate{};
}

bool PartDocument::inTransaction() const noexcept { return transactions_->open.has_value(); }
bool PartDocument::canUndo() const noexcept { return !transactions_->open && !transactions_->undo.empty(); }
bool PartDocument::canRedo() const noexcept { return !transactions_->open && !transactions_->redo.empty(); }

Result<RebuildReport> PartDocument::undo() {
    Transactions& t = *transactions_;
    if (t.open) return Result<RebuildReport>::failure(ErrorCode::InvalidInput, "commit or abort the open transaction first");
    if (t.undo.empty()) return Result<RebuildReport>::failure(ErrorCode::InvalidInput, "nothing to undo");
    Transactions::Entry e = std::move(t.undo.back());
    t.undo.pop_back();
    t.redo.push_back(Transactions::Entry{e.name, e.edits, captureState()});
    restoreState(std::move(e.state));
    return rebuild();
}

Result<RebuildReport> PartDocument::redo() {
    Transactions& t = *transactions_;
    if (t.open) return Result<RebuildReport>::failure(ErrorCode::InvalidInput, "commit or abort the open transaction first");
    if (t.redo.empty()) return Result<RebuildReport>::failure(ErrorCode::InvalidInput, "nothing to redo");
    Transactions::Entry e = std::move(t.redo.back());
    t.redo.pop_back();
    t.undo.push_back(Transactions::Entry{e.name, e.edits, captureState()});
    restoreState(std::move(e.state));
    return rebuild();
}

std::vector<TransactionInfo> PartDocument::undoHistory() const {
    std::vector<TransactionInfo> out;
    for (const auto& e : transactions_->undo) out.push_back({e.name, e.edits});
    return out;
}

std::vector<TransactionInfo> PartDocument::redoHistory() const {
    std::vector<TransactionInfo> out;
    for (const auto& e : transactions_->redo) out.push_back({e.name, e.edits});
    return out;
}

void PartDocument::clearHistory() {
    transactions_->undo.clear();
    transactions_->redo.clear();
    transactions_->open.reset();
}

}  // namespace bendcore::modeling
