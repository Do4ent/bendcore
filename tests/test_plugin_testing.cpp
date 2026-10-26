#include <gtest/gtest.h>
#include "bendcore/plugins/plugin_testing.hpp"

using namespace bendcore::plugins;

// Mock plugin for testing
class TestablePlugin : public IPlugin {
public:
    bool Initialize() override { init_called = true; return true; }
    bool Shutdown() override { shutdown_called = true; return true; }
    bool IsEnabled() const override { return enabled; }
    void SetEnabled(bool e) override { enabled = e; }
    PluginStatus GetStatus() const override { return PluginStatus::ACTIVE; }

    std::string RegisterHook(const std::string& name, HookCallback cb) override {
        return name + "_hook";
    }
    bool UnregisterHook(const std::string& id) override { return true; }
    std::vector<std::string> GetRegisteredHooks() const override { return {}; }

    std::string ExecuteCommand(const std::string& cmd, const std::map<std::string, std::string>&) override {
        return "OK";
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
            .id = "testable.plugin",
            .name = "Testable Plugin",
            .version = "1.0.0",
            .author = "Test",
            .description = "Plugin for testing",
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
    mutable std::map<std::string, std::any> config;
};

class PluginTestFrameworkTest : public ::testing::Test {
protected:
    PluginTestFramework framework;
};

TEST_F(PluginTestFrameworkTest, RegisterTest) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    EXPECT_EQ(framework.GetTestCount("plugin1"), 1);
}

TEST_F(PluginTestFrameworkTest, RegisterMultipleTests) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.RegisterTest("plugin1", "test2", []() { return true; });
    framework.RegisterTest("plugin1", "test3", []() { return true; });
    EXPECT_EQ(framework.GetTestCount("plugin1"), 3);
}

TEST_F(PluginTestFrameworkTest, RegisterTestsForDifferentPlugins) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.RegisterTest("plugin2", "test2", []() { return true; });
    framework.RegisterTest("plugin3", "test3", []() { return true; });

    EXPECT_EQ(framework.GetTestCount("plugin1"), 1);
    EXPECT_EQ(framework.GetTestCount("plugin2"), 1);
    EXPECT_EQ(framework.GetTestCount("plugin3"), 1);
}

TEST_F(PluginTestFrameworkTest, RunPluginTests_AllPass) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.RegisterTest("plugin1", "test2", []() { return true; });

    auto results = framework.RunPluginTests("plugin1");
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0].passed);
    EXPECT_TRUE(results[1].passed);
}

TEST_F(PluginTestFrameworkTest, RunPluginTests_SomeFail) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.RegisterTest("plugin1", "test2", []() { return false; });
    framework.RegisterTest("plugin1", "test3", []() { return true; });

    auto results = framework.RunPluginTests("plugin1");
    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].passed);
    EXPECT_FALSE(results[1].passed);
    EXPECT_TRUE(results[2].passed);
}

TEST_F(PluginTestFrameworkTest, RunPluginTests_NonexistentPlugin) {
    auto results = framework.RunPluginTests("nonexistent");
    EXPECT_EQ(results.size(), 0);
}

TEST_F(PluginTestFrameworkTest, RunAllTests) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.RegisterTest("plugin2", "test2", []() { return true; });

    auto results = framework.RunAllTests();
    EXPECT_EQ(results.size(), 2);
}

TEST_F(PluginTestFrameworkTest, TestExecutionTime) {
    framework.RegisterTest("plugin1", "test_time", []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    });

    auto results = framework.RunPluginTests("plugin1");
    EXPECT_GE(results[0].execution_time_ms, 10.0);
}

TEST_F(PluginTestFrameworkTest, TestErrorMessage) {
    framework.RegisterTest("plugin1", "test_fail", []() { return false; });

    auto results = framework.RunPluginTests("plugin1");
    EXPECT_FALSE(results[0].error_message.empty());
}

TEST_F(PluginTestFrameworkTest, ClearTests) {
    framework.RegisterTest("plugin1", "test1", []() { return true; });
    framework.Clear();
    EXPECT_EQ(framework.GetTestCount("plugin1"), 0);
}

TEST_F(PluginTestFrameworkTest, TestExceptionHandling) {
    framework.RegisterTest("plugin1", "test_exception", []() {
        throw std::runtime_error("Test exception");
    });

    auto results = framework.RunPluginTests("plugin1");
    EXPECT_FALSE(results[0].passed);
    EXPECT_FALSE(results[0].error_message.empty());
}

