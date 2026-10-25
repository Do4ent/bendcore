#include "bendcore/document/document_event_bus.hpp"
#include <algorithm>

namespace BendCore::CadCore {

uint64_t DocumentEventBus::subscribe(const std::string& event_type, EventCallback cb) noexcept {
    if (!cb) return 0;
    uint64_t id = ++next_id_;
    subs_[event_type].push_back({id, cb});
    return id;
}

bool DocumentEventBus::unsubscribe(uint64_t sub_id) noexcept {
    for (auto& [type, cbs] : subs_) {
        auto it = std::find_if(cbs.begin(), cbs.end(),
            [sub_id](const auto& p) { return p.first == sub_id; });
        if (it != cbs.end()) {
            cbs.erase(it);
            return true;
        }
    }
    return false;
}

void DocumentEventBus::dispatch(const std::shared_ptr<IEvent>& event,
                                EventPriority pri) noexcept {
    if (!event) return;
    queue_.push_back({event, pri});
}

size_t DocumentEventBus::processQueue() noexcept {
    size_t count = 0;
    for (const auto& queued : queue_) {
        auto it = subs_.find(queued.event->getType());
        if (it != subs_.end()) {
            for (const auto& [id, cb] : it->second) {
                try { cb(queued.event); } catch (...) {}
            }
        }
        processed_++;
        count++;
    }
    queue_.clear();
    return count;
}

void DocumentEventBus::clearQueue() noexcept {
    queue_.clear();
}

} // namespace BendCore::CadCore
