#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "bendcore/document/entity_id.hpp"

namespace bendcore::commands {

using namespace bendcore::document;

/**
 * Типы событий документа
 */
enum class DocumentEventType {
    EntityCreated = 0,
    EntityDeleted = 1,
    EntityModified = 2,
    HierarchyChanged = 3,
    DocumentRevisionChanged = 4
};

/**
 * Событие документа
 */
struct DocumentEvent {
    DocumentEventType type;
    EntityId affected_id;
    std::chrono::system_clock::time_point timestamp;
    std::string description;

    DocumentEvent();
    DocumentEvent(DocumentEventType t, EntityId id, const std::string& desc);
};

/**
 * DocumentEventBus — реализует observer pattern для уведомлений об изменениях
 * CAD-CORE-008: Реактивность к изменениям документа
 */
class DocumentEventBus {
public:
    DocumentEventBus();
    ~DocumentEventBus() = default;

    // Подписка на события
    [[nodiscard]] std::string subscribe(
        DocumentEventType type,
        std::function<void(const DocumentEvent&)> handler
    );

    // Отписка
    void unsubscribe(const std::string& subscription_id);

    // Публикация события
    void publish(const DocumentEvent& event);

    // Состояние
    [[nodiscard]] size_t subscriber_count(DocumentEventType type) const;

    // Batch операции
    void begin_batch();
    void end_batch();

    [[nodiscard]] bool in_batch() const { return batch_mode_; }

private:
    using Subscriber = std::function<void(const DocumentEvent&)>;

    std::map<DocumentEventType, std::map<std::string, Subscriber>> subscribers_;
    std::vector<DocumentEvent> batch_queue_;
    bool batch_mode_ = false;
    int next_subscription_id_ = 0;

    std::string generate_subscription_id();
};

}  // namespace bendcore::commands
