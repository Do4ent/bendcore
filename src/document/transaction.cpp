#include "bendcore/document/transaction.hpp"
#include <algorithm>
#include <sstream>

namespace BendCore::CadCore {

uint64_t DocumentTransaction::next_transaction_id_ = 1;

DocumentTransaction::DocumentTransaction(CadDocument& doc, const std::string& description)
    : id_(next_transaction_id_++)
    , description_(description)
    , document_(doc)
    , snapshot_(doc)
    , base_revision_(doc.getRevision())
    , is_active_(true)
    , timestamp_(std::chrono::system_clock::now())
{}

DocumentTransaction::~DocumentTransaction() {
    if (is_active_) {
        rollback();
    }
}

bool DocumentTransaction::commit() {
    if (!is_active_) {
        return false;
    }

    // Validate consistency
    std::string validation_error = validate();
    if (!validation_error.empty()) {
        return false;
    }

    is_active_ = false;
    return true;
}

bool DocumentTransaction::rollback() {
    if (!is_active_) {
        return false;
    }

    // Restore document from snapshot
    document_.clear();

    // Copy entities from snapshot
    for (const auto& entity_id : snapshot_.getEntityIds()) {
        const auto& entity = snapshot_.getEntity(entity_id);
        document_.addEntity(entity);
    }

    is_active_ = false;
    return true;
}

void DocumentTransaction::recordEntityChange(const EntityId& id) {
    if (!is_active_) return;

    auto it = std::find(affected_entities_.begin(), affected_entities_.end(), id);
    if (it == affected_entities_.end()) {
        affected_entities_.push_back(id);
    }
}

size_t DocumentTransaction::getSize() const noexcept {
    // Estimate size based on affected entities
    size_t size = 0;

    // Each entity roughly 512 bytes
    size += affected_entities_.size() * 512;

    // Add overhead
    size += 256;

    return size;
}

std::string DocumentTransaction::validate() const {
    // Check document integrity
    std::string doc_validation = document_.validate();
    if (!doc_validation.empty()) {
        return doc_validation;
    }

    // Check for duplicate IDs
    if (document_.hasDuplicateIds()) {
        return "Duplicate entity IDs detected";
    }

    // Validate affected entities still exist
    for (const auto& entity_id : affected_entities_) {
        if (!document_.hasEntity(entity_id)) {
            std::ostringstream oss;
            oss << "Affected entity not found: " << entity_id.toString();
            return oss.str();
        }
    }

    return "";
}

} // namespace BendCore::CadCore
