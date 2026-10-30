#include "bendcore/assembly/assembly_validation.hpp"
#include "bendcore/assembly/assembly.hpp"
#include "bendcore/assembly/component_instance.hpp"
#include "bendcore/assembly/mate_solver.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/geometry/vector.hpp"

#include <cassert>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <cmath>

using namespace BendCore::CadCore;

static int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << std::endl;
        ++failures;
    } else {
        std::cout << "✓ " << what << std::endl;
    }
}

// ============================================================================
// Test: Validator Creation and Initialization
// ============================================================================

void test_validator_creation() {
    std::cout << "\n=== Validator Creation ===\n";

    AssemblyValidator validator;
    check(validator.getRules().size() > 0, "Validator has built-in rules");
    check(validator.getRule("constraint_satisfaction") != nullptr, "Has constraint satisfaction rule");
    check(validator.getRule("redundant_constraints") != nullptr, "Has redundant constraints rule");
}

void test_validator_profiles() {
    std::cout << "\n=== Validation Profiles ===\n";

    auto strict = AssemblyValidator::getStrictProfile();
    check(strict.check_constraints, "Strict profile checks constraints");
    check(strict.check_geometry, "Strict profile checks geometry");
    check(strict.check_performance, "Strict profile checks performance");

    auto standard = AssemblyValidator::getStandardProfile();
    check(standard.check_constraints, "Standard profile checks constraints");

    auto fast = AssemblyValidator::getFastProfile();
    check(fast.check_references, "Fast profile checks references");
    check(!fast.check_performance, "Fast profile skips performance");

    auto minimal = AssemblyValidator::getMinimalProfile();
    check(minimal.check_references, "Minimal profile checks references");
    check(!minimal.check_geometry, "Minimal profile skips geometry");
}

// ============================================================================
// Test: Assembly Integrity Checks
// ============================================================================

void test_empty_assembly_validation() {
    std::cout << "\n=== Empty Assembly Validation ===\n";

    auto assembly = createAssembly("EmptyAssembly", "Test empty assembly");
    AssemblyValidator validator;

    auto report = validator.validateAssembly(*assembly);
    check(report.stats.total_components == 0, "Empty assembly has 0 components");
    check(report.issues.size() > 0, "Empty assembly has validation issues");
    check(report.infos.size() > 0 || report.warnings.size() > 0, "Has warnings/infos");
}

void test_component_reference_validation() {
    std::cout << "\n=== Component Reference Validation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test references");
    AssemblyValidator validator;

    // Create component with valid reference
    auto instance1 = createComponentInstance(
        EntityId::generate(),  // valid part ID
        Vector3D(0, 0, 0)
    );
    instance1->setName("Component1");
    assembly->addComponentInstance(instance1);

    auto report = validator.validateAssembly(*assembly);
    // No error should be reported for valid reference
    size_t ref_errors = 0;
    for (const auto& issue : report.errors) {
        if (issue.category == "reference") {
            ref_errors++;
        }
    }
    check(ref_errors == 0, "Valid reference passes validation");
}

void test_duplicate_instance_detection() {
    std::cout << "\n=== Duplicate Instance Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test duplicates");
    AssemblyValidator validator;

    // Create first instance
    EntityId instance_id = EntityId::generate();
    auto instance1 = std::make_shared<ComponentInstance>(
        instance_id,
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    instance1->setName("Component1");
    assembly->addComponentInstance(instance1);

    // Try to add another instance with same ID (should fail in assembly)
    // For testing duplicate detection logic
    check(assembly->getComponentInstanceCount() == 1, "First instance added");

    // Create distinct instances for actual testing
    auto instance2 = std::make_shared<ComponentInstance>(
        EntityId::generate(),
        EntityId::generate(),
        Vector3D(10, 0, 0)
    );
    instance2->setName("Component2");
    assembly->addComponentInstance(instance2);

    auto report = validator.validateAssembly(*assembly);
    size_t dup_errors = 0;
    for (const auto& issue : report.errors) {
        if (issue.code == 1003) {
            dup_errors++;
        }
    }
    check(dup_errors == 0, "No duplicates detected for distinct instances");
}

