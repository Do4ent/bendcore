#include <gtest/gtest.h>
#include "bendcore/plugins/plugin_api.hpp"
#include <thread>
#include <chrono>

using namespace bendcore::plugins;

// Mock plugin implementation for testing
class TestPlugin : public IPlugin {
public:
    TestPlugin() : enabled_(true), status_(PluginStatus::UNLOADED) {}

    PluginMetadata GetMetadata() const override {
        return PluginMetadata{
            .id = "test.plugin",
            .name = "Test Plugin",
            .version = "1.0.0",
            .author = "Test Author",
            .description = "Test plugin for unit testing",
            .license = "MIT",
            .tags = {"test", "utility"},
            .documentation_url = "https://example.com",
            .repository_url = "https://github.com/example/repo",
            .permissions = static_cast<uint32_t>(PluginPermission::READ_DOCUMENTS)
        };
    }

    bool Initialize() override {
        status_ = PluginStatus::ACTIVE;
        return true;
    }

    bool Shutdown() override {
        status_ = PluginStatus::UNLOADED;
        return true;
    }

    bool IsEnabled() const override {
        return enabled_;
    }

    void SetEnabled(bool enabled) override {
        enabled_ = enabled;
    }

    PluginStatus GetStatus() const override {
        return status_;
    }

    std::string RegisterHook(const std::string& hook_name, HookCallback callback) override {
        std::string hook_id = hook_name + "_" + std::to_string(hook_counter_++);
        hooks_[hook_id] = callback;
        return hook_id;
    }

    bool UnregisterHook(const std::string& hook_id) override {
        return hooks_.erase(hook_id) > 0;
    }

    std::vector<std::string> GetRegisteredHooks() const override {
        std::vector<std::string> result;
        for (const auto& [id, _] : hooks_) {
            result.push_back(id);
        }
        return result;
    }

    std::string ExecuteCommand(const std::string& command,
                              const std::map<std::string, std::string>& args) override {
        return "EXECUTED:" + command;
    }

    std::map<std::string, std::any> GetConfiguration() const override {
        return config_;
    }

    bool SetConfiguration(const std::map<std::string, std::any>& config) override {
        config_ = config;
        return true;
    }

    std::string GetVersion() const override {
        return "1.0.0";
    }

    std::string GetRequiredAPIVersion() const override {
        return PLUGIN_API_VERSION;
    }

    bool Validate() const override {
        return !GetMetadata().id.empty() && !GetMetadata().name.empty();
    }

private:
    bool enabled_;
    PluginStatus status_;
    int hook_counter_ = 0;
    std::map<std::string, HookCallback> hooks_;
    std::map<std::string, std::any> config_;
};

// Tests for VersionValidator
class VersionValidatorTest : public ::testing::Test {};

TEST_F(VersionValidatorTest, ParseVersionValid) {
    int major, minor, patch;
    EXPECT_TRUE(VersionValidator::ParseVersion("1.0.0", major, minor, patch));
    EXPECT_EQ(major, 1);
    EXPECT_EQ(minor, 0);
    EXPECT_EQ(patch, 0);

    EXPECT_TRUE(VersionValidator::ParseVersion("2.5.3", major, minor, patch));
    EXPECT_EQ(major, 2);
    EXPECT_EQ(minor, 5);
    EXPECT_EQ(patch, 3);

    EXPECT_TRUE(VersionValidator::ParseVersion("10.100.1000", major, minor, patch));
    EXPECT_EQ(major, 10);
    EXPECT_EQ(minor, 100);
    EXPECT_EQ(patch, 1000);
}

TEST_F(VersionValidatorTest, ParseVersionInvalid) {
    int major, minor, patch;
    EXPECT_FALSE(VersionValidator::ParseVersion("1.0", major, minor, patch));
    EXPECT_FALSE(VersionValidator::ParseVersion("1.0.0.0", major, minor, patch));
    EXPECT_FALSE(VersionValidator::ParseVersion("a.b.c", major, minor, patch));
    EXPECT_FALSE(VersionValidator::ParseVersion("", major, minor, patch));
    EXPECT_FALSE(VersionValidator::ParseVersion("1", major, minor, patch));
}

