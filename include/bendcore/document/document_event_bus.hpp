#pragma once

#include <functional>
#include <map>
#include <vector>
#include <string>
#include <memory>

namespace BendCore::CadCore {

class IEvent {
public:
    virtual ~IEvent() = default;
    virtual std::string getType() const = 0;
};

enum class EventPriority { LOW = 0, NORMAL = 1, HIGH = 2, CRITICAL = 3 };

/**
 * @brief Document event bus for change notifications
 */
class DocumentEventBus {
public:
    using EventCallback = std::function<void(const std::shared_ptr<IEvent>&)>;

    uint64_t subscribe(const std::string& event_type, EventCallback cb) noexcept;
    bool unsubscribe(uint64_t sub_id) noexcept;
    void dispatch(const std::shared_ptr<IEvent>& event,
                  EventPriority pri = EventPriority::NORMAL) noexcept;

    size_t processQueue() noexcept;
    void clearQueue() noexcept;

    [[nodiscard]] size_t getQueueSize() const noexcept { return queue_.size(); }
    [[nodiscard]] uint64_t getProcessedCount() const noexcept { return processed_; }

private:
    struct QueuedEvent {
        std::shared_ptr<IEvent> event;
        EventPriority priority;
    };

    std::map<std::string, std::vector<std::pair<uint64_t, EventCallback>>> subs_;
    std::vector<QueuedEvent> queue_;
    uint64_t next_id_ = 0;
    uint64_t processed_ = 0;
};

} // namespace BendCore::CadCore