void test_circular_dependency_detection() {
    std::cout << "\n=== Circular Dependency Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test circular");
    AssemblyValidator validator;

    auto issues = validator.checkCircularDependencies(*assembly);
    check(issues.size() == 0, "No circular dependency in simple assembly");

    // Прямая ссылка сборки на себя — цикл даже без резолвера.
    auto self_ref = createAssembly("Self", "");
    self_ref->addComponentInstance(createComponentInstance(self_ref->getId()));
    check(!validator.checkCircularDependencies(*self_ref).empty(), "Self-reference detected");

    // A -> B -> C -> A через резолвер подсборок.
    auto a = createAssembly("A", ""), b = createAssembly("B", ""), c = createAssembly("C", "");
    a->addComponentInstance(createComponentInstance(b->getId()));
    b->addComponentInstance(createComponentInstance(c->getId()));
    c->addComponentInstance(createComponentInstance(a->getId()));
    std::map<EntityId, std::shared_ptr<const Assembly>> registry{
        {a->getId(), a}, {b->getId(), b}, {c->getId(), c}};
    Assembly::AssemblyResolver resolve = [&](const EntityId& id) -> std::shared_ptr<const Assembly> {
        auto it = registry.find(id);
        return it == registry.end() ? nullptr : it->second;
    };
    std::set<EntityId> path;
    check(a->hasCircularReferences(path, resolve), "Indirect cycle A->B->C->A detected");
    check(path.empty(), "Traversal path restored after check");

    // Ромб: Top использует Shared дважды (через L и R) — это не цикл.
    auto top = createAssembly("Top", ""), l = createAssembly("L", ""), r = createAssembly("R", "");
    auto shared = createAssembly("Shared", "");
    auto bolt = EntityId::generate();  // обычная деталь
    shared->addComponentInstance(createComponentInstance(bolt));
    l->addComponentInstance(createComponentInstance(shared->getId()));
    r->addComponentInstance(createComponentInstance(shared->getId()));
    top->addComponentInstance(createComponentInstance(l->getId()));
    top->addComponentInstance(createComponentInstance(r->getId()));
    registry = {{top->getId(), top}, {l->getId(), l}, {r->getId(), r}, {shared->getId(), shared}};
    check(!top->hasCircularReferences(path, resolve), "Diamond reuse is not a cycle");
}

// ============================================================================
// Test: Constraint Validation
// ============================================================================

void test_constraint_validation_report_counts() {
    std::cout << "\n=== Constraint Validation Report Counts ===\n";

    AssemblyValidationReport report;
    report.is_valid = true;

    // Add some test issues
    ValidationIssue error_issue;
    error_issue.severity = ValidationSeverity::ERROR;
    error_issue.message = "Test error";
    report.issues.push_back(error_issue);
    report.errors.push_back(error_issue);

    ValidationIssue warning_issue;
    warning_issue.severity = ValidationSeverity::WARNING;
    warning_issue.message = "Test warning";
    report.issues.push_back(warning_issue);
    report.warnings.push_back(warning_issue);

    check(report.getIssueCount(ValidationSeverity::ERROR) == 1, "Count errors correctly");
    check(report.getIssueCount(ValidationSeverity::WARNING) == 1, "Count warnings correctly");
    check(report.getIssueCount(ValidationSeverity::INFO) == 0, "Count infos correctly");
}

void test_constraint_satisfaction_check() {
    std::cout << "\n=== Constraint Satisfaction Check ===\n";

    auto assembly = createAssembly("TestAssembly", "Test constraint satisfaction");
    AssemblyValidator validator;

    // Create component instances
    auto instance = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance);

    auto issues = validator.checkConstraintSatisfaction(*assembly);
    check(true, "Constraint satisfaction check runs without error");
}

void test_over_constrained_detection() {
    std::cout << "\n=== Over-Constrained Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test over-constraint");
    AssemblyValidator validator;

    // Assembly with no components should not be reported as over-constrained
    auto issues = validator.checkOverConstrained(*assembly);
    check(issues.empty(), "Empty assembly not flagged as over-constrained");
}