TEST_F(VersionValidatorTest, IsCompatibleSameVersion) {
    EXPECT_TRUE(VersionValidator::IsCompatible("1.0.0", "1.0.0"));
    EXPECT_TRUE(VersionValidator::IsCompatible("2.3.4", "2.3.4"));
}

TEST_F(VersionValidatorTest, IsCompatibleMinorHigher) {
    EXPECT_TRUE(VersionValidator::IsCompatible("1.0.0", "1.1.0"));
    EXPECT_TRUE(VersionValidator::IsCompatible("1.2.0", "1.3.5"));
}

TEST_F(VersionValidatorTest, IsCompatiblePatchHigher) {
    EXPECT_TRUE(VersionValidator::IsCompatible("1.0.0", "1.0.1"));
    EXPECT_TRUE(VersionValidator::IsCompatible("1.0.5", "1.0.10"));
}

TEST_F(VersionValidatorTest, IsCompatibleMajorDifferent) {
    EXPECT_FALSE(VersionValidator::IsCompatible("1.0.0", "2.0.0"));
    EXPECT_FALSE(VersionValidator::IsCompatible("2.0.0", "1.5.0"));
}

TEST_F(VersionValidatorTest, IsCompatibleMinorLower) {
    EXPECT_FALSE(VersionValidator::IsCompatible("1.5.0", "1.2.0"));
}

TEST_F(VersionValidatorTest, IsCompatiblePatchLower) {
    EXPECT_FALSE(VersionValidator::IsCompatible("1.0.5", "1.0.2"));
}

TEST_F(VersionValidatorTest, CompareVersionsEqual) {
    EXPECT_EQ(VersionValidator::CompareVersions("1.0.0", "1.0.0"), 0);
    EXPECT_EQ(VersionValidator::CompareVersions("2.5.3", "2.5.3"), 0);
}

TEST_F(VersionValidatorTest, CompareVersionsMajor) {
    EXPECT_EQ(VersionValidator::CompareVersions("1.0.0", "2.0.0"), -1);
    EXPECT_EQ(VersionValidator::CompareVersions("3.0.0", "1.0.0"), 1);
}

TEST_F(VersionValidatorTest, CompareVersionsMinor) {
    EXPECT_EQ(VersionValidator::CompareVersions("1.0.0", "1.1.0"), -1);
    EXPECT_EQ(VersionValidator::CompareVersions("1.2.0", "1.1.0"), 1);
}

TEST_F(VersionValidatorTest, CompareVersionsPatch) {
    EXPECT_EQ(VersionValidator::CompareVersions("1.0.0", "1.0.1"), -1);
    EXPECT_EQ(VersionValidator::CompareVersions("1.0.5", "1.0.2"), 1);
}

// Tests for IPlugin
class PluginAPITest : public ::testing::Test {
protected:
    TestPlugin plugin;
};

TEST_F(PluginAPITest, GetMetadata) {
    auto metadata = plugin.GetMetadata();
    EXPECT_EQ(metadata.id, "test.plugin");
    EXPECT_EQ(metadata.name, "Test Plugin");
    EXPECT_EQ(metadata.version, "1.0.0");
    EXPECT_EQ(metadata.author, "Test Author");
    EXPECT_FALSE(metadata.tags.empty());
}

TEST_F(PluginAPITest, Initialize) {
    EXPECT_TRUE(plugin.Initialize());
    EXPECT_EQ(plugin.GetStatus(), PluginStatus::ACTIVE);
}

TEST_F(PluginAPITest, Shutdown) {
    plugin.Initialize();
    EXPECT_TRUE(plugin.Shutdown());
    EXPECT_EQ(plugin.GetStatus(), PluginStatus::UNLOADED);
}

