#pragma once

#include <vector>
#include <string>
#include <functional>
#include <map>

namespace bendcore::testing {

/**
 * CAD-CORE-023: ProductionChecklist
 *
 * Pre-launch production readiness validation for Phase F.
 * Performs security audits, memory leak detection, thread safety validation,
 * documentation completeness checks, and compliance verification.
 *
 * Features:
 * - Security audit (no hardcoded secrets)
 * - Memory leak detection
 * - Thread safety validation
 * - Documentation completeness
 * - Compliance verification
 */

enum class ChecklistCategory {
  Security,
  MemorySafety,
  ThreadSafety,
  Documentation,
  Compliance,
  Performance,
  Compatibility,
  Dependencies
};

struct CheckResult {
  std::string check_name;
  ChecklistCategory category;
  bool passed;
  std::string details;
  std::string remediation;
};

class ProductionChecklist {
public:
  ProductionChecklist();

  // Run all checks
  std::vector<CheckResult> run_all_checks();

  // Run checks by category
  std::vector<CheckResult> run_category(ChecklistCategory category);

  // Register custom check
  void register_check(
    const std::string& name,
    ChecklistCategory category,
    std::function<bool(std::string&)> check_fn
  );

  // Get summary
  struct Summary {
    size_t total_checks;
    size_t passed_checks;
    size_t failed_checks;
    double pass_rate;
    std::map<ChecklistCategory, size_t> checks_by_category;
    std::vector<std::string> critical_failures;
    bool is_production_ready;
  };

  Summary get_summary() const;

  // Report generation
  std::string generate_report() const;
  std::string generate_json_report() const;

private:
  std::vector<CheckResult> last_results_;
  std::map<std::string, std::function<bool(std::string&)>> custom_checks_;

  // Default checks
  void add_default_checks();

  // Check implementations
  bool check_no_hardcoded_secrets(std::string& details);
  bool check_no_memory_leaks(std::string& details);
  bool check_memory_bounds(std::string& details);
  bool check_null_pointer_safety(std::string& details);
  bool check_thread_safety_annotations(std::string& details);
  bool check_race_condition_potential(std::string& details);
  bool check_deadlock_potential(std::string& details);
  bool check_all_public_apis_documented(std::string& details);
  bool check_changelog_complete(std::string& details);
  bool check_license_present(std::string& details);
  bool check_contributing_guide(std::string& details);
  bool check_code_style_compliance(std::string& details);
  bool check_dependency_licenses_compatible(std::string& details);
  bool check_no_deprecated_functions(std::string& details);
  bool check_version_consistency(std::string& details);
};

} // namespace bendcore::testing
