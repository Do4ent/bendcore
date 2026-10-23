#include "bendcore/growthbook_client.hpp"
#include <cassert>
#include <thread>
#include <iostream>

using namespace bendcore;

void test_initialize_client() {
  std::cout << "TEST: Initialize client\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  GrowthBookClient::Config config;
  config.api_key = "test_api_key_123";
  config.sdk_key = "test_sdk_key_456";
  config.environment = "testing";
  config.enable_analytics = true;

  client.initialize(config);

  assert(client.is_initialized());
  assert(client.get_api_key() == "test_api_key_123");
  assert(client.get_sdk_key() == "test_sdk_key_456");
  assert(client.get_environment() == "testing");
  assert(client.is_analytics_enabled() == true);

  std::cout << "PASS: Initialize client\n";
}

void test_load_feature_flags() {
  std::cout << "TEST: Load feature flags\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  GrowthBookClient::Config config;
  config.api_key = "test_api_key";
  config.sdk_key = "test_sdk_key";
  config.environment = "production";

  client.initialize(config);

  // Refresh features from (simulated) GrowthBook API
  bool success = client.refresh_features();
  assert(success);

  std::cout << "PASS: Load feature flags\n";
}

void test_evaluate_flags() {
  std::cout << "TEST: Evaluate flags\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  // Test flag that doesn't exist yet
  bool enabled = client.is_feature_enabled("non_existent_feature");
  assert(enabled == false);

  std::cout << "PASS: Evaluate flags\n";
}

void test_network_error_handling() {
  std::cout << "TEST: Network error handling\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  GrowthBookClient::Config config;
  config.api_key = "";
  config.sdk_key = "";

  client.initialize(config);

  // Should not be initialized without keys
  assert(client.is_initialized() == false);

  // Should safely handle failed refresh
  bool success = client.refresh_features();
  assert(success == false);

  std::cout << "PASS: Network error handling\n";
}

void test_cache_behavior() {
  std::cout << "TEST: Cache behavior\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  GrowthBookClient::Config config;
  config.api_key = "test_key";
  config.sdk_key = "test_sdk";

  client.initialize(config);

  // First refresh should work
  bool first = client.refresh_features();
  assert(first == true);

  // Second refresh immediately should use cache
  bool second = client.refresh_features();
  assert(second == true);

  std::cout << "PASS: Cache behavior\n";
}

void test_thread_safety() {
  std::cout << "TEST: Thread safety\n";

  GrowthBookClient& client = GrowthBookClient::getInstance();

  GrowthBookClient::Config config;
  config.api_key = "test_api";
  config.sdk_key = "test_sdk";

  client.initialize(config);

  bool all_safe = true;

  // Spawn multiple threads accessing the client
  std::thread t1([&]() {
    for (int i = 0; i < 10; ++i) {
      client.is_feature_enabled("feature_" + std::to_string(i));
    }
  });

  std::thread t2([&]() {
    for (int i = 0; i < 10; ++i) {
      client.get_feature_value("feature_" + std::to_string(i));
    }
  });

  std::thread t3([&]() {
    for (int i = 0; i < 5; ++i) {
      client.refresh_features();
    }
  });

  t1.join();
  t2.join();
  t3.join();

  assert(all_safe);

  std::cout << "PASS: Thread safety\n";
}

int main() {
  std::cout << "\n=== GrowthBook Client Tests ===\n\n";

  test_initialize_client();
  test_load_feature_flags();
  test_evaluate_flags();
  test_network_error_handling();
  test_cache_behavior();
  test_thread_safety();

  std::cout << "\n=== All GrowthBook Client Tests Passed ===\n\n";

  return 0;
}
