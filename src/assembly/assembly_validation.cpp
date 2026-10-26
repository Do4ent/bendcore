#include "bendcore/assembly/assembly_validation.hpp"
#include "bendcore/assembly/assembly.hpp"
#include "bendcore/assembly/component_instance.hpp"
#include "bendcore/assembly/assembly_constraints.hpp"
#include "bendcore/assembly/mate_solver.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <set>
#include <iomanip>

namespace BendCore::CadCore {

// ==================== ValidationIssue ====================

size_t AssemblyValidationReport::getIssueCount(ValidationSeverity severity) const noexcept {
    switch (severity) {
        case ValidationSeverity::ERROR:
            return errors.size();
        case ValidationSeverity::WARNING:
            return warnings.size();
        case ValidationSeverity::INFO:
            return infos.size();
        default:
            return 0;
    }
}

size_t AssemblyValidationReport::getIssueCountByCategory(const std::string& category) const noexcept {
    size_t count = 0;
    for (const auto& issue : issues) {
        if (issue.category == category) {
            count++;
        }
    }
    return count;
}

// ==================== AssemblyValidator ====================

AssemblyValidator::AssemblyValidator() noexcept {
    initializeBuiltInRules();
}

void AssemblyValidator::initializeBuiltInRules() noexcept {
    registerRule(std::make_shared<EmptyComponentRule>());
    registerRule(std::make_shared<ConstraintSatisfactionRule>());
    registerRule(std::make_shared<RedundantConstraintRule>());
    registerRule(std::make_shared<ConflictingConstraintRule>());
    registerRule(std::make_shared<ComplexityRule>());
    registerRule(std::make_shared<IsolatedComponentRule>());
}

AssemblyValidationReport AssemblyValidator::validateAssembly(const Assembly& assembly) noexcept {
    return validateAssembly(assembly, getStandardProfile());
}

AssemblyValidationReport AssemblyValidator::validateAssembly(
    const Assembly& assembly,
    const ValidationProfile& profile
) noexcept {
    AssemblyValidationReport report;
    report.validation_time = std::chrono::steady_clock::now();
    
    if (profile.check_references) {
        auto issues = checkComponentReferences(assembly);
        report.issues.insert(report.issues.end(), issues.begin(), issues.end());
    }
    
    if (profile.check_dependencies) {
        auto issues = checkCircularDependencies(assembly);
        report.issues.insert(report.issues.end(), issues.begin(), issues.end());
    }
    
    auto dup_issues = checkDuplicateInstances(assembly);
    report.issues.insert(report.issues.end(), dup_issues.begin(), dup_issues.end());
    
    if (profile.check_geometry) {
        auto geom_issues = checkComponentGeometry(assembly);
        report.issues.insert(report.issues.end(), geom_issues.begin(), geom_issues.end());
        
        auto bounds_issues = checkAssemblyBounds(assembly);
        report.issues.insert(report.issues.end(), bounds_issues.begin(), bounds_issues.end());
        
        auto coord_issues = checkCoordinateSystems(assembly);
        report.issues.insert(report.issues.end(), coord_issues.begin(), coord_issues.end());
    }
    
    if (profile.check_constraints) {
        auto constraint_issues = checkConstraintSatisfaction(assembly);
        report.issues.insert(report.issues.end(), constraint_issues.begin(), constraint_issues.end());
        
        auto over_issues = checkOverConstrained(assembly);
        report.issues.insert(report.issues.end(), over_issues.begin(), over_issues.end());
        
        auto under_issues = checkUnderConstrained(assembly);
        report.issues.insert(report.issues.end(), under_issues.begin(), under_issues.end());
        
        auto conflict_issues = checkConstraintConflicts(assembly);
        report.issues.insert(report.issues.end(), conflict_issues.begin(), conflict_issues.end());
        
        auto redundant_issues = checkRedundantConstraints(assembly);
        report.issues.insert(report.issues.end(), redundant_issues.begin(), redundant_issues.end());
    }
    
    if (profile.check_performance) {
        auto perf_issues = checkPerformanceMetrics(assembly);
        report.issues.insert(report.issues.end(), perf_issues.begin(), perf_issues.end());
    }
    
    applyCustomRules(assembly, profile, report);
    
    for (const auto& issue : report.issues) {
        switch (issue.severity) {
            case ValidationSeverity::ERROR:
                report.errors.push_back(issue);
                break;
            case ValidationSeverity::WARNING:
                report.warnings.push_back(issue);
                break;
            case ValidationSeverity::INFO:
                report.infos.push_back(issue);
                break;
        }
    }
    
    report.stats.total_components = assembly.getComponentInstanceCount();
    
    if (assembly.hasConstraintSolver()) {
        const auto& solver = assembly.getConstraintSolver();
        const auto& constraints = solver.getConstraints();
        report.stats.total_constraints = constraints.size();
        
        for (const auto& constraint : constraints) {
            if (constraint.isSatisfied()) {
                report.stats.satisfied_constraints++;
            }
        }
    }
    
    report.stats.assembly_complexity = analyzeComplexity(assembly);
    
    auto bounds = calculateAssemblyBounds(assembly);
    report.stats.bounds_volume = (bounds[3] - bounds[0]) * (bounds[4] - bounds[1]) * (bounds[5] - bounds[2]);
    
    report.is_valid = report.errors.empty();
    
    report.summary = generateSummary(report);
    
    return report;
}

std::vector<ValidationIssue> AssemblyValidator::checkComponentReferences(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Null component instance in assembly";
            issue.category = "reference";
            issue.code = 1001;
            issue.recommendation = "Remove the invalid component instance";
            issues.push_back(issue);
            continue;
        }
        
