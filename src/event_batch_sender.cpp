#include "bendcore/event_batch_sender.hpp"
#include "bendcore/growthbook_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace bendcore {

EventBatchSender::EventBatchSender() = default;

EventBatchSender::~EventBatchSender() {
  stop();
}

void EventBatchSender::start(const BatchConfig& config) {
  config_ = config;
  running_ = true;
}

void EventBatchSender::stop() {
  running_ = false;
}

bool EventBatchSender::send_batch(const std::vector<AnalyticsEvent>& events) {
  if (events.empty()) {
    return true;
  }

  // Check if GrowthBook is initialized
  GrowthBookClient& gb = GrowthBookClient::getInstance();
  if (!gb.is_initialized() || gb.get_api_key().empty()) {
    last_error_ = "GrowthBook not initialized";
    failed_sends_++;
    return false;
  }

  return send_to_growthbook(events);
}

bool EventBatchSender::is_running() const {
  return running_;
}

size_t EventBatchSender::get_pending_count() const {
  return 0;  // Would be tracked by AnalyticsEventQueue
}

const std::string& EventBatchSender::get_last_error() const {
  return last_error_;
}

uint32_t EventBatchSender::get_failed_sends() const {
  return failed_sends_;
}

uint32_t EventBatchSender::get_successful_sends() const {
  return successful_sends_;
}

bool EventBatchSender::send_to_growthbook(const std::vector<AnalyticsEvent>& events) {
  try {
    std::string batch_json = serialize_batch(events);

    // In a real implementation, this would make an HTTP POST request
    // POST /api/events to GrowthBook API endpoint
    // For now, simulate successful send

    successful_sends_++;
    last_error_ = "";
    return true;
  }
  catch (const std::exception& e) {
    last_error_ = std::string("Failed to send batch: ") + e.what();
    failed_sends_++;
    return false;
  }
}

std::string EventBatchSender::serialize_batch(const std::vector<AnalyticsEvent>& events) {
  nlohmann::json batch = nlohmann::json::array();

  for (const auto& event : events) {
    batch.push_back(event.to_json());
  }

  return batch.dump();
}

}  // namespace bendcore
