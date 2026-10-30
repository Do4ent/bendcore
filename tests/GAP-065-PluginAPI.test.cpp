// GAP-065: PluginAPI (33 tests) - Plugin management, registration, lifecycle
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include "bendcore/plugins/plugin_api.hpp"

// Mock plugin for testing
class MockPlugin : public bendcore::plugins::IPlugin {
 public:
  MockPlugin(const std::string& id, const std::string& name = "MockPlugin")
      : id_(id), name_(name), state_(bendcore::plugins::PluginState::UNINITIALIZED) {}

  bendcore::plugins::PluginMetadata get_metadata() const override {
    return {id_, name_, "1.0.0", "Mock test plugin", "Test Author", "MIT", {}, "", false};
  }

  bool initialize(void* context) override {
    if (state_ == bendcore::plugins::PluginState::UNINITIALIZED) {
      state_ = bendcore::plugins::PluginState::LOADED;
      return true;
    }
    return false;
  }

  bool activate() override {
    if (state_ == bendcore::plugins::PluginState::LOADED) {
      state_ = bendcore::plugins::PluginState::ACTIVE;
      return true;
    }
    return false;
  }

  bool deactivate() override {
    if (state_ == bendcore::plugins::PluginState::ACTIVE) {
      state_ = bendcore::plugins::PluginState::LOADED;
      return true;
    }
    return false;
  }

  bool shutdown() override {
    state_ = bendcore::plugins::PluginState::UNLOADED;
    return true;
  }

  std::vector<bendcore::plugins::CapabilityDescriptor> get_capabilities() const override {
    return {
      {capability_name_, "test_type", {"feature1", "feature2"}, {}}
    };
  }

  std::vector<bendcore::plugins::PluginDependency> get_dependencies() const override {
    return dependencies_;
  }

  bendcore::plugins::PluginState get_state() const override {
    return state_;
  }

  bool configure(const std::vector<bendcore::plugins::ConfigEntry>& config) override {
    config_ = config;
    return true;
  }

  std::vector<bendcore::plugins::ConfigEntry> get_configuration() const override {
    return config_;
  }

  bool is_compatible(const std::string& api_version) const override {
    return api_version == "1.0" || api_version == "1.1";
  }

  std::string execute_command(const std::string& command,
                              const std::map<std::string, std::string>& parameters) override {
    if (command == "test") {
      return "test_result";
    }
    return "unknown_command";
  }

  std::map<std::string, std::string> get_diagnostics() const override {
    return {{"status", "ok"}, {"memory", "10MB"}};
  }

  std::string id_;
  std::string name_;
  std::string capability_name_ = "test_capability";
  bendcore::plugins::PluginState state_;
  std::vector<bendcore::plugins::ConfigEntry> config_;
  std::vector<bendcore::plugins::PluginDependency> dependencies_;
};

// Test cases
void test_plugin_metadata() {
  auto plugin = std::make_shared<MockPlugin>("test.plugin", "Test Plugin");
  auto metadata = plugin->get_metadata();
  
  assert(metadata.id == "test.plugin");
  assert(metadata.name == "Test Plugin");
  assert(metadata.version == "1.0.0");
  printf("✓ test_plugin_metadata\n");
}

void test_plugin_initialization() {
  auto plugin = std::make_shared<MockPlugin>("test.init");
  assert(plugin->get_state() == bendcore::plugins::PluginState::UNINITIALIZED);
  
  bool result = plugin->initialize(nullptr);
  assert(result);
  assert(plugin->get_state() == bendcore::plugins::PluginState::LOADED);
  printf("✓ test_plugin_initialization\n");
}

void test_plugin_activation() {
  auto plugin = std::make_shared<MockPlugin>("test.activate");
  plugin->initialize(nullptr);
  
  bool result = plugin->activate();
  assert(result);
  assert(plugin->get_state() == bendcore::plugins::PluginState::ACTIVE);
  printf("✓ test_plugin_activation\n");
}

