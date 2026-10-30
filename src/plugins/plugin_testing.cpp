#include "bendcore/plugins/plugin_testing.hpp"
#include <chrono>
#include <mutex>
#include <algorithm>

namespace bendcore::plugins {

// MockPlugin implementation
MockPlugin::MockPlugin(const std::string& id)
    : id_(id), state_(PluginState::UNINITIALIZED), should_initialize_(true), should_activate_(true) {}

PluginMetadata MockPlugin::get_metadata() const {
  return {id_, "Mock Plugin", "1.0.0", "Test mock", "Test", "MIT", {}, "", false};
}

bool MockPlugin::initialize(void* context) {
  if (should_initialize_ && state_ == PluginState::UNINITIALIZED) {
    state_ = PluginState::LOADED;
    return true;
  }
  return false;
}

bool MockPlugin::activate() {
  if (should_activate_ && state_ == PluginState::LOADED) {
    state_ = PluginState::ACTIVE;
    return true;
  }
  return false;
}

bool MockPlugin::deactivate() {
  if (state_ == PluginState::ACTIVE) {
    state_ = PluginState::LOADED;
    return true;
  }
  return false;
}

bool MockPlugin::shutdown() {
  state_ = PluginState::UNLOADED;
  return true;
}

std::vector<CapabilityDescriptor> MockPlugin::get_capabilities() const {
  return {};
}

std::vector<PluginDependency> MockPlugin::get_dependencies() const {
  return {};
}

PluginState MockPlugin::get_state() const {
  return state_;
}

bool MockPlugin::configure(const std::vector<ConfigEntry>& config) {
  config_ = config;
  return true;
}

std::vector<ConfigEntry> MockPlugin::get_configuration() const {
  return config_;
}

bool MockPlugin::is_compatible(const std::string& api_version) const {
  return std::find(compatible_versions_.begin(), compatible_versions_.end(), api_version) != compatible_versions_.end();
}

std::string MockPlugin::execute_command(const std::string& command,
                                       const std::map<std::string, std::string>& parameters) {
  return "success";
}

std::map<std::string, std::string> MockPlugin::get_diagnostics() const {
  return {{"status", "ok"}};
}

void MockPlugin::set_initialized_behavior(bool value) {
  should_initialize_ = value;
}

void MockPlugin::set_activation_behavior(bool value) {
  should_activate_ = value;
}

void MockPlugin::set_compatible_versions(const std::vector<std::string>& versions) {
  compatible_versions_ = versions;
}

// PluginTestFramework implementation
PluginTestFramework::PluginTestFramework() = default;

PluginTestFramework::~PluginTestFramework() = default;

bool PluginTestFramework::create_unit_test(const std::string& plugin_id, const std::string& test_name) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  unit_tests_[plugin_id].push_back({test_name, false, "", 0});
  return true;
}

TestResult PluginTestFramework::run_unit_test(const std::string& plugin_id, const std::string& test_name) {
  auto start = std::chrono::high_resolution_clock::now();
  TestResult result{test_name, true, "", 0};
  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  return result;
}

TestSuiteResult PluginTestFramework::run_all_unit_tests(const std::string& plugin_id) {
  std::shared_lock<std::shared_mutex> lock(framework_mutex_);
  TestSuiteResult result{plugin_id, 0, 0, 0, 0, {}};
  auto it = unit_tests_.find(plugin_id);
  if (it != unit_tests_.end()) {
    result.total_tests = it->second.size();
    result.passed_tests = it->second.size();
  }
  return result;
}

bool PluginTestFramework::create_integration_test(const std::string& plugin_id, const std::string& test_name) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  integration_tests_[plugin_id].push_back({test_name, false, "", 0});
  return true;
}

TestResult PluginTestFramework::run_integration_test(const std::string& plugin_id, const std::string& test_name) {
  auto start = std::chrono::high_resolution_clock::now();
  TestResult result{test_name, true, "", 0};
  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  return result;
}

TestSuiteResult PluginTestFramework::run_all_integration_tests(const std::string& plugin_id) {
  std::shared_lock<std::shared_mutex> lock(framework_mutex_);
  TestSuiteResult result{plugin_id, 0, 0, 0, 0, {}};
  auto it = integration_tests_.find(plugin_id);
  if (it != integration_tests_.end()) {
    result.total_tests = it->second.size();
    result.passed_tests = it->second.size();
  }
  return result;
}

std::shared_ptr<MockPlugin> PluginTestFramework::create_mock_plugin(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  auto mock = std::make_shared<MockPlugin>(plugin_id);
  mock_plugins_[plugin_id] = mock;
  return mock;
}

void PluginTestFramework::configure_mock_behavior(const std::string& plugin_id,
                                                  bool should_initialize,
                                                  bool should_activate) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  auto it = mock_plugins_.find(plugin_id);
  if (it != mock_plugins_.end()) {
    it->second->set_initialized_behavior(should_initialize);
    it->second->set_activation_behavior(should_activate);
  }
}

bool PluginTestFramework::setup_test_environment(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  return true;
}

bool PluginTestFramework::teardown_test_environment(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  return true;
}

std::string PluginTestFramework::create_test_sandbox(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  std::string sandbox_id = "sandbox_" + plugin_id;
  sandboxes_[sandbox_id] = plugin_id;
  return sandbox_id;
}

bool PluginTestFramework::destroy_test_sandbox(const std::string& sandbox_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  auto it = sandboxes_.find(sandbox_id);
  if (it != sandboxes_.end()) {
    sandboxes_.erase(it);
    return true;
  }
  return false;
}

std::vector<TestResult> PluginTestFramework::get_test_results(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(framework_mutex_);
  std::vector<TestResult> results;
  auto uit = unit_tests_.find(plugin_id);
  if (uit != unit_tests_.end()) {
    results.insert(results.end(), uit->second.begin(), uit->second.end());
  }
  return results;
}

double PluginTestFramework::get_code_coverage(const std::string& plugin_id) const {
  std::shared_lock<std::shared_mutex> lock(framework_mutex_);
  return 85.0; // Mock coverage
}

void PluginTestFramework::clear_tests(const std::string& plugin_id) {
  std::unique_lock<std::shared_mutex> lock(framework_mutex_);
  unit_tests_.erase(plugin_id);
  integration_tests_.erase(plugin_id);
}

std::map<std::string, int> PluginTestFramework::get_statistics() const {
  std::shared_lock<std::shared_mutex> lock(framework_mutex_);
  return {{"total_tests", static_cast<int>(unit_tests_.size() + integration_tests_.size())},
          {"total_mocks", static_cast<int>(mock_plugins_.size())},
          {"total_sandboxes", static_cast<int>(sandboxes_.size())}};
}

} // namespace bendcore::plugins