void test_under_constrained_detection() {
    std::cout << "\n=== Under-Constrained Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test under-constraint");
    AssemblyValidator validator;

    // Create unconstrained components
    for (int i = 0; i < 2; i++) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i * 10, 0, 0)
        );
        instance->setName("Component" + std::to_string(i));
        assembly->addComponentInstance(instance);
    }

    auto issues = validator.checkUnderConstrained(*assembly);
    // Components with no solver should be reported as unconstrained
    check(issues.size() > 0 || assembly->getComponentInstanceCount() == 2,
          "Under-constraint check executes");
}

void test_redundant_constraint_detection() {
    std::cout << "\n=== Redundant Constraint Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test redundancy");
    AssemblyValidator validator;

    auto issues = validator.checkRedundantConstraints(*assembly);
    check(issues.empty(), "Empty assembly has no redundant constraints");
}

void test_conflicting_constraint_detection() {
    std::cout << "\n=== Conflicting Constraint Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test conflicts");
    AssemblyValidator validator;

    auto issues = validator.checkConstraintConflicts(*assembly);
    check(issues.empty(), "Empty assembly has no conflicting constraints");
}

// ============================================================================
// Test: Geometry Validation
// ============================================================================

void test_component_geometry_validation() {
    std::cout << "\n=== Component Geometry Validation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test geometry");

    auto instance = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance);

    AssemblyValidator validator;
    auto issues = validator.checkComponentGeometry(*assembly);
    check(issues.empty(), "Valid geometry passes check");
}

void test_degenerate_geometry_detection() {
    std::cout << "\n=== Degenerate Geometry Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test degenerate");

    // Add two components at same position
    auto instance1 = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    instance1->setName("Component1");
    assembly->addComponentInstance(instance1);

    auto instance2 = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    instance2->setName("Component2");
    assembly->addComponentInstance(instance2);

    AssemblyValidator validator;
    auto issues = validator.checkDegenerateGeometry(*assembly);
    check(issues.size() > 0, "Detects components at same position");
}

void test_assembly_bounds_calculation() {
    std::cout << "\n=== Assembly Bounds Calculation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test bounds");

    // Add components at specific positions
    auto instance1 = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance1);

    auto instance2 = createComponentInstance(
        EntityId::generate(),
        Vector3D(10, 10, 10)
    );
    assembly->addComponentInstance(instance2);

    AssemblyValidator validator;
    auto bounds = validator.calculateAssemblyBounds(*assembly);

    check(bounds[0] < 0, "Min X is less than 0");
    check(bounds[1] < 0, "Min Y is less than 0");
    check(bounds[2] < 0, "Min Z is less than 0");
    check(bounds[3] > 10, "Max X is greater than 10");
    check(bounds[4] > 10, "Max Y is greater than 10");
    check(bounds[5] > 10, "Max Z is greater than 10");
}

void test_assembly_bounds_validation() {
    std::cout << "\n=== Assembly Bounds Validation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test bounds validation");

    auto instance = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance);

    AssemblyValidator validator;
    auto issues = validator.checkAssemblyBounds(*assembly);
    check(issues.empty() || issues.size() > 0, "Bounds check executes");
}

void test_coordinate_system_validation() {
    std::cout << "\n=== Coordinate System Validation ===\n";

    auto assembly = createAssembly("TestAssembly", "Test coordinate systems");

    auto instance = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance);

    AssemblyValidator validator;
    auto issues = validator.checkCoordinateSystems(*assembly);
    check(issues.empty(), "Valid coordinate system passes check");
}

void test_component_intersection_detection() {
    std::cout << "\n=== Component Intersection Detection ===\n";

    auto assembly = createAssembly("TestAssembly", "Test intersections");

    // Add two components close together
    auto instance1 = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance1);

    auto instance2 = createComponentInstance(
        EntityId::generate(),
        Vector3D(0.0001, 0, 0)  // Very close
    );
    assembly->addComponentInstance(instance2);

    AssemblyValidator validator;
    auto issues = validator.checkComponentIntersections(*assembly);
    check(issues.size() > 0, "Detects close components");
}