TEST_F(PluginAPITest, EnableDisable) {
    EXPECT_TRUE(plugin.IsEnabled());
    plugin.SetEnabled(false);
    EXPECT_FALSE(plugin.IsEnabled());
    plugin.SetEnabled(true);
    EXPECT_TRUE(plugin.IsEnabled());
}

TEST_F(PluginAPITest, GetStatus) {
    EXPECT_EQ(plugin.GetStatus(), PluginStatus::UNLOADED);
    plugin.Initialize();
    EXPECT_EQ(plugin.GetStatus(), PluginStatus::ACTIVE);
}

TEST_F(PluginAPITest, RegisterHook) {
    std::string hook_id = plugin.RegisterHook("test_hook", [](const std::string& name, const auto& data) {
        return true;
    });
    EXPECT_FALSE(hook_id.empty());
    EXPECT_TRUE(hook_id.find("test_hook") != std::string::npos);
}

TEST_F(PluginAPITest, RegisterMultipleHooks) {
    std::string hook_id1 = plugin.RegisterHook("hook1", [](const auto&, const auto&) { return true; });
    std::string hook_id2 = plugin.RegisterHook("hook2", [](const auto&, const auto&) { return true; });

    EXPECT_NE(hook_id1, hook_id2);
    EXPECT_FALSE(hook_id1.empty());
    EXPECT_FALSE(hook_id2.empty());
}

TEST_F(PluginAPITest, UnregisterHook) {
    std::string hook_id = plugin.RegisterHook("test_hook", [](const auto&, const auto&) { return true; });
    EXPECT_TRUE(plugin.UnregisterHook(hook_id));
    EXPECT_FALSE(plugin.UnregisterHook(hook_id));  // Should fail second time
}

TEST_F(PluginAPITest, GetRegisteredHooks) {
    plugin.RegisterHook("hook1", [](const auto&, const auto&) { return true; });
    plugin.RegisterHook("hook2", [](const auto&, const auto&) { return true; });

    auto hooks = plugin.GetRegisteredHooks();
    EXPECT_EQ(hooks.size(), 2);
}

TEST_F(PluginAPITest, ExecuteCommand) {
    auto result = plugin.ExecuteCommand("test_command", {});
    EXPECT_EQ(result, "EXECUTED:test_command");

    result = plugin.ExecuteCommand("another_command", {{"arg1", "value1"}});
    EXPECT_EQ(result, "EXECUTED:another_command");
}

TEST_F(PluginAPITest, GetConfiguration) {
    auto config = plugin.GetConfiguration();
    EXPECT_TRUE(config.empty());
}

TEST_F(PluginAPITest, SetConfiguration) {
    std::map<std::string, std::any> config = {
        {"option1", std::string("value1")},
        {"option2", 42}
    };
    EXPECT_TRUE(plugin.SetConfiguration(config));

    auto retrieved = plugin.GetConfiguration();
    EXPECT_EQ(retrieved.size(), 2);
}

TEST_F(PluginAPITest, GetVersion) {
    EXPECT_EQ(plugin.GetVersion(), "1.0.0");
}

TEST_F(PluginAPITest, GetRequiredAPIVersion) {
    EXPECT_EQ(plugin.GetRequiredAPIVersion(), PLUGIN_API_VERSION);
}

TEST_F(PluginAPITest, Validate) {
    EXPECT_TRUE(plugin.Validate());
}

// Tests for HookExecutor
class HookExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        HookExecutor::UnregisterGlobalHook("test_hook");
    }
};

TEST_F(HookExecutorTest, ExecuteNoHooks) {
    std::map<std::string, std::any> data;
    EXPECT_TRUE(HookExecutor::ExecuteHooks("nonexistent_hook", data));
}

TEST_F(HookExecutorTest, RegisterAndExecuteHook) {
    bool called = false;
    HookExecutor::RegisterGlobalHook("test_hook", [&called](const auto&, const auto&) {
        called = true;
        return true;
    });

    std::map<std::string, std::any> data;
    EXPECT_TRUE(HookExecutor::ExecuteHooks("test_hook", data));
    EXPECT_TRUE(called);
}

