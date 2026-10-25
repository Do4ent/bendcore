#include "bendcore/commands/document_event_bus.hpp"

namespace bendcore::commands {

DocumentEvent::DocumentEvent()
    : type(DocumentEventType::EntityCreated) {
    timestamp = std::chrono::system_clock::now();
}

DocumentEvent::DocumentEvent(DocumentEventType t, EntityId id, const std::string& desc)
    : type(t), affected_id(id), description(desc) {
    timestamp = std::chrono::system_clock::now();
}

DocumentEventBus::DocumentEventBus() {}

std::string DocumentEventBus::subscribe(
    DocumentEventType type,
    std::function<void(const DocumentEvent&)> handler) {
    std::string subscription_id = generate_subscription_id();
    subscribers_[type][subscription_id] = handler;
    return subscription_id;
}

void DocumentEventBus::unsubscribe(const std::string& subscription_id) {
    for (auto& [type, subs] : subscribers_) {
        subs.erase(subscription_id);
    }
}

void DocumentEventBus::publish(const DocumentEvent& event) {
    if (batch_mode_) {
        batch_queue_.push_back(event);
        return;
    }

    // Публиковать событие всем подписчикам
    auto it = subscribers_.find(event.type);
    if (it != subscribers_.end()) {
        for (auto& [sub_id, handler] : it->second) {
            handler(event);
        }
    }
}

size_t DocumentEventBus::subscriber_count(DocumentEventType type) const {
    auto it = subscribers_.find(type);
    if (it == subscribers_.end()) {
        return 0;
    }
    return it->second.size();
}

void DocumentEventBus::begin_batch() {
    batch_mode_ = true;
    batch_queue_.clear();
}

void DocumentEventBus::end_batch() {
    batch_mode_ = false;

    // Публиковать все события из queue
    for (const auto& event : batch_queue_) {
        auto it = subscribers_.find(event.type);
        if (it != subscribers_.end()) {
            for (auto& [sub_id, handler] : it->second) {
                handler(event);
            }
        }
    }

    batch_queue_.clear();
}

std::string DocumentEventBus::generate_subscription_id() {
    return "sub-" + std::to_string(next_subscription_id_++);
}

}  // namespace bendcore::commands