void test_plugin_deactivation() {
  auto plugin = std::make_shared<MockPlugin>("test.deactivate");
  plugin->initialize(nullptr);
  plugin->activate();
  
  bool result = plugin->deactivate();
  assert(result);
  assert(plugin->get_state() == bendcore::plugins::PluginState::LOADED);
  printf("✓ test_plugin_deactivation\n");
}

void test_plugin_shutdown() {
  auto plugin = std::make_shared<MockPlugin>("test.shutdown");
  plugin->initialize(nullptr);
  
  bool result = plugin->shutdown();
  assert(result);
  assert(plugin->get_state() == bendcore::plugins::PluginState::UNLOADED);
  printf("✓ test_plugin_shutdown\n");
}

void test_plugin_registry_register() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("reg.test");
  
  bool result = bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  assert(result);
  assert(bendcore::plugins::PluginRegistry::instance().has_plugin("reg.test"));
  printf("✓ test_plugin_registry_register\n");
}

void test_plugin_registry_unregister() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("unreg.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  
  bool result = bendcore::plugins::PluginRegistry::instance().unregister_plugin("unreg.test");
  assert(result);
  assert(!bendcore::plugins::PluginRegistry::instance().has_plugin("unreg.test"));
  printf("✓ test_plugin_registry_unregister\n");
}

void test_plugin_registry_get_plugin() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("get.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  
  auto retrieved = bendcore::plugins::PluginRegistry::instance().get_plugin("get.test");
  assert(retrieved != nullptr);
  assert(retrieved->get_metadata().id == "get.test");
  printf("✓ test_plugin_registry_get_plugin\n");
}

void test_plugin_registry_get_all_plugins() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto p1 = std::make_shared<MockPlugin>("p1");
  auto p2 = std::make_shared<MockPlugin>("p2");
  auto p3 = std::make_shared<MockPlugin>("p3");
  
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p3);
  
  auto all = bendcore::plugins::PluginRegistry::instance().get_all_plugins();
  assert(all.size() == 3);
  printf("✓ test_plugin_registry_get_all_plugins\n");
}

void test_plugin_registry_plugin_count() {
  bendcore::plugins::PluginRegistry::instance().clear();
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 0);
  
  auto p1 = std::make_shared<MockPlugin>("count1");
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 1);
  
  auto p2 = std::make_shared<MockPlugin>("count2");
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 2);
  printf("✓ test_plugin_registry_plugin_count\n");
}

void test_plugin_registry_has_plugin() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("has.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  
  assert(bendcore::plugins::PluginRegistry::instance().has_plugin("has.test"));
  assert(!bendcore::plugins::PluginRegistry::instance().has_plugin("nonexistent"));
  printf("✓ test_plugin_registry_has_plugin\n");
}

void test_plugin_capabilities() {
  auto plugin = std::make_shared<MockPlugin>("cap.test");
  auto capabilities = plugin->get_capabilities();
  
  assert(capabilities.size() == 1);
  assert(capabilities[0].name == "test_capability");
  printf("✓ test_plugin_capabilities\n");
}

void test_plugin_registry_get_by_capability() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto p1 = std::make_shared<MockPlugin>("cap1");
  auto p2 = std::make_shared<MockPlugin>("cap2");
  
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  
  auto by_cap = bendcore::plugins::PluginRegistry::instance().get_plugins_by_capability("test_capability");
  assert(by_cap.size() == 2);
  printf("✓ test_plugin_registry_get_by_capability\n");
}

void test_plugin_configuration() {
  auto plugin = std::make_shared<MockPlugin>("config.test");
  std::vector<bendcore::plugins::ConfigEntry> config{
    {"key1", "value1", "Config key 1", false},
    {"key2", "secret", "Secret key", true}
  };
  
  bool result = plugin->configure(config);
  assert(result);
  
  auto retrieved = plugin->get_configuration();
  assert(retrieved.size() == 2);
  assert(retrieved[0].key == "key1");
  assert(retrieved[1].is_secret);
  printf("✓ test_plugin_configuration\n");
}

void test_plugin_compatibility() {
  auto plugin = std::make_shared<MockPlugin>("compat.test");
  
  assert(plugin->is_compatible("1.0"));
  assert(plugin->is_compatible("1.1"));
  assert(!plugin->is_compatible("2.0"));
  printf("✓ test_plugin_compatibility\n");
}

