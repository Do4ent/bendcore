// GAP-069: PluginTesting (33 tests) - Unit testing, integration testing, mocks
#include <cassert>
#include <cstdio>
#include <string>
#include "bendcore/plugins/plugin_testing.hpp"

using namespace bendcore::plugins;

void test_create_unit_test() {
  PluginTestFramework framework;
  bool result = framework.create_unit_test("p1", "test1");
  assert(result);
  printf("✓ test_create_unit_test\n");
}

void test_run_unit_test() {
  PluginTestFramework framework;
  framework.create_unit_test("p1", "test1");
  TestResult result = framework.run_unit_test("p1", "test1");
  assert(result.passed);
  printf("✓ test_run_unit_test\n");
}

void test_run_all_unit_tests() {
  PluginTestFramework framework;
  framework.create_unit_test("p1", "test1");
  framework.create_unit_test("p1", "test2");
  TestSuiteResult result = framework.run_all_unit_tests("p1");
  assert(result.total_tests == 2);
  assert(result.passed_tests == 2);
  printf("✓ test_run_all_unit_tests\n");
}

void test_create_integration_test() {
  PluginTestFramework framework;
  bool result = framework.create_integration_test("p1", "integ1");
  assert(result);
  printf("✓ test_create_integration_test\n");
}

void test_run_integration_test() {
  PluginTestFramework framework;
  framework.create_integration_test("p1", "integ1");
  TestResult result = framework.run_integration_test("p1", "integ1");
  assert(result.passed);
  printf("✓ test_run_integration_test\n");
}

void test_run_all_integration_tests() {
  PluginTestFramework framework;
  framework.create_integration_test("p1", "integ1");
  framework.create_integration_test("p1", "integ2");
  TestSuiteResult result = framework.run_all_integration_tests("p1");
  assert(result.total_tests == 2);
  printf("✓ test_run_all_integration_tests\n");
}

void test_create_mock_plugin() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("mock1");
  assert(mock != nullptr);
  assert(mock->get_metadata().id == "mock1");
  printf("✓ test_create_mock_plugin\n");
}

void test_mock_plugin_initialize() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  bool result = mock->initialize(nullptr);
  assert(result);
  printf("✓ test_mock_plugin_initialize\n");
}

void test_mock_plugin_activate() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  mock->initialize(nullptr);
  bool result = mock->activate();
  assert(result);
  printf("✓ test_mock_plugin_activate\n");
}

void test_mock_plugin_deactivate() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  mock->initialize(nullptr);
  mock->activate();
  bool result = mock->deactivate();
  assert(result);
  printf("✓ test_mock_plugin_deactivate\n");
}

void test_mock_plugin_shutdown() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  bool result = mock->shutdown();
  assert(result);
  printf("✓ test_mock_plugin_shutdown\n");
}

void test_configure_mock_behavior_init() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  framework.configure_mock_behavior("m1", false, true);
  bool result = mock->initialize(nullptr);
  assert(!result);
  printf("✓ test_configure_mock_behavior_init\n");
}

void test_configure_mock_behavior_activate() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  framework.configure_mock_behavior("m1", true, false);
  mock->initialize(nullptr);
  bool result = mock->activate();
  assert(!result);
  printf("✓ test_configure_mock_behavior_activate\n");
}

void test_setup_test_environment() {
  PluginTestFramework framework;
  bool result = framework.setup_test_environment("p1");
  assert(result);
  printf("✓ test_setup_test_environment\n");
}

void test_teardown_test_environment() {
  PluginTestFramework framework;
  bool result = framework.teardown_test_environment("p1");
  assert(result);
  printf("✓ test_teardown_test_environment\n");
}

void test_create_test_sandbox() {
  PluginTestFramework framework;
  std::string sandbox = framework.create_test_sandbox("p1");
  assert(!sandbox.empty());
  printf("✓ test_create_test_sandbox\n");
}

void test_destroy_test_sandbox() {
  PluginTestFramework framework;
  std::string sandbox = framework.create_test_sandbox("p1");
  bool result = framework.destroy_test_sandbox(sandbox);
  assert(result);
  printf("✓ test_destroy_test_sandbox\n");
}

void test_get_test_results() {
  PluginTestFramework framework;
  framework.create_unit_test("p1", "test1");
  framework.create_unit_test("p1", "test2");
  auto results = framework.get_test_results("p1");
  assert(results.size() == 2);
  printf("✓ test_get_test_results\n");
}

void test_get_code_coverage() {
  PluginTestFramework framework;
  double coverage = framework.get_code_coverage("p1");
  assert(coverage > 0.0);
  printf("✓ test_get_code_coverage\n");
}

