#include "bendcore/api/document_api.hpp"
#include <sstream>

namespace BendCore::CadCore::API {

DocumentAPI::DocumentAPI(CadDocument& doc)
    : document_(doc)
    , undo_redo_(std::make_unique<UndoRedoManager>(100, 100000000))
    , serializer_(std::make_unique<DocumentSerializer>(DocumentSerializer::Format::BINARY))
    , last_saved_revision_(doc.getRevision())
    , dirty_status_(DirtyStatus::CLEAN)
{}

DocumentAPI::~DocumentAPI() = default;

void DocumentAPI::clearDirtyStatus() noexcept {
    last_saved_revision_ = document_.getRevision();
    dirty_status_ = DirtyStatus::CLEAN;
    dirty_description_.clear();
}

bool DocumentAPI::undo() {
    if (!undo_redo_->canUndo()) {
        return false;
    }

    if (undo_redo_->undo(document_)) {
        updateDirtyStatus();

        DocumentEvent event{
            DocumentEvent::Type::UNDO,
            document_.getRevision(),
            "Undo operation"
        };
        emitEvent(event);

        return true;
    }

    return false;
}

bool DocumentAPI::redo() {
    if (!undo_redo_->canRedo()) {
        return false;
    }

    if (undo_redo_->redo(document_)) {
        updateDirtyStatus();

        DocumentEvent event{
            DocumentEvent::Type::REDO,
            document_.getRevision(),
            "Redo operation"
        };
        emitEvent(event);

        return true;
    }

    return false;
}

std::unique_ptr<DocumentTransaction> DocumentAPI::beginTransaction(
    const std::string& description)
{
    return std::make_unique<DocumentTransaction>(document_, description);
}

SerializationResult DocumentAPI::save(const std::string& filepath) {
    DocumentMetadata metadata;
    metadata.title = "CAD Document";
    metadata.author = "BendCore";
    metadata.version = {1, 0, 0};
    metadata.modified_time = std::chrono::system_clock::now();

    auto result = serializer_->serializeToFile(document_, filepath, metadata);

    if (result.success) {
        clearDirtyStatus();

        DocumentEvent event{
            DocumentEvent::Type::SAVED,
            document_.getRevision(),
            "Document saved to " + filepath
        };
        emitEvent(event);
    } else {
        last_error_ = result.error;
    }

    return result;
}

SerializationResult DocumentAPI::load(const std::string& filepath) {
    CadDocument temp_doc;
    auto result = serializer_->deserializeFromFile(temp_doc, filepath);

    if (result.success) {
        document_ = temp_doc;
        clearDirtyStatus();
        undo_redo_->clear();

        DocumentEvent event{
            DocumentEvent::Type::LOADED,
            document_.getRevision(),
            "Document loaded from " + filepath
        };
        emitEvent(event);
    } else {
        last_error_ = result.error;
    }

    return result;
}

void DocumentAPI::subscribeToEvents(const EventCallback& callback) {
    event_subscribers_.push_back(callback);
}

void DocumentAPI::unsubscribeFromEvents() {
    event_subscribers_.clear();
}

DocumentAPI::Statistics DocumentAPI::getStatistics() const {
    return {
        document_.getRevision(),
        document_.getActiveEntityCount(),
        undo_redo_->getUndoCount(),
        undo_redo_->getRedoCount(),
        undo_redo_->getMemoryUsage(),
        dirty_status_
    };
}

std::vector<EntityId> DocumentAPI::createEntities(
    const std::vector<Entity>& entities,
    const std::string& description)
{
    std::vector<EntityId> created_ids;

    {
        auto tx = beginTransaction(description);

        for (const auto& entity : entities) {
            try {
                document_.addEntity(entity);
                created_ids.push_back(entity.id);

                if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
                    tx_ptr->recordEntityChange(entity.id);
                }
            } catch (const std::exception&) {
                // Continue with next entity
            }
        }

        if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
            tx_ptr->commit();
        }
    }

    updateDirtyStatus();

    DocumentEvent event{
        DocumentEvent::Type::MODIFIED,
        document_.getRevision(),
        description,
        created_ids
    };
    emitEvent(event);

    return created_ids;
}

bool DocumentAPI::updateEntities(
    const std::vector<Entity>& entities,
    const std::string& description)
{
    std::vector<EntityId> affected_ids;

    {
        auto tx = beginTransaction(description);

        for (const auto& entity : entities) {
            try {
                auto& existing = document_.getEntity(entity.id);
                existing = entity;
                affected_ids.push_back(entity.id);

                if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
                    tx_ptr->recordEntityChange(entity.id);
                }
            } catch (const std::exception&) {
                // Continue with next entity
            }
        }

        if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
            tx_ptr->commit();
        }
    }

    updateDirtyStatus();

    DocumentEvent event{
        DocumentEvent::Type::MODIFIED,
        document_.getRevision(),
        description,
        affected_ids
    };
    emitEvent(event);

    return !affected_ids.empty();
}

bool DocumentAPI::deleteEntities(
    const std::vector<EntityId>& ids,
    const std::string& description)
{
    std::vector<EntityId> deleted_ids;

    {
        auto tx = beginTransaction(description);

        for (const auto& id : ids) {
            try {
                document_.removeEntity(id);
                deleted_ids.push_back(id);

                if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
                    tx_ptr->recordEntityChange(id);
                }
            } catch (const std::exception&) {
                // Continue with next entity
            }
        }

        if (auto* tx_ptr = dynamic_cast<DocumentTransaction*>(tx.get())) {
            tx_ptr->commit();
        }
    }

    updateDirtyStatus();

    DocumentEvent event{
        DocumentEvent::Type::MODIFIED,
        document_.getRevision(),
        description,
        deleted_ids
    };
    emitEvent(event);

    return !deleted_ids.empty();
}

void DocumentAPI::emitEvent(const DocumentEvent& event) {
    for (const auto& subscriber : event_subscribers_) {
        if (subscriber) {
            subscriber(event);
        }
    }
}

void DocumentAPI::updateDirtyStatus() {
    if (isModified()) {
        dirty_status_ = DirtyStatus::MODIFIED;

        std::ostringstream oss;
        oss << "Document modified (revision " << getCurrentRevision() << ")";
        dirty_description_ = oss.str();
    } else {
        dirty_status_ = DirtyStatus::CLEAN;
        dirty_description_ = "Document saved";
    }
}

} // namespace BendCore::CadCore::API