TEST_F(HookExecutorTest, ExecuteMultipleHooks) {
    int call_count = 0;
    HookExecutor::RegisterGlobalHook("multi_hook", [&call_count](const auto&, const auto&) {
        call_count++;
        return true;
    });
    HookExecutor::RegisterGlobalHook("multi_hook", [&call_count](const auto&, const auto&) {
        call_count++;
        return true;
    });

    std::map<std::string, std::any> data;
    EXPECT_TRUE(HookExecutor::ExecuteHooks("multi_hook", data));
    EXPECT_EQ(call_count, 2);
}

TEST_F(HookExecutorTest, HookWithData) {
    std::string received_data;
    HookExecutor::RegisterGlobalHook("data_hook", [&received_data](const auto& name, const auto& data) {
        if (!data.empty()) {
            received_data = name;
        }
        return true;
    });

    std::map<std::string, std::any> data = {{"key", std::string("value")}};
    EXPECT_TRUE(HookExecutor::ExecuteHooks("data_hook", data));
    EXPECT_EQ(received_data, "data_hook");
}

TEST_F(HookExecutorTest, HookReturnsfalse) {
    bool success = true;
    HookExecutor::RegisterGlobalHook("fail_hook", [&success](const auto&, const auto&) {
        return false;
    });

    std::map<std::string, std::any> data;
    EXPECT_FALSE(HookExecutor::ExecuteHooks("fail_hook", data));
}

// Tests for PluginStatus enum
class PluginStatusTest : public ::testing::Test {};

TEST_F(PluginStatusTest, StatusValues) {
    EXPECT_EQ(static_cast<int>(PluginStatus::UNLOADED), 0);
    EXPECT_EQ(static_cast<int>(PluginStatus::LOADING), 1);
    EXPECT_EQ(static_cast<int>(PluginStatus::LOADED), 2);
    EXPECT_EQ(static_cast<int>(PluginStatus::INITIALIZING), 3);
    EXPECT_EQ(static_cast<int>(PluginStatus::ACTIVE), 4);
    EXPECT_EQ(static_cast<int>(PluginStatus::DISABLED), 5);
    EXPECT_EQ(static_cast<int>(PluginStatus::ERROR), 6);
    EXPECT_EQ(static_cast<int>(PluginStatus::UNLOADING), 7);
}

// Tests for PluginType enum
class PluginTypeTest : public ::testing::Test {};

TEST_F(PluginTypeTest, TypeValues) {
    EXPECT_EQ(static_cast<int>(PluginType::RENDERER), 0);
    EXPECT_EQ(static_cast<int>(PluginType::ANALYZER), 1);
    EXPECT_EQ(static_cast<int>(PluginType::EXPORTER), 2);
    EXPECT_EQ(static_cast<int>(PluginType::IMPORTER), 3);
    EXPECT_EQ(static_cast<int>(PluginType::UTILITY), 4);
    EXPECT_EQ(static_cast<int>(PluginType::EXTENSION), 5);
}

// Tests for PluginPermission flags
class PluginPermissionTest : public ::testing::Test {};

TEST_F(PluginPermissionTest, PermissionFlags) {
    uint32_t all = static_cast<uint32_t>(PluginPermission::ALL_PERMISSIONS);
    EXPECT_GT(all, 0);

    uint32_t read = static_cast<uint32_t>(PluginPermission::READ_DOCUMENTS);
    uint32_t write = static_cast<uint32_t>(PluginPermission::WRITE_DOCUMENTS);
    EXPECT_NE(read, write);
}

TEST_F(PluginPermissionTest, PermissionBitwise) {
    uint32_t perms = static_cast<uint32_t>(PluginPermission::READ_DOCUMENTS) |
                    static_cast<uint32_t>(PluginPermission::WRITE_DOCUMENTS);

    EXPECT_TRUE(perms & static_cast<uint32_t>(PluginPermission::READ_DOCUMENTS));
    EXPECT_TRUE(perms & static_cast<uint32_t>(PluginPermission::WRITE_DOCUMENTS));
}