        if (instance->getPartId().isNull()) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Component instance has null part reference";
            issue.location = instance->getId().toString();
            issue.category = "reference";
            issue.code = 1002;
            issue.recommendation = "Set a valid part reference for this component";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkCircularDependencies(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    std::set<EntityId> checked_ids;
    if (assembly.hasCircularReferences(checked_ids)) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::ERROR;
        issue.message = "Assembly contains circular dependency";
        issue.location = assembly.getId().toString();
        issue.category = "dependency";
        issue.code = 2001;
        issue.recommendation = "Remove the circular reference by modifying assembly structure";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkDuplicateInstances(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    std::map<EntityId, int> id_counts;
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (instance) {
            id_counts[instance->getId()]++;
        }
    }
    
    for (const auto& [id, count] : id_counts) {
        if (count > 1) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Duplicate component instance ID found (" + std::to_string(count) + " occurrences)";
            issue.location = id.toString();
            issue.category = "integrity";
            issue.code = 1003;
            issue.recommendation = "Remove or regenerate duplicate instances";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkComponentGeometry(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance) continue;
        
        const auto& pos = instance->getPosition();
        if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z) ||
            std::isinf(pos.x) || std::isinf(pos.y) || std::isinf(pos.z)) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Component has invalid position (NaN or Inf)";
            issue.location = instance->getId().toString();
            issue.category = "geometry";
            issue.code = 3001;
            issue.recommendation = "Reset component position to valid coordinates";
            issues.push_back(issue);
        }
        
        const auto& orient = instance->getOrientation();
        if (std::isnan(orient.x) || std::isnan(orient.y) || std::isnan(orient.z) ||
            std::isinf(orient.x) || std::isinf(orient.y) || std::isinf(orient.z)) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Component has invalid orientation (NaN or Inf)";
            issue.location = instance->getId().toString();
            issue.category = "geometry";
            issue.code = 3002;
            issue.recommendation = "Reset component orientation to valid angles";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkConstraintSatisfaction(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    const auto& constraints = solver.getConstraints();
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::WARNING;
            issue.message = "Constraint is suppressed (may not be applied)";
            issue.location = constraint.getId().toString();
            issue.category = "constraint";
            issue.code = 4001;
            issue.recommendation = "Enable constraint if it should be active";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkOverConstrained(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    size_t component_count = assembly.getComponentInstanceCount();
    
    size_t dof = component_count * 6;
    
    const auto& constraints = solver.getConstraints();
    size_t active_constraints = 0;
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) {
            active_constraints++;
        }
    }
    
    if (active_constraints > dof) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::WARNING;
        issue.message = "Assembly may be over-constrained (" + std::to_string(active_constraints) +
                       " constraints > " + std::to_string(dof) + " DOF)";
        issue.location = assembly.getId().toString();
        issue.category = "constraint";
        issue.code = 4002;
        issue.recommendation = "Review constraints for potential redundancy or conflicts";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkUnderConstrained(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    size_t component_count = assembly.getComponentInstanceCount();
    
    if (component_count == 0) {
        return issues;
    }
    
    std::set<EntityId> constrained_components;
    const auto& constraints = solver.getConstraints();
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) {
            constrained_components.insert(constraint.getEntity1());
            constrained_components.insert(constraint.getEntity2());
        }
    }
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance || instance->isSuppressed()) continue;
        
        if (constrained_components.find(instance->getId()) == constrained_components.end()) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::WARNING;
            issue.message = "Component is not constrained to any other component";
            issue.location = instance->getId().toString();
            issue.category = "constraint";
            issue.code = 4003;
            issue.recommendation = "Add constraints to position this component relative to others";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkConstraintConflicts(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkRedundantConstraints(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    const auto& constraints = solver.getConstraints();
    
    std::map<std::string, int> constraint_signatures;
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) continue;
        
        std::ostringstream ss;
        ss << static_cast<int>(constraint.getType()) << ":"
           << constraint.getEntity1().toString() << ":"
           << constraint.getEntity2().toString();
        
        constraint_signatures[ss.str()]++;
    }
    
    for (const auto& [sig, count] : constraint_signatures) {
        if (count > 1) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::WARNING;
            issue.message = "Potentially redundant constraints (" + std::to_string(count) + " similar constraints)";
            issue.category = "constraint";
            issue.code = 4005;
            issue.recommendation = "Review and remove redundant constraints";
            issues.push_back(issue);
            break;
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkDegenerateGeometry(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    std::map<std::string, std::vector<std::shared_ptr<ComponentInstance>>> position_map;
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance) continue;
        
        const auto& pos = instance->getPosition();
        std::ostringstream ss;
        ss.precision(6);
        ss << pos.x << "," << pos.y << "," << pos.z;
        position_map[ss.str()].push_back(instance);
    }
    
    for (const auto& [pos_str, instances] : position_map) {
        if (instances.size() > 1) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::WARNING;
            issue.message = "Multiple components at the same position (potential issue)";
            issue.location = instances[0]->getId().toString();
            issue.category = "geometry";
            issue.code = 3003;
            issue.recommendation = "Separate components to different positions";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkAssemblyBounds(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    auto bounds = calculateAssemblyBounds(assembly);
    
    double range_x = bounds[3] - bounds[0];
    double range_y = bounds[4] - bounds[1];
    double range_z = bounds[5] - bounds[2];
    
    if (range_x < 1e-6 || range_y < 1e-6 || range_z < 1e-6) {
        if (assembly.getComponentInstanceCount() > 1) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::WARNING;
            issue.message = "Assembly bounds have very small range (potentially degenerate)";
            issue.location = assembly.getId().toString();
            issue.category = "geometry";
            issue.code = 3004;
            issue.recommendation = "Check if components are properly positioned";
            issues.push_back(issue);
        }
    }
    
    double max_range = std::max({range_x, range_y, range_z});
    if (max_range > 1e6) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::WARNING;
        issue.message = "Assembly bounds are extremely large (potential issue)";
        issue.location = assembly.getId().toString();
        issue.category = "geometry";
        issue.code = 3005;
        issue.recommendation = "Verify component positions are correct";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkCoordinateSystems(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance) continue;
        
        auto matrix = instance->getTransformMatrix();
        
        bool has_invalid = false;
        for (int i = 0; i < 16; i++) {
            if (std::isnan(matrix[i]) || std::isinf(matrix[i])) {
                has_invalid = true;
                break;
            }
        }
        
        if (has_invalid) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::ERROR;
            issue.message = "Component has invalid transformation matrix";
            issue.location = instance->getId().toString();
            issue.category = "geometry";
            issue.code = 3006;
            issue.recommendation = "Reset component transformation";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkComponentIntersections(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    const double MIN_DISTANCE = 0.001;
    
    auto instances = assembly.getComponentInstances();
    for (size_t i = 0; i < instances.size(); i++) {
        for (size_t j = i + 1; j < instances.size(); j++) {
            if (!instances[i] || !instances[j]) continue;
            
            const auto& pos1 = instances[i]->getPosition();
            const auto& pos2 = instances[j]->getPosition();
            
            Vector3D diff(pos2.x - pos1.x, pos2.y - pos1.y, pos2.z - pos1.z);
            double distance = diff.magnitude();
            
            if (distance < MIN_DISTANCE && distance > 1e-10) {
                ValidationIssue issue;
                issue.severity = ValidationSeverity::WARNING;
                issue.message = "Components are very close (potential intersection)";
                issue.location = instances[i]->getId().toString();
                issue.category = "geometry";
                issue.code = 3007;
                issue.recommendation = "Move components apart or add gap constraint";
                issues.push_back(issue);
            }
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> AssemblyValidator::checkPerformanceMetrics(
    const Assembly& assembly
) const noexcept {
    std::vector<ValidationIssue> issues;
    
    size_t component_count = assembly.getComponentInstanceCount();
    
    const size_t LARGE_ASSEMBLY = 500;
    const size_t VERY_LARGE_ASSEMBLY = 1000;
    
    if (component_count > VERY_LARGE_ASSEMBLY) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::WARNING;
        issue.message = "Assembly has very large number of components (" +
                       std::to_string(component_count) + ")";
        issue.location = assembly.getId().toString();
        issue.category = "performance";
        issue.code = 5001;
        issue.recommendation = "Consider breaking into sub-assemblies";
        issues.push_back(issue);
    } else if (component_count > LARGE_ASSEMBLY) {
        ValidationIssue issue;
        issue.severity = ValidationSeverity::INFO;
        issue.message = "Assembly is large (" + std::to_string(component_count) + " components)";
        issue.location = assembly.getId().toString();
        issue.category = "performance";
        issue.code = 5002;
        issue.recommendation = "Monitor performance with this assembly size";
        issues.push_back(issue);
    }
    
    if (assembly.hasConstraintSolver()) {
        const auto& solver = assembly.getConstraintSolver();
        size_t constraint_count = solver.getConstraints().size();
        const size_t CONSTRAINT_THRESHOLD = component_count * 10;
        
        if (constraint_count > CONSTRAINT_THRESHOLD) {
            ValidationIssue issue;
            issue.severity = ValidationSeverity::INFO;
            issue.message = "Assembly has high constraint-to-component ratio";
            issue.location = assembly.getId().toString();
            issue.category = "performance";
            issue.code = 5003;
            issue.recommendation = "Review constraints for redundancy";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

double AssemblyValidator::analyzeComplexity(const Assembly& assembly) const noexcept {
    double complexity = 0.0;
    
    size_t component_count = assembly.getComponentInstanceCount();
    double component_factor = std::min(0.4, static_cast<double>(component_count) / 1000.0);
    complexity += component_factor;
    
    size_t constraint_count = 0;
    if (assembly.hasConstraintSolver()) {
        constraint_count = assembly.getConstraintSolver().getConstraints().size();
    }
    double constraint_factor = std::min(0.3, static_cast<double>(constraint_count) / 5000.0);
    complexity += constraint_factor;
    
    if (component_count > 0) {
        double ratio = static_cast<double>(constraint_count) / component_count;
        double ratio_factor = std::min(0.3, ratio / 10.0);
        complexity += ratio_factor;
    }
    
    return std::min(1.0, complexity);
}

std::array<double, 6> AssemblyValidator::calculateAssemblyBounds(
    const Assembly& assembly
) const noexcept {
    std::array<double, 6> bounds = {
        1e9, 1e9, 1e9,
        -1e9, -1e9, -1e9
    };
    
    auto instances = assembly.getComponentInstances();
    if (instances.empty()) {
        return bounds;
    }
    
    for (const auto& instance : instances) {
        if (!instance) continue;
        
        const auto& pos = instance->getPosition();
        const double COMPONENT_SIZE = 5.0;
        
        bounds[0] = std::min(bounds[0], pos.x - COMPONENT_SIZE);
        bounds[1] = std::min(bounds[1], pos.y - COMPONENT_SIZE);
        bounds[2] = std::min(bounds[2], pos.z - COMPONENT_SIZE);
        
        bounds[3] = std::max(bounds[3], pos.x + COMPONENT_SIZE);
        bounds[4] = std::max(bounds[4], pos.y + COMPONENT_SIZE);
        bounds[5] = std::max(bounds[5], pos.z + COMPONENT_SIZE);
    }
    
    return bounds;
}

void AssemblyValidator::registerRule(std::shared_ptr<ValidationRule> rule) noexcept {
    if (!rule) return;
    
    rules_.push_back(rule);
    rules_by_name_[rule->getName()] = rule;
}

bool AssemblyValidator::unregisterRule(const std::string& rule_name) noexcept {
    auto it = rules_by_name_.find(rule_name);
    if (it == rules_by_name_.end()) {
        return false;
    }
    
    auto rule = it->second;
    rules_by_name_.erase(it);
    
    auto vec_it = std::find(rules_.begin(), rules_.end(), rule);
    if (vec_it != rules_.end()) {
        rules_.erase(vec_it);
    }
    
    return true;
}

std::shared_ptr<ValidationRule> AssemblyValidator::getRule(const std::string& rule_name) const noexcept {
    auto it = rules_by_name_.find(rule_name);
    if (it != rules_by_name_.end()) {
        return it->second;
    }
    return nullptr;
}

ValidationProfile AssemblyValidator::createProfile(
    const std::string& name,
    const std::vector<std::string>& rule_names
) const noexcept {
    ValidationProfile profile;
    profile.name = name;
    profile.enabled_rules = rule_names;
    return profile;
}

ValidationProfile AssemblyValidator::getStrictProfile() noexcept {
    ValidationProfile profile;
    profile.name = "Strict";
    profile.description = "All validation checks enabled";
    profile.enabled_rules = {
        "empty_components",
        "constraint_satisfaction",
        "empty_components",
        "redundant_constraints",
        "conflicting_constraints",
        "assembly_complexity",
        "isolated_components"
    };
    profile.check_geometry = true;
    profile.check_constraints = true;
    profile.check_references = true;
    profile.check_performance = true;
    profile.check_dependencies = true;
    return profile;
}

ValidationProfile AssemblyValidator::getStandardProfile() noexcept {
    ValidationProfile profile;
    profile.name = "Standard";
    profile.description = "Common validation checks";
    profile.enabled_rules = {
        "constraint_satisfaction",
        "empty_components",
        "conflicting_constraints",
        "assembly_complexity"
    };
    profile.check_geometry = true;
    profile.check_constraints = true;
    profile.check_references = true;
    profile.check_performance = true;
    profile.check_dependencies = true;
    return profile;
}

ValidationProfile AssemblyValidator::getFastProfile() noexcept {
    ValidationProfile profile;
    profile.name = "Fast";
    profile.description = "Quick validation checks only";
    profile.check_geometry = true;
    profile.check_constraints = true;
    profile.check_references = true;
    profile.check_performance = false;
    profile.check_dependencies = false;
    return profile;
}

ValidationProfile AssemblyValidator::getMinimalProfile() noexcept {
    ValidationProfile profile;
    profile.name = "Minimal";
    profile.description = "Essential checks only";
    profile.check_geometry = false;
    profile.check_constraints = false;
    profile.check_references = true;
    profile.check_performance = false;
    profile.check_dependencies = false;
    return profile;
}

void AssemblyValidator::applyCustomRules(
    const Assembly& assembly,
    const ValidationProfile& profile,
    AssemblyValidationReport& report
) const noexcept {
    for (const auto& rule : rules_) {
        if (!rule) continue;
        
        bool enabled = profile.enabled_rules.empty() ||
                      std::find(profile.enabled_rules.begin(),
                               profile.enabled_rules.end(),
                               rule->getName()) != profile.enabled_rules.end();
        
        if (!enabled) continue;
        
        auto rule_issues = rule->validate(assembly);
        
        for (auto& issue : rule_issues) {
            auto severity_it = profile.severity_overrides.find(rule->getName());
            if (severity_it != profile.severity_overrides.end()) {
                issue.severity = severity_it->second;
            }
            report.issues.push_back(issue);
        }
    }
}

std::string AssemblyValidator::generateSummary(
    const AssemblyValidationReport& report
) const noexcept {
    std::ostringstream ss;
    
    ss << "Assembly Validation Report\n";
    ss << "===========================\n";
    ss << "Status: " << (report.is_valid ? "VALID" : "INVALID") << "\n";
    ss << "Components: " << report.stats.total_components << "\n";
    ss << "Constraints: " << report.stats.total_constraints << "\n";
    ss << "Satisfied: " << report.stats.satisfied_constraints << "\n";
    ss << "Unsatisfied: " << report.stats.unsatisfied_constraints << "\n";
    ss << "Redundant: " << report.stats.redundant_constraints << "\n";
    ss << "Conflicting: " << report.stats.conflicting_constraints << "\n";
    ss << "Complexity: " << std::fixed << std::setprecision(2) << report.stats.assembly_complexity << "\n";
    ss << "\nIssues:\n";
    ss << "Errors: " << report.errors.size() << "\n";
    ss << "Warnings: " << report.warnings.size() << "\n";
    ss << "Infos: " << report.infos.size() << "\n";
    
    return ss.str();
}

// ==================== Built-in Rules ====================

std::vector<ValidationIssue> EmptyComponentRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (assembly.isEmpty()) {
        ValidationIssue issue;
        issue.severity = getDefaultSeverity();
        issue.message = "Assembly is empty (no components)";
        issue.location = assembly.getId().toString();
        issue.category = "structure";
        issue.code = 6001;
        issue.recommendation = "Add components to the assembly";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> ConstraintSatisfactionRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    const auto& constraints = solver.getConstraints();
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) continue;
    }
    
    return issues;
}

std::vector<ValidationIssue> RedundantConstraintRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    const auto& constraints = solver.getConstraints();
    
    std::map<std::string, int> constraint_map;
    for (const auto& constraint : constraints) {
        std::ostringstream ss;
        ss << constraint.getEntity1().toString() << ":"
           << constraint.getEntity2().toString() << ":"
           << static_cast<int>(constraint.getType());
        constraint_map[ss.str()]++;
    }
    
    for (const auto& [sig, count] : constraint_map) {
        if (count > 1) {
            ValidationIssue issue;
            issue.severity = getDefaultSeverity();
            issue.message = "Similar constraints found (" + std::to_string(count) + ")";
            issue.category = "constraint";
            issue.code = 7002;
            issue.recommendation = "Consider removing redundant constraint";
            issues.push_back(issue);
            break;
        }
    }
    
    return issues;
}

std::vector<ValidationIssue> ConflictingConstraintRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        return issues;
    }
    
    return issues;
}

std::vector<ValidationIssue> ComplexityRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    AssemblyValidator validator;
    double complexity = validator.analyzeComplexity(assembly);
    
    if (complexity > 0.7) {
        ValidationIssue issue;
        issue.severity = getDefaultSeverity();
        issue.message = "Assembly complexity is high (" + std::to_string(static_cast<int>(complexity * 100)) + "%)";
        issue.location = assembly.getId().toString();
        issue.category = "performance";
        issue.code = 7004;
        issue.recommendation = "Consider optimizing assembly structure";
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<ValidationIssue> IsolatedComponentRule::validate(const Assembly& assembly) const noexcept {
    std::vector<ValidationIssue> issues;
    
    if (!assembly.hasConstraintSolver()) {
        for (const auto& instance : assembly.getComponentInstances()) {
            if (!instance) continue;
            
            ValidationIssue issue;
            issue.severity = getDefaultSeverity();
            issue.message = "Component is not constrained";
            issue.location = instance->getId().toString();
            issue.category = "constraint";
            issue.code = 7005;
            issue.recommendation = "Add constraints to position this component";
            issues.push_back(issue);
        }
        return issues;
    }
    
    const auto& solver = assembly.getConstraintSolver();
    std::set<EntityId> constrained_components;
    const auto& constraints = solver.getConstraints();
    
    for (const auto& constraint : constraints) {
        if (constraint.isSatisfied()) {
            constrained_components.insert(constraint.getEntity1());
            constrained_components.insert(constraint.getEntity2());
        }
    }
    
    for (const auto& instance : assembly.getComponentInstances()) {
        if (!instance || instance->isSuppressed()) continue;
        
        if (constrained_components.find(instance->getId()) == constrained_components.end()) {
            ValidationIssue issue;
            issue.severity = getDefaultSeverity();
            issue.message = "Component is isolated (not constrained to others)";
            issue.location = instance->getId().toString();
            issue.category = "constraint";
            issue.code = 7005;
            issue.recommendation = "Add constraints to connect this component to the assembly";
            issues.push_back(issue);
        }
    }
    
    return issues;
}

} // namespace BendCore::CadCore
