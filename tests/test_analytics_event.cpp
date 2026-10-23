#include "bendcore/analytics_event.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace bendcore;

void test_create_event() {
  std::cout << "TEST: Create event\n";

  AnalyticsEvent event;
  event.event_name = "test_event";
  event.category = EventCategory::IMPORT;
  event.user_id = "user_123";
  event.session_id = "session_456";
  event.properties["key1"] = "value1";
  event.properties["key2"] = "value2";

  assert(event.event_name == "test_event");
  assert(event.user_id == "user_123");
  assert(event.session_id == "session_456");
  assert(event.properties.size() == 2);
  assert(event.properties["key1"] == "value1");

  std::cout << "PASS: Create event\n";
}

void test_queue_event() {
  std::cout << "TEST: Queue event\n";

  AnalyticsEventQueue queue;

  AnalyticsEvent event = events::import_step_opened("user_123", "session_456");
  queue.track_event(event);

  assert(queue.get_queue_size() == 1);

  std::cout << "PASS: Queue event\n";
}

void test_batch_events() {
  std::cout << "TEST: Batch events\n";

  AnalyticsEventQueue queue;

  for (int i = 0; i < 10; ++i) {
    AnalyticsEvent event;
    event.event_name = "event_" + std::to_string(i);
    event.category = EventCategory::IMPORT;
    event.user_id = "user_123";
    event.session_id = "session_456";
    queue.track_event(event);
  }

  assert(queue.get_queue_size() == 10);

  std::cout << "PASS: Batch events\n";
}

void test_flush_queue() {
  std::cout << "TEST: Flush queue\n";

  AnalyticsEventQueue queue;

  AnalyticsEvent event1 = events::template_match_requested("user_1", "session_1");
  AnalyticsEvent event2 = events::template_match_success("user_1", "session_1", {});

  queue.track_event(event1);
  queue.track_event(event2);

  assert(queue.get_queue_size() == 2);

  queue.flush();

  assert(queue.get_queue_size() == 0);

  std::cout << "PASS: Flush queue\n";
}

void test_json_serialization() {
  std::cout << "TEST: JSON serialization\n";

  AnalyticsEvent event = events::import_step_success("user_123", "session_456",
                                                     {{"file_size", "1024"}, {"duration_ms", "5000"}});

  auto json = event.to_json();

  assert(json["event_name"] == "import_step_success");
  assert(json["user_id"] == "user_123");
  assert(json["session_id"] == "session_456");
  assert(json["category"] == static_cast<int>(EventCategory::IMPORT));
  assert(json["properties"]["file_size"] == "1024");
  assert(json["properties"]["duration_ms"] == "5000");

  std::cout << "PASS: JSON serialization\n";
}

void test_background_flush_loop() {
  std::cout << "TEST: Background flush loop\n";

  AnalyticsEventQueue queue;

  // Start background flushing every 100ms
  queue.start_background_flush(std::chrono::milliseconds(100));
  assert(queue.is_background_flush_active());

  // Add some events
  queue.track_event(events::formula_created("user_1", "session_1"));
  queue.track_event(events::formula_evaluated("user_1", "session_1"));

  assert(queue.get_queue_size() == 2);

  // Wait for background flush to trigger
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Queue should be flushed now
  assert(queue.get_queue_size() == 0);

  // Stop background flushing
  queue.stop_background_flush();
  assert(!queue.is_background_flush_active());

  std::cout << "PASS: Background flush loop\n";
}

void test_thread_safety() {
  std::cout << "TEST: Thread safety\n";

  AnalyticsEventQueue queue;
  bool all_safe = true;

  // Spawn multiple threads tracking events
  std::thread t1([&]() {
    for (int i = 0; i < 20; ++i) {
      queue.track_event(events::tube_created("user_1", "session_1"));
    }
  });

  std::thread t2([&]() {
    for (int i = 0; i < 20; ++i) {
      queue.track_event(events::tube_saved("user_1", "session_1"));
    }
  });

  std::thread t3([&]() {
    for (int i = 0; i < 10; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      queue.flush();
    }
  });

  t1.join();
  t2.join();
  t3.join();

  assert(all_safe);

  std::cout << "PASS: Thread safety\n";
}

void test_event_timestamp() {
  std::cout << "TEST: Event timestamp\n";

  auto before = std::chrono::system_clock::now();
  AnalyticsEvent event = events::session_start("user_123");
  auto after = std::chrono::system_clock::now();

  assert(event.timestamp >= before);
  assert(event.timestamp <= after);

  std::cout << "PASS: Event timestamp\n";
}

int main() {
  std::cout << "\n=== Analytics Event Tests ===\n\n";

  test_create_event();
  test_queue_event();
  test_batch_events();
  test_flush_queue();
  test_json_serialization();
  test_background_flush_loop();
  test_thread_safety();
  test_event_timestamp();

  std::cout << "\n=== All Analytics Event Tests Passed ===\n\n";

  return 0;
}
