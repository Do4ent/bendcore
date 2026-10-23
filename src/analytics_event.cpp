#include "bendcore/analytics_event.hpp"
#include <iostream>

namespace bendcore {

nlohmann::json AnalyticsEvent::to_json() const {
  nlohmann::json j;
  j["event_name"] = event_name;
  j["category"] = static_cast<int>(category);
  j["properties"] = nlohmann::json::object();

  for (const auto& [key, value] : properties) {
    j["properties"][key] = value;
  }

  j["user_id"] = user_id;
  j["session_id"] = session_id;
  j["timestamp"] = std::chrono::system_clock::to_time_t(timestamp);
  j["version"] = version;

  return j;
}

AnalyticsEventQueue::AnalyticsEventQueue() = default;

AnalyticsEventQueue::~AnalyticsEventQueue() {
  stop_background_flush();
}

void AnalyticsEventQueue::track_event(const AnalyticsEvent& event) {
  std::lock_guard<std::mutex> lock(mutex_);
  event_queue_.push_back(event);
}

void AnalyticsEventQueue::flush() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (event_queue_.empty()) {
    return;
  }

  // Convert events to JSON and send to GrowthBook
  nlohmann::json batch = nlohmann::json::array();

  for (const auto& event : event_queue_) {
    batch.push_back(event.to_json());
  }

  // In a real implementation, this would make an HTTP POST request to GrowthBook API
  // For now, just clear the queue
  event_queue_.clear();
  last_flush_ = std::chrono::system_clock::now();
}

size_t AnalyticsEventQueue::get_queue_size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return event_queue_.size();
}

std::chrono::milliseconds AnalyticsEventQueue::get_last_flush_age() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush_);
}

void AnalyticsEventQueue::start_background_flush(std::chrono::milliseconds flush_interval) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (background_flush_active_) {
    return;
  }

  background_flush_active_ = true;
  flush_thread_ = std::thread([this, flush_interval]() {
    background_flush_loop(flush_interval);
  });
}

void AnalyticsEventQueue::stop_background_flush() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    background_flush_active_ = false;
  }

  if (flush_thread_.joinable()) {
    flush_thread_.join();
  }
}

bool AnalyticsEventQueue::is_background_flush_active() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return background_flush_active_;
}

void AnalyticsEventQueue::background_flush_loop(std::chrono::milliseconds flush_interval) {
  while (true) {
    std::this_thread::sleep_for(flush_interval);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!background_flush_active_) {
        break;
      }
    }

    flush();
  }
}

std::vector<AnalyticsEvent> AnalyticsEventQueue::get_events() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return event_queue_;
}

// Event helper functions
namespace events {

AnalyticsEvent import_step_opened(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "import_step_opened";
  event.category = EventCategory::IMPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent import_step_success(const std::string& user_id, const std::string& session_id,
                                   const std::map<std::string, std::string>& props) {
  AnalyticsEvent event;
  event.event_name = "import_step_success";
  event.category = EventCategory::IMPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties = props;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent import_step_error(const std::string& user_id, const std::string& session_id,
                                 const std::string& error_msg) {
  AnalyticsEvent event;
  event.event_name = "import_step_error";
  event.category = EventCategory::IMPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties["error"] = error_msg;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent template_match_requested(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "template_match_requested";
  event.category = EventCategory::TEMPLATE;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent template_match_success(const std::string& user_id, const std::string& session_id,
                                      const std::map<std::string, std::string>& props) {
  AnalyticsEvent event;
  event.event_name = "template_match_success";
  event.category = EventCategory::TEMPLATE;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties = props;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent template_selected(const std::string& user_id, const std::string& session_id,
                                 const std::string& template_id) {
  AnalyticsEvent event;
  event.event_name = "template_selected";
  event.category = EventCategory::TEMPLATE;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties["template_id"] = template_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent search_opened(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "search_opened";
  event.category = EventCategory::SEARCH;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent search_query(const std::string& user_id, const std::string& session_id,
                            const std::string& query) {
  AnalyticsEvent event;
  event.event_name = "search_query";
  event.category = EventCategory::SEARCH;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties["query"] = query;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent formula_created(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "formula_created";
  event.category = EventCategory::FORMULA;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent formula_evaluated(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "formula_evaluated";
  event.category = EventCategory::FORMULA;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent formula_error(const std::string& user_id, const std::string& session_id,
                             const std::string& error_msg) {
  AnalyticsEvent event;
  event.event_name = "formula_error";
  event.category = EventCategory::FORMULA;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties["error"] = error_msg;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent tube_created(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "tube_created";
  event.category = EventCategory::TUBE_DESIGN;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent tube_saved(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "tube_saved";
  event.category = EventCategory::TUBE_DESIGN;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent bend_added(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "bend_added";
  event.category = EventCategory::TUBE_DESIGN;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent export_dwf(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "export_dwf";
  event.category = EventCategory::EXPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent export_pdf(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "export_pdf";
  event.category = EventCategory::EXPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent export_step(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "export_step";
  event.category = EventCategory::EXPORT;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent session_start(const std::string& user_id) {
  AnalyticsEvent event;
  event.event_name = "session_start";
  event.category = EventCategory::SYSTEM;
  event.user_id = user_id;
  event.session_id = "session_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent session_end(const std::string& user_id, const std::string& session_id) {
  AnalyticsEvent event;
  event.event_name = "session_end";
  event.category = EventCategory::SYSTEM;
  event.user_id = user_id;
  event.session_id = session_id;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

AnalyticsEvent app_error(const std::string& user_id, const std::string& session_id,
                        const std::string& error_msg) {
  AnalyticsEvent event;
  event.event_name = "app_error";
  event.category = EventCategory::SYSTEM;
  event.user_id = user_id;
  event.session_id = session_id;
  event.properties["error"] = error_msg;
  event.timestamp = std::chrono::system_clock::now();
  return event;
}

}  // namespace events

}  // namespace bendcore