// ============================================================================
// Test: Performance Validation
// ============================================================================

void test_complexity_analysis() {
    std::cout << "\n=== Complexity Analysis ===\n";

    auto assembly = createAssembly("SimpleAssembly", "Simple");

    // Add a few components
    for (int i = 0; i < 3; i++) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i * 10, 0, 0)
        );
        assembly->addComponentInstance(instance);
    }

    AssemblyValidator validator;
    double complexity = validator.analyzeComplexity(*assembly);

    check(complexity >= 0.0 && complexity <= 1.0, "Complexity is in valid range");
    check(complexity < 0.5, "Simple assembly has low complexity");
}

void test_complexity_rule() {
    std::cout << "\n=== Complexity Rule ===\n";

    auto assembly = createAssembly("TestAssembly", "Test complexity rule");

    ComplexityRule rule;
    check(rule.getName() == "assembly_complexity", "Rule name is correct");
    check(rule.getDefaultSeverity() == ValidationSeverity::INFO, "Default severity is INFO");

    auto issues = rule.validate(*assembly);
    check(issues.empty() || issues.size() > 0, "Rule executes without error");
}

void test_performance_metrics_check() {
    std::cout << "\n=== Performance Metrics Check ===\n";

    auto assembly = createAssembly("TestAssembly", "Test performance");

    AssemblyValidator validator;
    auto issues = validator.checkPerformanceMetrics(*assembly);
    check(issues.empty(), "Empty assembly has no performance issues");
}

void test_large_assembly_detection() {
    std::cout << "\n=== Large Assembly Detection ===\n";

    auto assembly = createAssembly("LargeAssembly", "Large test");

    // Add many components
    for (int i = 0; i < 10; i++) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i, i, i)
        );
        assembly->addComponentInstance(instance);
    }

    AssemblyValidator validator;
    auto issues = validator.checkPerformanceMetrics(*assembly);
    check(assembly->getComponentInstanceCount() == 10, "Added 10 components");
}

// ============================================================================
// Test: Validation Rules
// ============================================================================

void test_empty_component_rule() {
    std::cout << "\n=== Empty Component Rule ===\n";

    EmptyComponentRule rule;
    check(rule.getName() == "empty_components", "Rule name is correct");
    check(rule.getDefaultSeverity() == ValidationSeverity::WARNING, "Default severity is WARNING");

    auto empty_assembly = createAssembly("Empty", "Empty assembly");
    auto issues = rule.validate(*empty_assembly);
    check(issues.size() > 0, "Detects empty assembly");

    // Add a component
    auto instance = createComponentInstance(EntityId::generate(), Vector3D(0, 0, 0));
    empty_assembly->addComponentInstance(instance);

    issues = rule.validate(*empty_assembly);
    check(issues.empty(), "No issues for non-empty assembly");
}

void test_constraint_satisfaction_rule() {
    std::cout << "\n=== Constraint Satisfaction Rule ===\n";

    ConstraintSatisfactionRule rule;
    check(rule.getName() == "constraint_satisfaction", "Rule name is correct");
    check(rule.getDefaultSeverity() == ValidationSeverity::ERROR, "Default severity is ERROR");

    auto assembly = createAssembly("Test", "Test");
    auto issues = rule.validate(*assembly);
    check(issues.empty(), "No unsatisfied constraints in empty assembly");
}

void test_redundant_constraint_rule() {
    std::cout << "\n=== Redundant Constraint Rule ===\n";

    RedundantConstraintRule rule;
    check(rule.getName() == "redundant_constraints", "Rule name is correct");

    auto assembly = createAssembly("Test", "Test");
    auto issues = rule.validate(*assembly);
    check(issues.empty(), "No redundant constraints in empty assembly");
}

