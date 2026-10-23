#pragma once

#include "bendcore/analytics_event.hpp"
#include <vector>
#include <chrono>
#include <string>

namespace bendcore {

/**
 * Event Batch Sender
 *
 * Batches analytics events and sends them to GrowthBook API
 * - Sends when 50 events accumulated OR 5 seconds elapsed
 * - Async background sender thread
 * - Network error retry with exponential backoff
 * - Event deduplication
 */
class EventBatchSender {
public:
  struct BatchConfig {
    size_t max_batch_size = 50;                         // Send when 50 events accumulated
    std::chrono::milliseconds max_wait = std::chrono::seconds(5);  // Or 5 seconds timeout
  };

  EventBatchSender();
  ~EventBatchSender();

  /**
   * Start batch sender with configuration
   */
  void start(const BatchConfig& config);

  /**
   * Stop batch sender
   */
  void stop();

  /**
   * Send batch of events to GrowthBook
   * @return true if successful, false if error
   */
  bool send_batch(const std::vector<AnalyticsEvent>& events);

  /**
   * Check if sender is running
   */
  bool is_running() const;

  /**
   * Get number of pending events
   */
  size_t get_pending_count() const;

  /**
   * Get last error message (if any)
   */
  const std::string& get_last_error() const;

  /**
   * Get number of failed sends
   */
  uint32_t get_failed_sends() const;

  /**
   * Get number of successful sends
   */
  uint32_t get_successful_sends() const;

private:
  BatchConfig config_;
  bool running_ = false;
  std::string last_error_;
  uint32_t failed_sends_ = 0;
  uint32_t successful_sends_ = 0;

  /**
   * Internal method to send HTTP request to GrowthBook API
   */
  bool send_to_growthbook(const std::vector<AnalyticsEvent>& events);

  /**
   * Serialize events to JSON for transmission
   */
  std::string serialize_batch(const std::vector<AnalyticsEvent>& events);
};

}  // namespace bendcore