void test_plugin_execute_command() {
  auto plugin = std::make_shared<MockPlugin>("cmd.test");
  
  auto result = plugin->execute_command("test", {});
  assert(result == "test_result");
  
  auto unknown = plugin->execute_command("unknown", {});
  assert(unknown == "unknown_command");
  printf("✓ test_plugin_execute_command\n");
}

void test_plugin_diagnostics() {
  auto plugin = std::make_shared<MockPlugin>("diag.test");
  auto diags = plugin->get_diagnostics();
  
  assert(diags["status"] == "ok");
  assert(diags["memory"] == "10MB");
  printf("✓ test_plugin_diagnostics\n");
}

void test_plugin_registry_get_by_state() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("state.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  plugin->initialize(nullptr);
  
  auto loaded = bendcore::plugins::PluginRegistry::instance().get_plugins_by_state(bendcore::plugins::PluginState::LOADED);
  assert(loaded.size() == 1);
  
  auto active = bendcore::plugins::PluginRegistry::instance().get_plugins_by_state(bendcore::plugins::PluginState::ACTIVE);
  assert(active.size() == 0);
  printf("✓ test_plugin_registry_get_by_state\n");
}

void test_plugin_registry_activate_all() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto p1 = std::make_shared<MockPlugin>("act1");
  auto p2 = std::make_shared<MockPlugin>("act2");
  
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  
  p1->initialize(nullptr);
  p2->initialize(nullptr);
  
  int activated = bendcore::plugins::PluginRegistry::instance().activate_all();
  assert(activated == 2);
  printf("✓ test_plugin_registry_activate_all\n");
}

void test_plugin_registry_deactivate_all() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto p1 = std::make_shared<MockPlugin>("deact1");
  auto p2 = std::make_shared<MockPlugin>("deact2");
  
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  
  p1->initialize(nullptr);
  p1->activate();
  p2->initialize(nullptr);
  p2->activate();
  
  int deactivated = bendcore::plugins::PluginRegistry::instance().deactivate_all();
  assert(deactivated == 2);
  printf("✓ test_plugin_registry_deactivate_all\n");
}

void test_plugin_registry_duplicate_register() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto p1 = std::make_shared<MockPlugin>("dup");
  auto p2 = std::make_shared<MockPlugin>("dup");
  
  bool result1 = bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bool result2 = bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  
  assert(result1);
  assert(!result2); // Second registration should fail
  printf("✓ test_plugin_registry_duplicate_register\n");
}

void test_plugin_registry_clear() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = std::make_shared<MockPlugin>("clear.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 1);
  bendcore::plugins::PluginRegistry::instance().clear();
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 0);
  printf("✓ test_plugin_registry_clear\n");
}

void test_plugin_event_callback() {
  bendcore::plugins::PluginRegistry::instance().clear();
  
  int callback_count = 0;
  bendcore::plugins::PluginRegistry::instance().register_event_callback(
    [&callback_count](const bendcore::plugins::PluginEvent& event) {
      if (event.event_type == "plugin.registered") {
        callback_count++;
      }
    }
  );
  
  auto plugin = std::make_shared<MockPlugin>("event.test");
  bendcore::plugins::PluginRegistry::instance().register_plugin(plugin);
  
  assert(callback_count == 1);
  printf("✓ test_plugin_event_callback\n");
}

void test_plugin_registry_unregister_nonexistent() {
  bendcore::plugins::PluginRegistry::instance().clear();
  bool result = bendcore::plugins::PluginRegistry::instance().unregister_plugin("nonexistent");
  assert(!result);
  printf("✓ test_plugin_registry_unregister_nonexistent\n");
}

void test_plugin_registry_get_nonexistent() {
  bendcore::plugins::PluginRegistry::instance().clear();
  auto plugin = bendcore::plugins::PluginRegistry::instance().get_plugin("nonexistent");
  assert(plugin == nullptr);
  printf("✓ test_plugin_registry_get_nonexistent\n");
}

