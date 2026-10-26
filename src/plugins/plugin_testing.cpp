#include "bendcore/plugins/plugin_testing.hpp"
#include <chrono>
#include <algorithm>

namespace bendcore::plugins {

void PluginTestFramework::RegisterTest(const std::string& plugin_id, const std::string& test_name,
                                      std::function<bool()> test_func) {
    tests_.push_back(TestCase{plugin_id, test_name, test_func});
}

std::vector<TestResult> PluginTestFramework::RunPluginTests(const std::string& plugin_id) {
    std::vector<TestResult> results;

    for (const auto& test : tests_) {
        if (test.plugin_id != plugin_id) continue;

        auto start = std::chrono::high_resolution_clock::now();
        try {
            bool passed = test.test_func();
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

            results.push_back(TestResult{
                .test_name = test.test_name,
                .passed = passed,
                .error_message = passed ? "" : "Test assertion failed",
                .execution_time_ms = elapsed
            });
        } catch (const std::exception& e) {
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

            results.push_back(TestResult{
                .test_name = test.test_name,
                .passed = false,
                .error_message = e.what(),
                .execution_time_ms = elapsed
            });
        }
    }

    return results;
}

std::vector<TestResult> PluginTestFramework::RunAllTests() {
    std::vector<TestResult> results;
    std::vector<std::string> unique_plugins;

    for (const auto& test : tests_) {
        if (std::find(unique_plugins.begin(), unique_plugins.end(), test.plugin_id) == unique_plugins.end()) {
            unique_plugins.push_back(test.plugin_id);
        }
    }

    for (const auto& plugin_id : unique_plugins) {
        auto plugin_results = RunPluginTests(plugin_id);
        results.insert(results.end(), plugin_results.begin(), plugin_results.end());
    }

    return results;
}

int PluginTestFramework::GetTestCount(const std::string& plugin_id) const {
    int count = 0;
    for (const auto& test : tests_) {
        if (test.plugin_id == plugin_id) {
            count++;
        }
    }
    return count;
}

void PluginTestFramework::Clear() {
    tests_.clear();
}

// PluginValidator implementation
bool PluginValidator::ValidatePlugin(const std::shared_ptr<IPlugin>& plugin) {
    if (!plugin) return false;
    return CheckRequiredMethods(plugin) && VerifyMetadata(plugin->GetMetadata());
}

bool PluginValidator::CheckRequiredMethods(const std::shared_ptr<IPlugin>& plugin) {
    if (!plugin) return false;
    try {
        auto metadata = plugin->GetMetadata();
        auto status = plugin->GetStatus();
        auto version = plugin->GetVersion();
        auto api_version = plugin->GetRequiredAPIVersion();
        bool validate = plugin->Validate();
        return true;
    } catch (...) {
        return false;
    }
}

bool PluginValidator::VerifyMetadata(const PluginMetadata& metadata) {
    return !metadata.id.empty() && !metadata.name.empty() && !metadata.version.empty();
}

bool PluginValidator::TestInitialization(const std::shared_ptr<IPlugin>& plugin) {
    if (!plugin) return false;
    try {
        bool init_result = plugin->Initialize();
        bool shutdown_result = plugin->Shutdown();
        return init_result && shutdown_result;
    } catch (...) {
        return false;
    }
}

bool PluginValidator::TestConfiguration(const std::shared_ptr<IPlugin>& plugin) {
    if (!plugin) return false;
    try {
        std::map<std::string, std::any> config = {{"test_key", std::string("test_value")}};
        bool set_result = plugin->SetConfiguration(config);
        auto get_result = plugin->GetConfiguration();
        return set_result && !get_result.empty();
    } catch (...) {
        return false;
    }
}

// PluginBenchmark implementation
double PluginBenchmark::BenchmarkInitialization(const std::shared_ptr<IPlugin>& plugin, int iterations) {
    if (!plugin) return 0.0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        plugin->Initialize();
        plugin->Shutdown();
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    return elapsed / iterations;
}

double PluginBenchmark::BenchmarkHookExecution(const std::shared_ptr<IPlugin>& plugin, int iterations) {
    if (!plugin) return 0.0;

    std::string hook_id = plugin->RegisterHook("bench_hook", [](const auto&, const auto&) {
        return true;
    });

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        std::map<std::string, std::any> params;
        HookExecutor::ExecuteHooks("bench_hook", params);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    return elapsed / iterations;
}

size_t PluginBenchmark::EstimateMemoryUsage(const std::shared_ptr<IPlugin>& plugin) {
    // Rough estimate - actual implementation would need more sophisticated tracking
    return sizeof(*plugin.get());
}

} // namespace bendcore::plugins