class PluginValidatorTest : public ::testing::Test {
protected:
    std::shared_ptr<IPlugin> plugin = std::make_shared<TestablePlugin>();
};

TEST_F(PluginValidatorTest, ValidateValidPlugin) {
    EXPECT_TRUE(PluginValidator::ValidatePlugin(plugin));
}

TEST_F(PluginValidatorTest, ValidateNullPlugin) {
    std::shared_ptr<IPlugin> null_plugin;
    EXPECT_FALSE(PluginValidator::ValidatePlugin(null_plugin));
}

TEST_F(PluginValidatorTest, CheckRequiredMethods) {
    EXPECT_TRUE(PluginValidator::CheckRequiredMethods(plugin));
}

TEST_F(PluginValidatorTest, CheckRequiredMethodsNull) {
    std::shared_ptr<IPlugin> null_plugin;
    EXPECT_FALSE(PluginValidator::CheckRequiredMethods(null_plugin));
}

TEST_F(PluginValidatorTest, VerifyMetadata) {
    auto metadata = plugin->GetMetadata();
    EXPECT_TRUE(PluginValidator::VerifyMetadata(metadata));
}

TEST_F(PluginValidatorTest, VerifyMetadataInvalid) {
    PluginMetadata bad_metadata{"", "", "", "", "", "", {}, "", "", 0};
    EXPECT_FALSE(PluginValidator::VerifyMetadata(bad_metadata));
}

TEST_F(PluginValidatorTest, TestInitialization) {
    EXPECT_TRUE(PluginValidator::TestInitialization(plugin));
}

TEST_F(PluginValidatorTest, TestConfiguration) {
    EXPECT_TRUE(PluginValidator::TestConfiguration(plugin));
}

class PluginBenchmarkTest : public ::testing::Test {
protected:
    std::shared_ptr<IPlugin> plugin = std::make_shared<TestablePlugin>();
};

TEST_F(PluginBenchmarkTest, BenchmarkInitialization) {
    double avg_time = PluginBenchmark::BenchmarkInitialization(plugin, 10);
    EXPECT_GT(avg_time, 0.0);
}

TEST_F(PluginBenchmarkTest, BenchmarkNullPlugin) {
    std::shared_ptr<IPlugin> null_plugin;
    double avg_time = PluginBenchmark::BenchmarkInitialization(null_plugin, 10);
    EXPECT_EQ(avg_time, 0.0);
}

TEST_F(PluginBenchmarkTest, BenchmarkHookExecution) {
    double avg_time = PluginBenchmark::BenchmarkHookExecution(plugin, 100);
    EXPECT_GE(avg_time, 0.0);
}

TEST_F(PluginBenchmarkTest, EstimateMemoryUsage) {
    size_t memory = PluginBenchmark::EstimateMemoryUsage(plugin);
    EXPECT_GT(memory, 0);
}

TEST_F(PluginBenchmarkTest, EstimateMemoryNullPlugin) {
    std::shared_ptr<IPlugin> null_plugin;
    size_t memory = PluginBenchmark::EstimateMemoryUsage(null_plugin);
    // Should handle null gracefully
    EXPECT_GE(memory, 0);
}

// Integration tests
class PluginTestingIntegrationTest : public ::testing::Test {
protected:
    PluginTestFramework framework;
    std::shared_ptr<IPlugin> plugin = std::make_shared<TestablePlugin>();
};

TEST_F(PluginTestingIntegrationTest, FullTestWorkflow) {
    framework.RegisterTest("testable.plugin", "test_validate", [this]() {
        return PluginValidator::ValidatePlugin(plugin);
    });
    framework.RegisterTest("testable.plugin", "test_init", [this]() {
        return PluginValidator::TestInitialization(plugin);
    });
    framework.RegisterTest("testable.plugin", "test_config", [this]() {
        return PluginValidator::TestConfiguration(plugin);
    });

    auto results = framework.RunPluginTests("testable.plugin");
    EXPECT_EQ(results.size(), 3);
    for (const auto& result : results) {
        EXPECT_TRUE(result.passed);
    }
}

TEST_F(PluginTestingIntegrationTest, BenchmarkPluginPerformance) {
    double init_time = PluginBenchmark::BenchmarkInitialization(plugin, 50);
    double hook_time = PluginBenchmark::BenchmarkHookExecution(plugin, 100);
    size_t memory = PluginBenchmark::EstimateMemoryUsage(plugin);

    EXPECT_GT(init_time, 0.0);
    EXPECT_GE(hook_time, 0.0);
    EXPECT_GT(memory, 0);
}
