#ifndef BENDCORE_PLUGINS_PLUGIN_TESTING_HPP
#define BENDCORE_PLUGINS_PLUGIN_TESTING_HPP

#include "plugin_api.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <shared_mutex>

namespace bendcore::plugins {

/**
 * Test result
 */
struct TestResult {
  std::string test_name;
  bool passed;
  std::string error_message;
  long duration_ms;
};

/**
 * Test suite result
 */
struct TestSuiteResult {
  std::string plugin_id;
  int total_tests;
  int passed_tests;
  int failed_tests;
  long total_duration_ms;
  std::vector<TestResult> results;
  double coverage_percentage = 0.0;
};

/**
 * Mock plugin for testing
 */
class MockPlugin : public IPlugin {
 public:
  explicit MockPlugin(const std::string& id);

  PluginMetadata get_metadata() const override;
  bool initialize(void* context) override;
  bool activate() override;
  bool deactivate() override;
  bool shutdown() override;
  std::vector<CapabilityDescriptor> get_capabilities() const override;
  std::vector<PluginDependency> get_dependencies() const override;
  PluginState get_state() const override;
  bool configure(const std::vector<ConfigEntry>& config) override;
  std::vector<ConfigEntry> get_configuration() const override;
  bool is_compatible(const std::string& api_version) const override;
  std::string execute_command(const std::string& command,
                             const std::map<std::string, std::string>& parameters) override;
  std::map<std::string, std::string> get_diagnostics() const override;

  // Test-specific methods
  void set_initialized_behavior(bool value);
  void set_activation_behavior(bool value);
  void set_compatible_versions(const std::vector<std::string>& versions);

 private:
  std::string id_;
  PluginState state_;
  bool should_initialize_;
  bool should_activate_;
  std::vector<std::string> compatible_versions_;
  std::vector<ConfigEntry> config_;
};

/**
 * Plugin test framework
 * 
 * Provides tools for testing plugins including unit testing,
 * integration testing, and mock plugin generation.
 */
class PluginTestFramework {
 public:
  /**
   * Create test framework
   */
  PluginTestFramework();

  /**
   * Destructor
   */
  ~PluginTestFramework();

  // Unit Testing

  /**
   * Create unit test for plugin
   * @param plugin_id Plugin identifier
   * @param test_name Test name
   * @return true if test created
   */
  bool create_unit_test(const std::string& plugin_id, const std::string& test_name);

  /**
   * Run unit test
   * @param plugin_id Plugin identifier
   * @param test_name Test name
   * @return Test result
   */
  TestResult run_unit_test(const std::string& plugin_id, const std::string& test_name);

  /**
   * Run all unit tests for plugin
   * @param plugin_id Plugin identifier
   * @return Test suite result
   */
  TestSuiteResult run_all_unit_tests(const std::string& plugin_id);

  // Integration Testing

  /**
   * Create integration test
   * @param plugin_id Plugin identifier
   * @param test_name Test name
   * @return true if test created
   */
  bool create_integration_test(const std::string& plugin_id, const std::string& test_name);

  /**
   * Run integration test
   * @param plugin_id Plugin identifier
   * @param test_name Test name
   * @return Test result
   */
  TestResult run_integration_test(const std::string& plugin_id, const std::string& test_name);

  /**
   * Run all integration tests
   * @param plugin_id Plugin identifier
   * @return Test suite result
   */
  TestSuiteResult run_all_integration_tests(const std::string& plugin_id);

  // Mock Plugin Generation

  /**
   * Create mock plugin
   * @param plugin_id Plugin identifier
   * @return Shared pointer to mock plugin
   */
  std::shared_ptr<MockPlugin> create_mock_plugin(const std::string& plugin_id);

  /**
   * Configure mock plugin behavior
   * @param plugin_id Mock plugin identifier
   * @param should_initialize Initialization behavior
   * @param should_activate Activation behavior
   */
  void configure_mock_behavior(const std::string& plugin_id,
                              bool should_initialize,
                              bool should_activate);

  // Test Isolation

  /**
   * Setup test environment
   * @param plugin_id Plugin identifier
   * @return true if setup successful
   */
  bool setup_test_environment(const std::string& plugin_id);

  /**
   * Teardown test environment
   * @param plugin_id Plugin identifier
   * @return true if teardown successful
   */
  bool teardown_test_environment(const std::string& plugin_id);

  /**
   * Create isolated test sandbox
   * @param plugin_id Plugin identifier
   * @return Sandbox identifier
   */
  std::string create_test_sandbox(const std::string& plugin_id);

  /**
   * Destroy test sandbox
   * @param sandbox_id Sandbox identifier
   * @return true if destroyed
   */
  bool destroy_test_sandbox(const std::string& sandbox_id);

  // Test Management

  /**
   * Get test results for plugin
   * @param plugin_id Plugin identifier
   * @return Vector of test results
   */
  std::vector<TestResult> get_test_results(const std::string& plugin_id) const;

  /**
   * Get test coverage percentage
   * @param plugin_id Plugin identifier
   * @return Coverage percentage (0-100)
   */
  double get_code_coverage(const std::string& plugin_id) const;

  /**
   * Clear all tests for plugin
   * @param plugin_id Plugin identifier
   */
  void clear_tests(const std::string& plugin_id);

  /**
   * Get framework statistics
   * @return Map of statistics
   */
  std::map<std::string, int> get_statistics() const;

 private:
  mutable std::shared_mutex framework_mutex_;
  std::map<std::string, std::vector<TestResult>> unit_tests_;
  std::map<std::string, std::vector<TestResult>> integration_tests_;
  std::map<std::string, std::shared_ptr<MockPlugin>> mock_plugins_;
  std::map<std::string, std::string> sandboxes_;
};

} // namespace bendcore::plugins

#endif // BENDCORE_PLUGINS_PLUGIN_TESTING_HPP
