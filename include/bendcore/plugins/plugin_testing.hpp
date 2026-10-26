#pragma once

#include "plugin_api.hpp"
#include <vector>
#include <string>
#include <functional>

namespace bendcore::plugins {

/**
 * Test result for a plugin test
 */
struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
    double execution_time_ms;
};

/**
 * Plugin testing framework
 */
class PluginTestFramework {
public:
    /**
     * Register a test for a plugin
     */
    void RegisterTest(const std::string& plugin_id, const std::string& test_name,
                     std::function<bool()> test_func);

    /**
     * Run tests for a plugin
     */
    std::vector<TestResult> RunPluginTests(const std::string& plugin_id);

    /**
     * Run all tests
     */
    std::vector<TestResult> RunAllTests();

    /**
     * Get test count for plugin
     */
    int GetTestCount(const std::string& plugin_id) const;

    /**
     * Clear all tests
     */
    void Clear();

private:
    struct TestCase {
        std::string plugin_id;
        std::string test_name;
        std::function<bool()> test_func;
    };

    std::vector<TestCase> tests_;
};

/**
 * Plugin validator
 */
class PluginValidator {
public:
    /**
     * Validate plugin structure and integrity
     */
    static bool ValidatePlugin(const std::shared_ptr<IPlugin>& plugin);

    /**
     * Check if plugin implements all required methods
     */
    static bool CheckRequiredMethods(const std::shared_ptr<IPlugin>& plugin);

    /**
     * Verify plugin metadata
     */
    static bool VerifyMetadata(const PluginMetadata& metadata);

    /**
     * Test plugin initialization
     */
    static bool TestInitialization(const std::shared_ptr<IPlugin>& plugin);

    /**
     * Test plugin configuration
     */
    static bool TestConfiguration(const std::shared_ptr<IPlugin>& plugin);
};

/**
 * Plugin benchmark
 */
class PluginBenchmark {
public:
    /**
     * Benchmark plugin initialization time
     */
    static double BenchmarkInitialization(const std::shared_ptr<IPlugin>& plugin, int iterations = 100);

    /**
     * Benchmark hook execution
     */
    static double BenchmarkHookExecution(const std::shared_ptr<IPlugin>& plugin, int iterations = 1000);

    /**
     * Get plugin memory usage estimate
     */
    static size_t EstimateMemoryUsage(const std::shared_ptr<IPlugin>& plugin);
};

} // namespace bendcore::plugins
