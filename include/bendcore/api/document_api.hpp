#pragma once

#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/transaction.hpp"
#include "bendcore/document/undo_redo_manager.hpp"
#include "bendcore/serialization/document_serializer.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace BendCore::CadCore::API {

/**
 * @brief Document dirty status
 */
enum class DirtyStatus {
    CLEAN,
    MODIFIED,
    SAVED_PENDING
};

/**
 * @brief Document synchronization event
 */
struct DocumentEvent {
    enum class Type {
        CREATED,
        MODIFIED,
        SAVED,
        LOADED,
        UNDO,
        REDO,
        REVISION_CHANGED,
        DIRTY_STATUS_CHANGED,
        ERROR
    };

    Type type;
    uint64_t revision;
    std::string description;
    std::vector<EntityId> affected_entities;
    std::string error_message;
};

/**
 * @brief React Document API (GAP-019: API for React)
 *
 * Provides synchronization interface for web frontend:
 * - Revision management
 * - Dirty status tracking
 * - Undo/Redo stack API
 * - Event subscription
 * - Error handling
 */
class DocumentAPI {
public:
    explicit DocumentAPI(CadDocument& doc);
    ~DocumentAPI();

    // Document access
    [[nodiscard]] CadDocument& getDocument() noexcept { return document_; }
    [[nodiscard]] const CadDocument& getDocument() const noexcept { return document_; }

    // Revision management
    [[nodiscard]] uint64_t getCurrentRevision() const noexcept {
        return document_.getRevision();
    }

    [[nodiscard]] uint64_t getLastSavedRevision() const noexcept {
        return last_saved_revision_;
    }

    [[nodiscard]] bool isModified() const noexcept {
        return getCurrentRevision() != getLastSavedRevision();
    }

    // Dirty status
    [[nodiscard]] DirtyStatus getDirtyStatus() const noexcept { return dirty_status_; }

    [[nodiscard]] const std::string& getDirtyDescription() const noexcept {
        return dirty_description_;
    }

    void clearDirtyStatus() noexcept;

    // Undo/Redo operations
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return undo_redo_->canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return undo_redo_->canRedo(); }

    [[nodiscard]] std::vector<std::string> getUndoHistory(size_t limit = 10) const {
        return undo_redo_->getUndoHistory(limit);
    }

    [[nodiscard]] std::vector<std::string> getRedoHistory(size_t limit = 10) const {
        return undo_redo_->getRedoHistory(limit);
    }

    // Transaction management
    [[nodiscard]] std::unique_ptr<DocumentTransaction> beginTransaction(
        const std::string& description = ""
    );

    // Serialization
    [[nodiscard]] SerializationResult save(const std::string& filepath);
    [[nodiscard]] SerializationResult load(const std::string& filepath);

    // Event subscription
    using EventCallback = std::function<void(const DocumentEvent&)>;
    void subscribeToEvents(const EventCallback& callback);
    void unsubscribeFromEvents();

    // Error reporting
    [[nodiscard]] const std::string& getLastError() const noexcept { return last_error_; }
    void clearLastError() noexcept { last_error_.clear(); }

    // Statistics
    struct Statistics {
        uint64_t current_revision;
        uint64_t entity_count;
        size_t undo_stack_size;
        size_t redo_stack_size;
        size_t memory_usage;
        DirtyStatus dirty_status;
    };

    [[nodiscard]] Statistics getStatistics() const;

    // Batch operations
    [[nodiscard]] std::vector<EntityId> createEntities(
        const std::vector<Entity>& entities,
        const std::string& description = "Create multiple entities"
    );

    [[nodiscard]] bool updateEntities(
        const std::vector<Entity>& entities,
        const std::string& description = "Update entities"
    );

    [[nodiscard]] bool deleteEntities(
        const std::vector<EntityId>& ids,
        const std::string& description = "Delete entities"
    );

private:
    CadDocument& document_;
    std::unique_ptr<UndoRedoManager> undo_redo_;
    std::unique_ptr<DocumentSerializer> serializer_;

    // State tracking
    uint64_t last_saved_revision_;
    DirtyStatus dirty_status_;
    std::string dirty_description_;
    std::string last_error_;

    // Event system
    std::vector<EventCallback> event_subscribers_;

    /**
     * @brief Emit document event
     */
    void emitEvent(const DocumentEvent& event);

    /**
     * @brief Update dirty status based on current revision
     */
    void updateDirtyStatus();
};

} // namespace BendCore::CadCore::API