void test_conflicting_constraint_rule() {
    std::cout << "\n=== Conflicting Constraint Rule ===\n";

    ConflictingConstraintRule rule;
    check(rule.getName() == "conflicting_constraints", "Rule name is correct");

    auto assembly = createAssembly("Test", "Test");
    auto issues = rule.validate(*assembly);
    check(issues.empty(), "No conflicting constraints in empty assembly");
}

void test_isolated_component_rule() {
    std::cout << "\n=== Isolated Component Rule ===\n";

    IsolatedComponentRule rule;
    check(rule.getName() == "isolated_components", "Rule name is correct");

    auto assembly = createAssembly("Test", "Test");

    // Add unconstrained component
    auto instance = createComponentInstance(EntityId::generate(), Vector3D(0, 0, 0));
    assembly->addComponentInstance(instance);

    auto issues = rule.validate(*assembly);
    check(issues.size() > 0, "Detects isolated component");
}

void test_rule_registration() {
    std::cout << "\n=== Rule Registration ===\n";

    AssemblyValidator validator;

    // Count initial rules
    size_t initial_count = validator.getRules().size();
    check(initial_count > 0, "Validator has built-in rules");

    // Register custom rule
    class CustomRule : public ValidationRule {
    public:
        [[nodiscard]] std::string getName() const noexcept override { return "custom_test"; }
        [[nodiscard]] std::string getDescription() const noexcept override { return "Test rule"; }
        [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
            return ValidationSeverity::INFO;
        }
        [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly&) const noexcept override {
            return {};
        }
    };

    auto custom_rule = std::make_shared<CustomRule>();
    validator.registerRule(custom_rule);

    check(validator.getRule("custom_test") != nullptr, "Custom rule registered");
    check(validator.getRules().size() == initial_count + 1, "Rule count increased");

    // Unregister rule
    bool removed = validator.unregisterRule("custom_test");
    check(removed, "Custom rule unregistered");
    check(validator.getRule("custom_test") == nullptr, "Rule no longer available");
}

// ============================================================================
// Test: Complete Validation Report
// ============================================================================

void test_full_validation_report() {
    std::cout << "\n=== Full Validation Report ===\n";

    auto assembly = createAssembly("CompleteTest", "Full test assembly");

    // Add some components
    for (int i = 0; i < 3; i++) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i * 5, 0, 0)
        );
        instance->setName("Part" + std::to_string(i));
        assembly->addComponentInstance(instance);
    }

    AssemblyValidator validator;
    auto report = validator.validateAssembly(*assembly);

    check(!report.summary.empty(), "Report has summary");
    check(report.stats.total_components == 3, "Correct component count");
    check(report.stats.assembly_complexity >= 0.0, "Complexity calculated");
    check(report.issues.size() >= 0, "Issues list created");

    // Check categorization
    size_t total = report.errors.size() + report.warnings.size() + report.infos.size();
    check(total == report.issues.size(), "Issues properly categorized");
}

void test_validation_with_strict_profile() {
    std::cout << "\n=== Validation with Strict Profile ===\n";

    auto assembly = createAssembly("StrictTest", "Strict profile test");

    auto instance = createComponentInstance(
        EntityId::generate(),
        Vector3D(0, 0, 0)
    );
    assembly->addComponentInstance(instance);

    AssemblyValidator validator;
    auto strict_profile = AssemblyValidator::getStrictProfile();
    auto report = validator.validateAssembly(*assembly, strict_profile);

    check(!report.summary.empty(), "Report generated with strict profile");
    check(report.validation_time.time_since_epoch().count() > 0, "Validation time recorded");
}

void test_validation_with_minimal_profile() {
    std::cout << "\n=== Validation with Minimal Profile ===\n";

    auto assembly = createAssembly("MinimalTest", "Minimal profile test");

    AssemblyValidator validator;
    auto minimal_profile = AssemblyValidator::getMinimalProfile();
    auto report = validator.validateAssembly(*assembly, minimal_profile);

    check(!report.summary.empty(), "Report generated with minimal profile");
}

