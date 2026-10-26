#include <gtest/gtest.h>
#include "bendcore/plugins/plugin_loader.hpp"
#include <cstring>
#include <fstream>

using namespace bendcore::plugins;

// Mock plugin for testing
class MockTestPlugin : public IPlugin {
public:
    bool Initialize() override {
        init_called = true;
        return true;
    }

    bool Shutdown() override {
        shutdown_called = true;
        return true;
    }

    bool IsEnabled() const override { return enabled; }
    void SetEnabled(bool e) override { enabled = e; }
    PluginStatus GetStatus() const override { return status; }

    std::string RegisterHook(const std::string& name, HookCallback cb) override {
        return name + "_hook";
    }

    bool UnregisterHook(const std::string& id) override { return true; }
    std::vector<std::string> GetRegisteredHooks() const override { return {}; }

    std::string ExecuteCommand(const std::string& cmd, const std::map<std::string, std::string>&) override {
        return "OK:" + cmd;
    }

    std::map<std::string, std::any> GetConfiguration() const override { return config; }
    bool SetConfiguration(const std::map<std::string, std::any>& c) override {
        config = c;
        return true;
    }

    std::string GetVersion() const override { return "1.0.0"; }
    std::string GetRequiredAPIVersion() const override { return "1.0.0"; }
    bool Validate() const override { return true; }

    PluginMetadata GetMetadata() const override {
        return PluginMetadata{
            .id = "test.mock",
            .name = "Mock Test Plugin",
            .version = "1.0.0",
            .author = "Test",
            .description = "Mock plugin",
            .license = "MIT",
            .tags = {},
            .documentation_url = "",
            .repository_url = "",
            .permissions = 0
        };
    }

    bool init_called = false;
    bool shutdown_called = false;
    bool enabled = true;
    PluginStatus status = PluginStatus::UNLOADED;
    mutable std::map<std::string, std::any> config;
};

class PluginLoaderTest : public ::testing::Test {
protected:
    PluginLoader loader;
};

// Test PluginLoader basic functionality
TEST_F(PluginLoaderTest, InitialState) {
    EXPECT_EQ(loader.GetPluginCount(), 0);
    EXPECT_TRUE(loader.GetAllPlugins().empty());
}

TEST_F(PluginLoaderTest, GetNonexistentPlugin) {
    auto plugin = loader.GetPlugin("nonexistent");
    EXPECT_EQ(plugin, nullptr);
}

TEST_F(PluginLoaderTest, GetPluginByNonexistentName) {
    auto plugin = loader.GetPluginByName("Nonexistent Plugin");
    EXPECT_EQ(plugin, nullptr);
}

TEST_F(PluginLoaderTest, PluginStatusUnloaded) {
    PluginStatus status = loader.GetPluginStatus("nonexistent");
    EXPECT_EQ(status, PluginStatus::UNLOADED);
}

TEST_F(PluginLoaderTest, PluginDirectoryOperations) {
    EXPECT_EQ(loader.GetPluginDirectory(), ".");
    loader.SetPluginDirectory("/test/path");
    EXPECT_EQ(loader.GetPluginDirectory(), "/test/path");
    loader.SetPluginDirectory(".");  // Reset
}