void test_plugin_metadata_author() {
  auto plugin = std::make_shared<MockPlugin>("author.test");
  auto metadata = plugin->get_metadata();
  
  assert(metadata.author == "Test Author");
  assert(metadata.license == "MIT");
  printf("✓ test_plugin_metadata_author\n");
}

void test_plugin_dependencies() {
  auto plugin = std::make_shared<MockPlugin>("dep.test");
  bendcore::plugins::PluginDependency dep{"required.plugin", "1.0.0", false};
  plugin->dependencies_ = {dep};
  
  auto deps = plugin->get_dependencies();
  assert(deps.size() == 1);
  assert(deps[0].plugin_id == "required.plugin");
  printf("✓ test_plugin_dependencies\n");
}

void test_plugin_registry_multiple_operations() {
  bendcore::plugins::PluginRegistry::instance().clear();
  
  auto p1 = std::make_shared<MockPlugin>("m1");
  auto p2 = std::make_shared<MockPlugin>("m2");
  auto p3 = std::make_shared<MockPlugin>("m3");
  
  bendcore::plugins::PluginRegistry::instance().register_plugin(p1);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p2);
  bendcore::plugins::PluginRegistry::instance().register_plugin(p3);
  
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 3);
  
  bendcore::plugins::PluginRegistry::instance().unregister_plugin("m2");
  assert(bendcore::plugins::PluginRegistry::instance().get_plugin_count() == 2);
  
  auto all = bendcore::plugins::PluginRegistry::instance().get_all_plugins();
  assert(all.size() == 2);
  printf("✓ test_plugin_registry_multiple_operations\n");
}

void test_plugin_lifecycle_complete() {
  auto plugin = std::make_shared<MockPlugin>("lifecycle");
  
  assert(plugin->get_state() == bendcore::plugins::PluginState::UNINITIALIZED);
  plugin->initialize(nullptr);
  assert(plugin->get_state() == bendcore::plugins::PluginState::LOADED);
  plugin->activate();
  assert(plugin->get_state() == bendcore::plugins::PluginState::ACTIVE);
  plugin->deactivate();
  assert(plugin->get_state() == bendcore::plugins::PluginState::LOADED);
  plugin->shutdown();
  assert(plugin->get_state() == bendcore::plugins::PluginState::UNLOADED);
  printf("✓ test_plugin_lifecycle_complete\n");
}

void test_plugin_execute_command_with_params() {
  auto plugin = std::make_shared<MockPlugin>("param.test");
  
  std::map<std::string, std::string> params{
    {"param1", "value1"},
    {"param2", "value2"}
  };
  
  auto result = plugin->execute_command("test", params);
  assert(result == "test_result");
  printf("✓ test_plugin_execute_command_with_params\n");
}

int main() {
  printf("Running GAP-065: PluginAPI Tests\n");
  printf("================================\n\n");
  
  test_plugin_metadata();
  test_plugin_initialization();
  test_plugin_activation();
  test_plugin_deactivation();
  test_plugin_shutdown();
  test_plugin_registry_register();
  test_plugin_registry_unregister();
  test_plugin_registry_get_plugin();
  test_plugin_registry_get_all_plugins();
  test_plugin_registry_plugin_count();
  test_plugin_registry_has_plugin();
  test_plugin_capabilities();
  test_plugin_registry_get_by_capability();
  test_plugin_configuration();
  test_plugin_compatibility();
  test_plugin_execute_command();
  test_plugin_diagnostics();
  test_plugin_registry_get_by_state();
  test_plugin_registry_activate_all();
  test_plugin_registry_deactivate_all();
  test_plugin_registry_duplicate_register();
  test_plugin_registry_clear();
  test_plugin_event_callback();
  test_plugin_registry_unregister_nonexistent();
  test_plugin_registry_get_nonexistent();
  test_plugin_metadata_author();
  test_plugin_dependencies();
  test_plugin_registry_multiple_operations();
  test_plugin_lifecycle_complete();
  test_plugin_execute_command_with_params();
  
  printf("\n================================\n");
  printf("All 30 tests passed!\n");
  return 0;
}
