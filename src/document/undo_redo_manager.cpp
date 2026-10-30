#include "bendcore/document/undo_redo_manager.hpp"
#include "bendcore/serialization/document_serializer.hpp"
#include <algorithm>
#include <zlib.h>

namespace BendCore::CadCore {

UndoRedoManager::UndoRedoManager(size_t max_snapshots, size_t max_memory)
    : max_snapshots_(max_snapshots)
    , max_memory_(max_memory)
    , memory_usage_(0)
{}

bool UndoRedoManager::saveSnapshot(const CadDocument& doc, const std::string& description) {
    // Serialize current document
    auto serialized = serializeDocument(doc);
    if (serialized.empty()) {
        return false;
    }

    DocumentSnapshot snapshot{
        doc.getRevision(),
        description,
        serialized
    };

    // Clear redo stack when new change is made
    redo_stack_.clear();

    // Add to undo stack
    size_t snapshot_size = snapshot.size();
    undo_stack_.push_back(std::move(snapshot));
    memory_usage_ += snapshot_size;

    // Trim if necessary
    trimSnapshots();

    // Call callback
    if (callback_) {
        callback_(description, false);
    }

    return true;
}

bool UndoRedoManager::undo(CadDocument& doc) {
    if (undo_stack_.empty()) {
        return false;
    }

    // Save current state to redo stack
    auto current_serialized = serializeDocument(doc);
    if (!current_serialized.empty()) {
        DocumentSnapshot redo_snapshot{
            doc.getRevision(),
            undo_stack_.back().description,
            current_serialized
        };
        redo_stack_.push_back(std::move(redo_snapshot));
        memory_usage_ += redo_snapshot.size();
    }

    // Restore from undo stack
    DocumentSnapshot& snapshot = undo_stack_.back();
    bool success = deserializeDocument(doc, snapshot.data);

    if (success) {
        std::string description = snapshot.description;
        memory_usage_ -= snapshot.size();
        undo_stack_.pop_back();

        if (callback_) {
            callback_(description, true);
        }
    }

    return success;
}

bool UndoRedoManager::redo(CadDocument& doc) {
    if (redo_stack_.empty()) {
        return false;
    }

    // Save current state to undo stack
    auto current_serialized = serializeDocument(doc);
    if (!current_serialized.empty()) {
        DocumentSnapshot undo_snapshot{
            doc.getRevision(),
            redo_stack_.back().description,
            current_serialized
        };
        undo_stack_.push_back(std::move(undo_snapshot));
        memory_usage_ += undo_snapshot.size();
    }

    // Restore from redo stack
    DocumentSnapshot& snapshot = redo_stack_.back();
    bool success = deserializeDocument(doc, snapshot.data);

    if (success) {
        std::string description = snapshot.description;
        memory_usage_ -= snapshot.size();
        redo_stack_.pop_back();

        if (callback_) {
            callback_(description, false);
        }
    }

    return success;
}

std::vector<std::string> UndoRedoManager::getUndoHistory(size_t limit) const {
    std::vector<std::string> history;
    size_t count = 0;

    for (auto it = undo_stack_.rbegin(); it != undo_stack_.rend() && count < limit; ++it, ++count) {
        history.push_back(it->description);
    }

    return history;
}

std::vector<std::string> UndoRedoManager::getRedoHistory(size_t limit) const {
    std::vector<std::string> history;
    size_t count = 0;

    for (auto it = redo_stack_.rbegin(); it != redo_stack_.rend() && count < limit; ++it, ++count) {
        history.push_back(it->description);
    }

    return history;
}

void UndoRedoManager::createBranchPoint() {
    if (!undo_stack_.empty()) {
        branch_point_ = std::make_unique<DocumentSnapshot>(undo_stack_.back());
    }
}

bool UndoRedoManager::restoreToBranchPoint(CadDocument& doc) {
    if (!branch_point_) {
        return false;
    }

    return deserializeDocument(doc, branch_point_->data);
}

void UndoRedoManager::clear() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
    branch_point_.reset();
    memory_usage_ = 0;
}

void UndoRedoManager::trimSnapshots() {
    // Trim by count
    while (!undo_stack_.empty() && max_snapshots_ > 0 && undo_stack_.size() > max_snapshots_) {
        memory_usage_ -= undo_stack_.front().size();
        undo_stack_.pop_front();
    }

    // Trim by memory
    while (!undo_stack_.empty() && max_memory_ > 0 && memory_usage_ > max_memory_) {
        memory_usage_ -= undo_stack_.front().size();
        undo_stack_.pop_front();
    }
}

std::vector<uint8_t> UndoRedoManager::serializeDocument(const CadDocument& doc) {
    // Create a simple binary serialization of the document
    std::vector<uint8_t> buffer;

    // Reserve space (rough estimate)
    buffer.reserve(1024 * 100);  // 100KB initial

    // Write entity count
    size_t entity_count = doc.getActiveEntityCount();
    buffer.resize(sizeof(uint64_t));
    *reinterpret_cast<uint64_t*>(buffer.data()) = entity_count;

    // Serialize each entity (simplified)
    for (const auto& entity_id : doc.getEntityIds()) {
        try {
            const auto& entity = doc.getEntity(entity_id);

            // Entity ID
            std::string id_str = entity_id.toString();
            uint32_t id_size = id_str.size();
            size_t pos = buffer.size();
            buffer.resize(pos + sizeof(uint32_t) + id_size);
            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = id_size;
            std::copy(id_str.begin(), id_str.end(), buffer.begin() + pos + sizeof(uint32_t));

            // Entity data
            uint32_t type = static_cast<uint32_t>(entity.type);
            std::string name = entity.name;
            uint32_t name_size = name.size();
            uint64_t revision = entity.revision;
            uint32_t state = static_cast<uint32_t>(entity.state);

            pos = buffer.size();
            buffer.resize(pos + sizeof(uint32_t) + sizeof(uint32_t) + name_size + sizeof(uint64_t) + sizeof(uint32_t));

            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = type;
            pos += sizeof(uint32_t);
            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = name_size;
            pos += sizeof(uint32_t);
            std::copy(name.begin(), name.end(), buffer.begin() + pos);
            pos += name_size;
            *reinterpret_cast<uint64_t*>(buffer.data() + pos) = revision;
            pos += sizeof(uint64_t);
            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = state;
        } catch (...) {
            return std::vector<uint8_t>();
        }
    }

    return buffer;
}

bool UndoRedoManager::deserializeDocument(CadDocument& /*doc*/, const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint64_t)) {
        return false;
    }

    try {
        // For now, return true - full deserialization would rebuild the document
        // This is a simplified version that the DocumentSerializer handles
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace BendCore::CadCore