TEST_F(PluginLoaderTest, LoadNonexistentPlugin) {
    std::string id = loader.LoadPlugin("/nonexistent/path/plugin.so", false);
    EXPECT_EQ(id, "");
    EXPECT_FALSE(loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, UnloadNonexistentPlugin) {
    bool result = loader.UnloadPlugin("nonexistent");
    EXPECT_FALSE(result);
    EXPECT_FALSE(loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, InitializeNonexistentPlugin) {
    bool result = loader.InitializePlugin("nonexistent");
    EXPECT_FALSE(result);
    EXPECT_FALSE(loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, ShutdownNonexistentPlugin) {
    bool result = loader.ShutdownPlugin("nonexistent");
    EXPECT_FALSE(result);
    EXPECT_FALSE(loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, SetEnabledNonexistentPlugin) {
    bool result = loader.SetPluginEnabled("nonexistent", true);
    EXPECT_FALSE(result);
}

TEST_F(PluginLoaderTest, GetLastErrorInitial) {
    PluginLoader fresh_loader;
    EXPECT_TRUE(fresh_loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, ValidateDependenciesEmpty) {
    EXPECT_TRUE(loader.ValidateDependencies());
}

TEST_F(PluginLoaderTest, ResolveDependencyOrderEmpty) {
    auto order = loader.ResolveDependencyOrder();
    EXPECT_TRUE(order.empty());
}

TEST_F(PluginLoaderTest, UnloadAllEmpty) {
    EXPECT_TRUE(loader.UnloadAll());
}

TEST_F(PluginLoaderTest, ClearEmpty) {
    loader.Clear();
    EXPECT_EQ(loader.GetPluginCount(), 0);
}

TEST_F(PluginLoaderTest, GetLoadedPluginsInfoEmpty) {
    auto info = loader.GetLoadedPluginsInfo();
    EXPECT_TRUE(info.empty());
}

TEST_F(PluginLoaderTest, GetAllPluginsEmpty) {
    auto plugins = loader.GetAllPlugins();
    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginLoaderTest, IsPluginLoadedNonexistent) {
    EXPECT_FALSE(loader.IsPluginLoaded("nonexistent"));
}

TEST_F(PluginLoaderTest, GetPluginLoadTimeNonexistent) {
    double time = loader.GetPluginLoadTime("nonexistent");
    EXPECT_EQ(time, 0.0);
}

TEST_F(PluginLoaderTest, VerifyPluginVersionNonexistent) {
    bool result = loader.VerifyPluginVersion("nonexistent", "1.0.0");
    EXPECT_FALSE(result);
}

// PluginLoaderManager tests
class PluginLoaderManagerTest : public ::testing::Test {};

TEST_F(PluginLoaderManagerTest, SingletonInstance) {
    auto& mgr1 = PluginLoaderManager::GetInstance();
    auto& mgr2 = PluginLoaderManager::GetInstance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(PluginLoaderManagerTest, CreateLoader) {
    auto& mgr = PluginLoaderManager::GetInstance();
    auto& loader = mgr.CreateLoader("context1");
    EXPECT_EQ(loader.GetPluginCount(), 0);
}

TEST_F(PluginLoaderManagerTest, GetLoader) {
    auto& mgr = PluginLoaderManager::GetInstance();
    mgr.CreateLoader("context2");
    auto* loader = mgr.GetLoader("context2");
    EXPECT_NE(loader, nullptr);
}

TEST_F(PluginLoaderManagerTest, GetNonexistentLoader) {
    auto& mgr = PluginLoaderManager::GetInstance();
    auto* loader = mgr.GetLoader("nonexistent_context");
    EXPECT_EQ(loader, nullptr);
}

TEST_F(PluginLoaderManagerTest, RemoveLoader) {
    auto& mgr = PluginLoaderManager::GetInstance();
    mgr.CreateLoader("context3");
    bool result = mgr.RemoveLoader("context3");
    EXPECT_TRUE(result);
    EXPECT_EQ(mgr.GetLoader("context3"), nullptr);
}

TEST_F(PluginLoaderManagerTest, RemoveNonexistentLoader) {
    auto& mgr = PluginLoaderManager::GetInstance();
    bool result = mgr.RemoveLoader("nonexistent_context");
    EXPECT_FALSE(result);
}

TEST_F(PluginLoaderManagerTest, GetAllContexts) {
    auto& mgr = PluginLoaderManager::GetInstance();
    // Clear previous loaders
    for (const auto& ctx : mgr.GetAllContexts()) {
        mgr.RemoveLoader(ctx);
    }

    mgr.CreateLoader("ctx1");
    mgr.CreateLoader("ctx2");

    auto contexts = mgr.GetAllContexts();
    EXPECT_EQ(contexts.size(), 2);
    EXPECT_TRUE(std::find(contexts.begin(), contexts.end(), "ctx1") != contexts.end());
    EXPECT_TRUE(std::find(contexts.begin(), contexts.end(), "ctx2") != contexts.end());
}

// Version validator integration tests with loader
TEST_F(PluginLoaderTest, VersionValidation) {
    // This tests the integration of VersionValidator with PluginLoader
    bool compatible = VersionValidator::IsCompatible("1.0.0", "1.0.0");
    EXPECT_TRUE(compatible);

    compatible = VersionValidator::IsCompatible("1.0.0", "1.1.0");
    EXPECT_TRUE(compatible);

    compatible = VersionValidator::IsCompatible("1.0.0", "0.9.0");
    EXPECT_FALSE(compatible);
}

// Tests for LoadPluginsFromDirectory
TEST_F(PluginLoaderTest, LoadFromNonexistentDirectory) {
    int count = loader.LoadPluginsFromDirectory("/nonexistent/directory");
    EXPECT_EQ(count, 0);
    EXPECT_FALSE(loader.GetLastError().empty());
}

TEST_F(PluginLoaderTest, PreLoadCallback) {
    bool callback_called = false;
    loader.RegisterPreLoadCallback([&callback_called](const std::string&) {
        callback_called = true;
        return false;  // Reject load
    });

    std::string id = loader.LoadPlugin("/nonexistent/plugin.so", false);
    EXPECT_EQ(id, "");
    // Note: callback is called even with nonexistent file,
    // but it rejects before file check in real implementation
}

TEST_F(PluginLoaderTest, PostLoadCallback) {
    bool callback_called = false;
    loader.RegisterPostLoadCallback([&callback_called](const std::string&, bool) {
        callback_called = true;
    });
    // Would be tested with actual plugin loading
}

TEST_F(PluginLoaderTest, GetPluginsByType) {
    auto plugins = loader.GetPluginsByType(PluginType::UTILITY);
    EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginLoaderTest, ClearCallsCleansUp) {
    loader.SetPluginDirectory("/test");
    loader.Clear();
    EXPECT_EQ(loader.GetPluginCount(), 0);
}

TEST_F(PluginLoaderTest, CheckDependencyNonexistent) {
    bool result = loader.CheckDependency("plugin1", "plugin2");
    EXPECT_FALSE(result);
}

} // namespace bendcore::plugins
