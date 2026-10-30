#include "bendcore/testing/test_suite.hpp"
#include "bendcore/testing/performance_profiler.hpp"
#include "bendcore/testing/production_checklist.hpp"
#include "bendcore/testing/launch_package.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace bendcore::testing;

int main() {
  std::cout << "\n";
  std::cout << "================================================================================\n";
  std::cout << "CAD-CORE Phase F: Final Production Launch\n";
  std::cout << "================================================================================\n\n";

  // Phase 1: Comprehensive Integration Testing (CAD-CORE-021)
  std::cout << "PHASE 1: Comprehensive Test Suite (CAD-CORE-021)\n";
  std::cout << "-----------------------------------------------\n\n";

  ComprehensiveTestSuite test_suite;
  auto test_results = test_suite.run_all();
  auto test_summary = test_suite.get_summary();

  std::cout << "Test Results Summary:\n";
  std::cout << "  Total Tests: " << test_summary.total_tests << "\n";
  std::cout << "  Passed:      " << test_summary.passed_tests << "\n";
  std::cout << "  Failed:      " << test_summary.failed_tests << "\n";
  std::cout << "  Pass Rate:   " << std::fixed << std::setprecision(1)
            << (test_summary.pass_rate * 100.0) << "%\n";
  std::cout << "  Duration:    " << test_summary.total_duration.count() << "ms\n\n";

  if (test_summary.pass_rate < 1.0) {
    std::cout << "WARNING: Some tests failed. Review test output above.\n\n";
  } else {
    std::cout << "✓ All tests PASSED\n\n";
  }

  // Save test report
  {
    std::ofstream report_file("CADCORE_PHASE_F_TEST_REPORT.html");
    report_file << test_suite.generate_html_report();
    report_file.close();
    std::cout << "Test report saved to: CADCORE_PHASE_F_TEST_REPORT.html\n\n";
  }

  // Phase 2: Performance Profiling (CAD-CORE-022)
  std::cout << "PHASE 2: Performance Profiling (CAD-CORE-022)\n";
  std::cout << "---------------------------------------------\n\n";

  PerformanceProfiler profiler;
  profiler.run_benchmark_suite();

  auto perf_report = profiler.generate_report();

  std::cout << "Performance Results:\n";
  std::cout << "  Peak Memory Usage: " << perf_report.all_metrics.size() << " operations tracked\n";
  std::cout << "  Status: " << (perf_report.meets_all_targets ? "PASS" : "NEEDS OPTIMIZATION") << "\n\n";

  size_t passed_validations = 0;
  for (const auto& v : perf_report.validations) {
    if (v.is_valid) {
      passed_validations++;
    } else {
      std::cout << "  ✗ " << v.operation_name << " (measured: "
                << v.measured_time.count() << "µs, target: "
                << v.target_time.count() << "µs)\n";
    }
  }

  std::cout << "  Passed: " << passed_validations << "/" << perf_report.validations.size()
            << " performance targets\n\n";

  // Save performance report
  {
    std::ofstream perf_file("CADCORE_PHASE_F_PERFORMANCE_REPORT.json");
    perf_file << profiler.export_json_report();
    perf_file.close();
    std::cout << "Performance report saved to: CADCORE_PHASE_F_PERFORMANCE_REPORT.json\n\n";
  }

  // Phase 3: Production Checklist (CAD-CORE-023)
  std::cout << "PHASE 3: Production Readiness Checklist (CAD-CORE-023)\n";
  std::cout << "------------------------------------------------------\n\n";

  ProductionChecklist checklist;
  auto checklist_results = checklist.run_all_checks();
  auto checklist_summary = checklist.get_summary();

  std::cout << "Production Checklist Results:\n";
  std::cout << "  Total Checks:     " << checklist_summary.total_checks << "\n";
  std::cout << "  Passed:           " << checklist_summary.passed_checks << "\n";
  std::cout << "  Failed:           " << checklist_summary.failed_checks << "\n";
  std::cout << "  Pass Rate:        " << std::fixed << std::setprecision(1)
            << (checklist_summary.pass_rate * 100.0) << "%\n";
  std::cout << "  Production Ready: " << (checklist_summary.is_production_ready ? "YES" : "NO") << "\n\n";

  if (!checklist_summary.critical_failures.empty()) {
    std::cout << "Critical Failures:\n";
    for (const auto& failure : checklist_summary.critical_failures) {
      std::cout << "  ✗ " << failure << "\n";
    }
    std::cout << "\n";
  }

  // Save checklist report
  {
    std::ofstream checklist_file("CADCORE_PHASE_F_CHECKLIST_REPORT.txt");
    checklist_file << checklist.generate_report();
    checklist_file.close();
    std::cout << "Checklist report saved to: CADCORE_PHASE_F_CHECKLIST_REPORT.txt\n\n";
  }

  // Phase 4: Launch Package (CAD-CORE-024)
  std::cout << "PHASE 4: Launch Package Generation (CAD-CORE-024)\n";
  std::cout << "-------------------------------------------------\n\n";

  LaunchPackage launch_pkg;
  auto pkg_contents = launch_pkg.generate_package();

  std::cout << "Launch Package Contents:\n";
  std::cout << "  Version:   " << pkg_contents.version << "\n";
  std::cout << "  Codename:  " << LaunchPackage::CODENAME << "\n";
  std::cout << "  Build:     " << pkg_contents.build_date << "\n";
  std::cout << "  Complete:  " << (pkg_contents.is_complete ? "YES" : "NO") << "\n";
  std::cout << "  Files:     " << pkg_contents.documentation_files.size() << "\n\n";

  std::cout << "Documentation Files:\n";
  for (const auto& file : pkg_contents.documentation_files) {
    std::cout << "  • " << std::setw(20) << std::left << file.filename
              << " - " << file.title << "\n";
  }
  std::cout << "\n";

  // Write launch package to disk
  if (launch_pkg.write_package_to_disk("./launch_package")) {
    std::cout << "✓ Launch package written to ./launch_package/\n\n";
  } else {
    std::cout << "✗ Failed to write launch package\n\n";
  }

  // Phase 5: Final Summary
  std::cout << "================================================================================\n";
  std::cout << "PHASE F SUMMARY: Production Launch Readiness\n";
  std::cout << "================================================================================\n\n";

  std::cout << "Component Status:\n";
  std::cout << "  CAD-CORE-021 (TestSuite):            "
            << (test_summary.pass_rate >= 1.0 ? "✓ PASS" : "✗ FAIL") << "\n";
  std::cout << "  CAD-CORE-022 (PerformanceProfiler): "
            << (perf_report.meets_all_targets ? "✓ PASS" : "⚠ REVIEW") << "\n";
  std::cout << "  CAD-CORE-023 (ProductionChecklist): "
            << (checklist_summary.is_production_ready ? "✓ READY" : "✗ NEEDS WORK") << "\n";
  std::cout << "  CAD-CORE-024 (LaunchPackage):       ✓ GENERATED\n\n";

  bool all_passed =
    (test_summary.pass_rate >= 1.0) &&
    perf_report.meets_all_targets &&
    checklist_summary.is_production_ready;

  std::cout << "================================================================================\n";
  if (all_passed) {
    std::cout << "✓ PRODUCTION READY FOR v6.0.0 LAUNCH\n";
  } else {
    std::cout << "⚠ REQUIRES FIXES BEFORE PRODUCTION LAUNCH\n";
  }
  std::cout << "================================================================================\n\n";

  std::cout << "Generated Reports:\n";
  std::cout << "  • CADCORE_PHASE_F_TEST_REPORT.html\n";
  std::cout << "  • CADCORE_PHASE_F_PERFORMANCE_REPORT.json\n";
  std::cout << "  • CADCORE_PHASE_F_CHECKLIST_REPORT.txt\n";
  std::cout << "  • launch_package/ (documentation directory)\n\n";

  return all_passed ? 0 : 1;
}