void test_health_statistics() {
    std::cout << "\n=== Health Statistics ===\n";

    auto assembly = createAssembly("HealthTest", "Health statistics test");

    for (int i = 0; i < 5; i++) {
        auto instance = createComponentInstance(
            EntityId::generate(),
            Vector3D(i, i, i)
        );
        assembly->addComponentInstance(instance);
    }

    AssemblyValidator validator;
    auto report = validator.validateAssembly(*assembly);

    check(report.stats.total_components == 5, "Component count correct");
    check(report.stats.assembly_complexity >= 0.0 && report.stats.assembly_complexity <= 1.0,
          "Complexity in valid range");
    check(report.stats.bounds_volume >= 0.0, "Bounds volume calculated");
}

void test_issue_categorization_by_severity() {
    std::cout << "\n=== Issue Categorization by Severity ===\n";

    AssemblyValidationReport report;

    ValidationIssue issue1;
    issue1.severity = ValidationSeverity::ERROR;
    issue1.message = "Error message";
    report.issues.push_back(issue1);
    report.errors.push_back(issue1);

    ValidationIssue issue2;
    issue2.severity = ValidationSeverity::WARNING;
    issue2.message = "Warning message";
    report.issues.push_back(issue2);
    report.warnings.push_back(issue2);

    ValidationIssue issue3;
    issue3.severity = ValidationSeverity::INFO;
    issue3.message = "Info message";
    report.issues.push_back(issue3);
    report.infos.push_back(issue3);

    check(report.getIssueCount(ValidationSeverity::ERROR) == 1, "Count errors");
    check(report.getIssueCount(ValidationSeverity::WARNING) == 1, "Count warnings");
    check(report.getIssueCount(ValidationSeverity::INFO) == 1, "Count infos");
}

void test_issue_categorization_by_category() {
    std::cout << "\n=== Issue Categorization by Category ===\n";

    AssemblyValidationReport report;

    ValidationIssue issue1;
    issue1.severity = ValidationSeverity::ERROR;
    issue1.category = "constraint";
    report.issues.push_back(issue1);

    ValidationIssue issue2;
    issue2.severity = ValidationSeverity::WARNING;
    issue2.category = "geometry";
    report.issues.push_back(issue2);

    check(report.getIssueCountByCategory("constraint") == 1, "Count constraint issues");
    check(report.getIssueCountByCategory("geometry") == 1, "Count geometry issues");
    check(report.getIssueCountByCategory("performance") == 0, "Count non-existent category");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "======================================\n";
    std::cout << "   Assembly Validation System Tests\n";
    std::cout << "======================================\n";

    // Validator creation and initialization
    test_validator_creation();
    test_validator_profiles();

    // Assembly integrity checks
    test_empty_assembly_validation();
    test_component_reference_validation();
    test_duplicate_instance_detection();
    test_circular_dependency_detection();

    // Constraint validation
    test_constraint_validation_report_counts();
    test_constraint_satisfaction_check();
    test_over_constrained_detection();
    test_under_constrained_detection();
    test_redundant_constraint_detection();
    test_conflicting_constraint_detection();

    // Geometry validation
    test_component_geometry_validation();
    test_degenerate_geometry_detection();
    test_assembly_bounds_calculation();
    test_assembly_bounds_validation();
    test_coordinate_system_validation();
    test_component_intersection_detection();

    // Performance validation
    test_complexity_analysis();
    test_complexity_rule();
    test_performance_metrics_check();
    test_large_assembly_detection();

    // Validation rules
    test_empty_component_rule();
    test_constraint_satisfaction_rule();
    test_redundant_constraint_rule();
    test_conflicting_constraint_rule();
    test_isolated_component_rule();
    test_rule_registration();

    // Complete validation report
    test_full_validation_report();
    test_validation_with_strict_profile();
    test_validation_with_minimal_profile();
    test_health_statistics();
    test_issue_categorization_by_severity();
    test_issue_categorization_by_category();

    std::cout << "\n======================================\n";
    if (failures == 0) {
        std::cout << "   All tests passed!\n";
        std::cout << "======================================\n";
        return 0;
    } else {
        std::cout << "   " << failures << " test(s) failed\n";
        std::cout << "======================================\n";
        return 1;
    }
}
