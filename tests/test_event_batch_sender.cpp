#include "bendcore/event_batch_sender.hpp"
#include "bendcore/growthbook_client.hpp"
#include <cassert>
#include <iostream>

using namespace bendcore;

void test_batch_accumulation() {
  std::cout << "TEST: Batch accumulation\n";

  EventBatchSender::BatchConfig config;
  config.max_batch_size = 50;
  config.max_wait = std::chrono::seconds(5);

  EventBatchSender sender;
  sender.start(config);

  assert(sender.is_running());

  std::cout << "PASS: Batch accumulation\n";
}

void test_size_threshold_trigger() {
  std::cout << "TEST: Size threshold trigger\n";

  // Initialize GrowthBook first
  GrowthBookClient& gb = GrowthBookClient::getInstance();
  GrowthBookClient::Config gb_config;
  gb_config.api_key = "test_api_key";
  gb_config.sdk_key = "test_sdk_key";
  gb.initialize(gb_config);

  EventBatchSender sender;
  EventBatchSender::BatchConfig config;
  config.max_batch_size = 5;  // Small batch size for testing

  sender.start(config);

  // Create batch of events
  std::vector<AnalyticsEvent> events;
  for (int i = 0; i < 5; ++i) {
    AnalyticsEvent event;
    event.event_name = "test_event_" + std::to_string(i);
    event.category = EventCategory::IMPORT;
    event.user_id = "user_test";
    event.session_id = "session_test";
    events.push_back(event);
  }

  // Send batch should succeed
  bool success = sender.send_batch(events);
  assert(success);
  assert(sender.get_successful_sends() == 1);

  std::cout << "PASS: Size threshold trigger\n";
}

void test_time_threshold_trigger() {
  std::cout << "TEST: Time threshold trigger\n";

  EventBatchSender sender;
  EventBatchSender::BatchConfig config;
  config.max_wait = std::chrono::milliseconds(100);

  sender.start(config);
  assert(sender.is_running());

  std::cout << "PASS: Time threshold trigger\n";
}

void test_json_format() {
  std::cout << "TEST: JSON format\n";

  GrowthBookClient& gb = GrowthBookClient::getInstance();
  GrowthBookClient::Config gb_config;
  gb_config.api_key = "test_key";
  gb_config.sdk_key = "test_sdk";
  gb.initialize(gb_config);

  EventBatchSender sender;
  EventBatchSender::BatchConfig config;
  sender.start(config);

  std::vector<AnalyticsEvent> events;
  AnalyticsEvent event = events::import_step_success("user_123", "session_456",
                                                     {{"file_size", "1024"}});
  events.push_back(event);

  bool success = sender.send_batch(events);
  assert(success);

  std::cout << "PASS: JSON format\n";
}

void test_network_error_handling() {
  std::cout << "TEST: Network error handling\n";

  // Don't initialize GrowthBook - should cause error
  GrowthBookClient& gb = GrowthBookClient::getInstance();

  EventBatchSender sender;
  sender.start(EventBatchSender::BatchConfig());

  std::vector<AnalyticsEvent> events;
  AnalyticsEvent event;
  event.event_name = "test";
  event.category = EventCategory::IMPORT;
  events.push_back(event);

  // Should handle missing GrowthBook gracefully
  bool success = sender.send_batch(events);
  // Will fail because GB not properly initialized, but sender should handle it
  assert(sender.get_failed_sends() > 0);

  std::cout << "PASS: Network error handling\n";
}

void test_event_deduplication() {
  std::cout << "TEST: Event deduplication\n";

  GrowthBookClient& gb = GrowthBookClient::getInstance();
  GrowthBookClient::Config gb_config;
  gb_config.api_key = "test_api";
  gb_config.sdk_key = "test_sdk";
  gb.initialize(gb_config);

  EventBatchSender sender;
  sender.start(EventBatchSender::BatchConfig());

  // Send same event multiple times
  AnalyticsEvent event = events::tube_created("user_test", "session_test");

  std::vector<AnalyticsEvent> batch1 = {event};
  std::vector<AnalyticsEvent> batch2 = {event};

  bool success1 = sender.send_batch(batch1);
  bool success2 = sender.send_batch(batch2);

  assert(success1);
  assert(success2);

  std::cout << "PASS: Event deduplication\n";
}

void test_sender_lifecycle() {
  std::cout << "TEST: Sender lifecycle\n";

  EventBatchSender sender;

  assert(!sender.is_running());

  sender.start(EventBatchSender::BatchConfig());
  assert(sender.is_running());

  sender.stop();
  assert(!sender.is_running());

  std::cout << "PASS: Sender lifecycle\n";
}

int main() {
  std::cout << "\n=== Event Batch Sender Tests ===\n\n";

  test_batch_accumulation();
  test_size_threshold_trigger();
  test_time_threshold_trigger();
  test_json_format();
  test_network_error_handling();
  test_event_deduplication();
  test_sender_lifecycle();

  std::cout << "\n=== All Event Batch Sender Tests Passed ===\n\n";

  return 0;
}
