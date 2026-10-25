#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <map>

namespace bendcore::testing {

/**
 * CAD-CORE-021: ComprehensiveTestSuite
 *
 * Comprehensive integration test suite for Phase F production launch.
 * Tests all phases A-E together with stress tests and crash recovery.
 *
 * Features:
 * - 50+ E2E integration tests
 * - Stress tests (10K+ entities)
 * - Multi-phase workflow validation
 * - Crash recovery testing
 * - Cross-platform compatibility checks
 */

enum class TestCategory {
  EntitySystem,        // Phase A: Document model
  CommandEngine,       // Phase B: Commands & transactions
  ParameterEngine,     // Phase C: Parameters & dependencies
  GeometryEngine,      // Phase D: Geometry & topology
  SerializationEngine, // Phase E: Persistence (if implemented)
  E2EWorkflows,        // Full end-to-end scenarios
  StressTesting,       // Large document handling
  RecoveryTesting,     // Crash/recovery scenarios
  MemorySafety,        // Memory and leak detection
  CrossPlatform        // Platform compatibility
};

struct TestResult {
  std::string test_name;
  TestCategory category;
  bool passed;
  std::chrono::milliseconds duration;
  std::string error_message;
  size_t operations_count;
};

class TestCase {
public:
  explicit TestCase(std::string name, TestCategory category)
    : name_(std::move(name)), category_(category) {}

  virtual ~TestCase() = default;

  virtual TestResult run() = 0;

  std::string name() const { return name_; }
  TestCategory category() const { return category_; }

protected:
  std::string name_;
  TestCategory category_;
};

class ComprehensiveTestSuite {
public:
  ComprehensiveTestSuite();

  // Register test cases
  void register_test(std::shared_ptr<TestCase> test);

  // Run all tests
  std::vector<TestResult> run_all();

  // Run tests by category
  std::vector<TestResult> run_category(TestCategory category);

  // Run specific test
  TestResult run_test(const std::string& test_name);

  // Stress testing
  std::vector<TestResult> run_stress_tests();

  // Integration testing
  std::vector<TestResult> run_integration_tests();

  // Recovery testing
  std::vector<TestResult> run_recovery_tests();

  // Get summary
  struct Summary {
    size_t total_tests;
    size_t passed_tests;
    size_t failed_tests;
    std::chrono::milliseconds total_duration;
    double pass_rate;
    std::map<TestCategory, size_t> tests_by_category;
  };

  Summary get_summary() const;

  // Report generation
  std::string generate_html_report() const;
  std::string generate_json_report() const;
  std::string generate_text_report() const;

private:
  std::vector<std::shared_ptr<TestCase>> tests_;
  std::vector<TestResult> last_results_;

  void add_default_tests();
};

// E2E Workflow Test: Create, Edit, Save, Load, Export
class E2EWorkflowTest : public TestCase {
public:
  E2EWorkflowTest();
  TestResult run() override;
};

// Stress Test: 10K+ entities
class StressTest : public TestCase {
public:
  explicit StressTest(size_t entity_count = 10000);
  TestResult run() override;

private:
  size_t entity_count_;
};

// Multi-Phase Integration Test: A→B→C→D→E
class MultiPhaseIntegrationTest : public TestCase {
public:
  MultiPhaseIntegrationTest();
  TestResult run() override;
};

// Crash Recovery Test
class CrashRecoveryTest : public TestCase {
public:
  CrashRecoveryTest();
  TestResult run() override;
};

// Cross-Platform Compatibility Test
class CrossPlatformTest : public TestCase {
public:
  CrossPlatformTest();
  TestResult run() override;
};

// Batch Operations Test
class BatchOperationsTest : public TestCase {
public:
  explicit BatchOperationsTest(size_t batch_size = 1000);
  TestResult run() override;

private:
  size_t batch_size_;
};

// Memory Leak Detection Test
class MemoryLeakTest : public TestCase {
public:
  MemoryLeakTest();
  TestResult run() override;
};

// Parameter Rebuild Test
class ParameterRebuildTest : public TestCase {
public:
  ParameterRebuildTest();
  TestResult run() override;
};

// Undo/Redo Consistency Test
class UndoRedoConsistencyTest : public TestCase {
public:
  UndoRedoConsistencyTest();
  TestResult run() override;
};

// Event Bus Reliability Test
class EventBusReliabilityTest : public TestCase {
public:
  explicit EventBusReliabilityTest(size_t event_count = 10000);
  TestResult run() override;

private:
  size_t event_count_;
};

// File Format Round-Trip Test
class FileFormatRoundTripTest : public TestCase {
public:
  FileFormatRoundTripTest();
  TestResult run() override;
};

} // namespace bendcore::testing