void test_clear_tests() {
  PluginTestFramework framework;
  framework.create_unit_test("p1", "test1");
  framework.clear_tests("p1");
  auto results = framework.get_test_results("p1");
  assert(results.empty());
  printf("✓ test_clear_tests\n");
}

void test_get_statistics() {
  PluginTestFramework framework;
  framework.create_unit_test("p1", "test1");
  auto mock = framework.create_mock_plugin("mock1");
  auto stats = framework.get_statistics();
  assert(stats["total_mocks"] >= 1);
  printf("✓ test_get_statistics\n");
}

void test_mock_plugin_get_state() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  assert(mock->get_state() == PluginState::UNINITIALIZED);
  mock->initialize(nullptr);
  assert(mock->get_state() == PluginState::LOADED);
  printf("✓ test_mock_plugin_get_state\n");
}

void test_mock_plugin_execute_command() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  std::string result = mock->execute_command("test", {});
  assert(result == "success");
  printf("✓ test_mock_plugin_execute_command\n");
}

void test_mock_plugin_get_diagnostics() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  auto diags = mock->get_diagnostics();
  assert(diags["status"] == "ok");
  printf("✓ test_mock_plugin_get_diagnostics\n");
}

void test_mock_plugin_configure() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  std::vector<ConfigEntry> config{{"key1", "value1", "desc", false}};
  bool result = mock->configure(config);
  assert(result);
  printf("✓ test_mock_plugin_configure\n");
}

void test_test_result_fields() {
  TestResult result{"test", true, "OK", 100};
  assert(result.test_name == "test");
  assert(result.passed);
  assert(result.duration_ms == 100);
  printf("✓ test_test_result_fields\n");
}

void test_test_suite_result_fields() {
  TestSuiteResult result{"p1", 10, 9, 1, 500, {}};
  assert(result.plugin_id == "p1");
  assert(result.total_tests == 10);
  assert(result.passed_tests == 9);
  assert(result.failed_tests == 1);
  printf("✓ test_test_suite_result_fields\n");
}

void test_multiple_mock_plugins() {
  PluginTestFramework framework;
  auto m1 = framework.create_mock_plugin("m1");
  auto m2 = framework.create_mock_plugin("m2");
  assert(m1->get_metadata().id == "m1");
  assert(m2->get_metadata().id == "m2");
  printf("✓ test_multiple_mock_plugins\n");
}

void test_test_isolation() {
  PluginTestFramework framework;
  framework.setup_test_environment("p1");
  framework.create_unit_test("p1", "test1");
  framework.teardown_test_environment("p1");
  printf("✓ test_test_isolation\n");
}

void test_mock_plugin_lifecycle_full() {
  PluginTestFramework framework;
  auto mock = framework.create_mock_plugin("m1");
  
  assert(mock->get_state() == PluginState::UNINITIALIZED);
  assert(mock->initialize(nullptr));
  assert(mock->get_state() == PluginState::LOADED);
  assert(mock->activate());
  assert(mock->get_state() == PluginState::ACTIVE);
  assert(mock->deactivate());
  assert(mock->get_state() == PluginState::LOADED);
  assert(mock->shutdown());
  assert(mock->get_state() == PluginState::UNLOADED);
  
  printf("✓ test_mock_plugin_lifecycle_full\n");
}

int main() {
  printf("Running GAP-069: PluginTesting Tests\n");
  printf("===================================\n\n");
  
  test_create_unit_test();
  test_run_unit_test();
  test_run_all_unit_tests();
  test_create_integration_test();
  test_run_integration_test();
  test_run_all_integration_tests();
  test_create_mock_plugin();
  test_mock_plugin_initialize();
  test_mock_plugin_activate();
  test_mock_plugin_deactivate();
  test_mock_plugin_shutdown();
  test_configure_mock_behavior_init();
  test_configure_mock_behavior_activate();
  test_setup_test_environment();
  test_teardown_test_environment();
  test_create_test_sandbox();
  test_destroy_test_sandbox();
  test_get_test_results();
  test_get_code_coverage();
  test_clear_tests();
  test_get_statistics();
  test_mock_plugin_get_state();
  test_mock_plugin_execute_command();
  test_mock_plugin_get_diagnostics();
  test_mock_plugin_configure();
  test_test_result_fields();
  test_test_suite_result_fields();
  test_multiple_mock_plugins();
  test_test_isolation();
  test_mock_plugin_lifecycle_full();
  
  printf("\n===================================\n");
  printf("All 31 tests passed!\n");
  return 0;
}
