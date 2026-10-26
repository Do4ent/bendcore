#include "../include/bendcore/manufacturing_constraints.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::manufacturing {

// ============================================================================
// TubeBendingConstraint Implementation
// ============================================================================

bool TubeBendingConstraint::validate() const {
    if (min_bend_radius_mm < 0 || max_bend_radius_mm < 0) {
        throw std::invalid_argument("TubeBendingConstraint: invalid bend radius values");
    }
    if (min_bend_radius_mm > max_bend_radius_mm) {
        throw std::invalid_argument("TubeBendingConstraint: min_bend_radius > max_bend_radius");
    }
    if (min_wall_thickness_mm < 0 || max_wall_thickness_mm < 0) {
        throw std::invalid_argument("TubeBendingConstraint: invalid wall thickness values");
    }
    if (min_diameter_mm < 0 || max_diameter_mm < 0) {
        throw std::invalid_argument("TubeBendingConstraint: invalid diameter values");
    }
    if (diameter_to_thickness_ratio < 0) {
        throw std::invalid_argument("TubeBendingConstraint: invalid diameter_to_thickness_ratio");
    }
    if (spring_back_factor < 0.0 || spring_back_factor > 1.0) {
        throw std::invalid_argument("TubeBendingConstraint: spring_back_factor must be 0.0-1.0");
    }
    if (min_bend_angle_deg <= 0 || max_bend_angle_deg >= 180) {
        throw std::invalid_argument("TubeBendingConstraint: invalid bend angle range");
    }
    return true;
}

std::string TubeBendingConstraint::getDescription() const {
    std::ostringstream oss;
    oss << "Tube Bending (" << material_name << "): "
        << "R=" << min_bend_radius_mm << "-" << max_bend_radius_mm << "mm, "
        << "D=" << min_diameter_mm << "-" << max_diameter_mm << "mm, "
        << "t=" << min_wall_thickness_mm << "-" << max_wall_thickness_mm << "mm, "
        << "angle=" << min_bend_angle_deg << "-" << max_bend_angle_deg << "°";
    return oss.str();
}

// ============================================================================
// WeldingConstraint Implementation
// ============================================================================

bool WeldingConstraint::validate() const {
    if (min_joint_gap_mm < 0 || max_joint_gap_mm < 0) {
        throw std::invalid_argument("WeldingConstraint: invalid joint gap values");
    }
    if (min_joint_gap_mm > max_joint_gap_mm) {
        throw std::invalid_argument("WeldingConstraint: min_joint_gap > max_joint_gap");
    }
    if (min_weld_size_mm < 0 || max_weld_size_mm < 0) {
        throw std::invalid_argument("WeldingConstraint: invalid weld size values");
    }
    if (min_weld_size_mm > max_weld_size_mm) {
        throw std::invalid_argument("WeldingConstraint: min_weld_size > max_weld_size");
    }
    if (min_weld_length_mm < 0) {
        throw std::invalid_argument("WeldingConstraint: invalid min_weld_length");
    }
    if (preheat_temperature_c < 0) {
        throw std::invalid_argument("WeldingConstraint: invalid preheat_temperature_c");
    }
    if (thermal_distortion_limit_mm < 0) {
        throw std::invalid_argument("WeldingConstraint: invalid thermal_distortion_limit");
    }
    return true;
}

std::string WeldingConstraint::getDescription() const {
    std::ostringstream oss;
    oss << "Welding: gap=" << min_joint_gap_mm << "-" << max_joint_gap_mm << "mm, "
        << "bead=" << min_weld_size_mm << "-" << max_weld_size_mm << "mm, "
        << "distortion_limit=" << thermal_distortion_limit_mm << "mm";
    if (preheat_temperature_c > 0) {
        oss << ", preheat=" << preheat_temperature_c << "°C";
    }
    return oss.str();
}

// ============================================================================
// MachiningConstraint Implementation
// ============================================================================

bool MachiningConstraint::validate() const {
    if (tool_diameter_mm <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid tool_diameter");
    }
    if (min_tool_clearance_mm < 0) {
        throw std::invalid_argument("MachiningConstraint: invalid min_tool_clearance");
    }
    if (max_tool_engagement_mm <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid max_tool_engagement");
    }
    if (surface_finish_ra_um <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid surface_finish");
    }
    if (tolerance_grade_um <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid tolerance_grade");
    }
    if (min_feature_size_mm <= 0 || max_feature_size_mm <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid feature sizes");
    }
    if (spindle_speed_rpm <= 0 || feed_rate_mm_per_min <= 0) {
        throw std::invalid_argument("MachiningConstraint: invalid speed/feed values");
    }
    return true;
}

std::string MachiningConstraint::getDescription() const {
    std::ostringstream oss;
    oss << "Machining: tool=" << tool_diameter_mm << "mm, "
        << "finish=" << surface_finish_ra_um << "µm Ra, "
        << "tolerance=" << tolerance_grade_um << "µm, "
        << "feature=" << min_feature_size_mm << "-" << max_feature_size_mm << "mm, "
        << "rpm=" << spindle_speed_rpm << ", feed=" << feed_rate_mm_per_min << "mm/min";
    return oss.str();
}

// ============================================================================
// AssemblyConstraint Implementation
// ============================================================================

bool AssemblyConstraint::validate() const {
    if (interference_tolerance_mm < 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid interference_tolerance");
    }
    if (clearance_min_mm < 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid clearance_min");
    }
    if (fastener_access_diameter_mm <= 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid fastener_access_diameter");
    }
    if (maintenance_clearance_mm < 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid maintenance_clearance");
    }
    if (assembly_steps_minimum <= 0 || assembly_steps_maximum <= 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid assembly_steps");
    }
    if (assembly_steps_minimum > assembly_steps_maximum) {
        throw std::invalid_argument("AssemblyConstraint: min_steps > max_steps");
    }
    if (estimated_assembly_time_hours < 0) {
        throw std::invalid_argument("AssemblyConstraint: invalid estimated_assembly_time");
    }
    return true;
}

std::string AssemblyConstraint::getDescription() const {
    std::ostringstream oss;
    oss << "Assembly: clearance=" << clearance_min_mm << "mm, "
        << "interference_tol=" << interference_tolerance_mm << "mm, "
        << "steps=" << assembly_steps_minimum << "-" << assembly_steps_maximum << ", "
        << "time=" << estimated_assembly_time_hours << "h";
    return oss.str();
}

// ============================================================================
// MaterialConstraint Implementation
// ============================================================================

bool MaterialConstraint::validate() const {
    if (density_g_per_cc <= 0) {
        throw std::invalid_argument("MaterialConstraint: invalid density");
    }
    if (yield_strength_mpa <= 0 || tensile_strength_mpa <= 0) {
        throw std::invalid_argument("MaterialConstraint: invalid strength values");
    }
    if (yield_strength_mpa > tensile_strength_mpa) {
        throw std::invalid_argument("MaterialConstraint: yield > tensile strength");
    }
    if (elongation_percent < 0 || elongation_percent > 1000) {
        throw std::invalid_argument("MaterialConstraint: invalid elongation");
    }
    if (material_cost_per_kg < 0 || max_material_cost_usd < 0) {
        throw std::invalid_argument("MaterialConstraint: invalid cost values");
    }
    if (lead_time_days < 0) {
        throw std::invalid_argument("MaterialConstraint: invalid lead_time");
    }
    if (thermal_conductivity_w_per_mk <= 0) {
        throw std::invalid_argument("MaterialConstraint: invalid thermal_conductivity");
    }
    if (max_operating_temperature_c < 0) {
        throw std::invalid_argument("MaterialConstraint: invalid max_operating_temperature");
    }
    return true;
}

std::string MaterialConstraint::getDescription() const {
    std::ostringstream oss;
    oss << "Material: σ_y=" << yield_strength_mpa << "MPa, "
        << "σ_u=" << tensile_strength_mpa << "MPa, "
        << "ε=" << elongation_percent << "%, "
        << "ρ=" << density_g_per_cc << "g/cc, "
        << "cost=" << material_cost_per_kg << "$/kg, "
        << "T_max=" << max_operating_temperature_c << "°C";
    return oss.str();
}

// ============================================================================
// ConstraintViolation Implementation
// ============================================================================

std::string ConstraintViolation::getSeverityString() const {
    switch (severity) {
        case ConstraintSeverity::INFO:
            return "INFO";
        case ConstraintSeverity::WARNING:
            return "WARNING";
        case ConstraintSeverity::ERROR:
            return "ERROR";
        case ConstraintSeverity::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

std::string ConstraintViolation::toString() const {
    std::ostringstream oss;
    oss << "[" << getSeverityString() << "] " << constraint_name
        << " (Entity: " << entity_id << "): " << description << " "
        << "(Violation: " << violation_magnitude << " " << violation_unit << ") "
        << "Recommendation: " << recommendation;
    return oss.str();
}

// ============================================================================
// ManufacturingConstraintsManager Implementation
// ============================================================================

void ManufacturingConstraintsManager::addTubeBendingConstraint(
    const std::string& entity_id,
    const TubeBendingConstraint& constraint) {
    constraint.validate();
    tube_bending_constraints_[entity_id] = constraint;
}

std::optional<TubeBendingConstraint> ManufacturingConstraintsManager::getTubeBendingConstraint(
    const std::string& entity_id) const {
    auto it = tube_bending_constraints_.find(entity_id);
    if (it != tube_bending_constraints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ManufacturingConstraintsManager::validateTubeBending(
    const std::string& entity_id,
    double actual_bend_radius_mm,
    double actual_wall_thickness_mm,
    double actual_diameter_mm,
    double actual_bend_angle_deg) {
    
    auto constraint = getTubeBendingConstraint(entity_id);
    if (!constraint) {
        return true;  // No constraint defined
    }
    
    bool valid = true;
    const auto& c = constraint.value();
    
    // Check bend radius
    if (actual_bend_radius_mm < c.min_bend_radius_mm) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Bend radius too small",
            c.min_bend_radius_mm - actual_bend_radius_mm,
            "mm"
        );
    }
    if (actual_bend_radius_mm > c.max_bend_radius_mm) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Bend radius too large",
            actual_bend_radius_mm - c.max_bend_radius_mm,
            "mm"
        );
    }
    
    // Check wall thickness
    if (actual_wall_thickness_mm < c.min_wall_thickness_mm) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Wall thickness too thin",
            c.min_wall_thickness_mm - actual_wall_thickness_mm,
            "mm"
        );
    }
    
    // Check diameter
    if (actual_diameter_mm < c.min_diameter_mm || actual_diameter_mm > c.max_diameter_mm) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Diameter out of range",
            std::abs(actual_diameter_mm - c.min_diameter_mm),
            "mm"
        );
    }
    
    // Check diameter-to-thickness ratio
    double actual_ratio = actual_diameter_mm / (actual_wall_thickness_mm > 0 ? actual_wall_thickness_mm : 1);
    if (actual_ratio > c.diameter_to_thickness_ratio) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Diameter-to-thickness ratio exceeded",
            actual_ratio - c.diameter_to_thickness_ratio,
            "ratio"
        );
    }
    
    // Check bend angle
    if (actual_bend_angle_deg < c.min_bend_angle_deg || actual_bend_angle_deg > c.max_bend_angle_deg) {
        valid = false;
        recordTubeBendingViolation(
            entity_id, c,
            "Bend angle out of range",
            std::abs(actual_bend_angle_deg - c.min_bend_angle_deg),
            "degrees"
        );
    }
    
    return valid;
}

void ManufacturingConstraintsManager::addWeldingConstraint(
    const std::string& joint_id,
    const WeldingConstraint& constraint) {
    constraint.validate();
    welding_constraints_[joint_id] = constraint;
}

std::optional<WeldingConstraint> ManufacturingConstraintsManager::getWeldingConstraint(
    const std::string& joint_id) const {
    auto it = welding_constraints_.find(joint_id);
    if (it != welding_constraints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ManufacturingConstraintsManager::validateWelding(
    const std::string& joint_id,
    double actual_gap_mm,
    double actual_weld_size_mm,
    bool has_root_pass,
    const std::string& surface_prep) {
    
    auto constraint = getWeldingConstraint(joint_id);
    if (!constraint) {
        return true;
    }
    
    bool valid = true;
    const auto& c = constraint.value();
    
    // Check joint gap
    if (actual_gap_mm < c.min_joint_gap_mm || actual_gap_mm > c.max_joint_gap_mm) {
        valid = false;
        ConstraintViolation violation(
            "Welding Gap",
            joint_id,
            ConstraintSeverity::ERROR,
            "Joint gap " + std::to_string(actual_gap_mm) + "mm outside range",
            "Adjust joint gap to " + std::to_string(c.min_joint_gap_mm) + "-" + 
                std::to_string(c.max_joint_gap_mm) + "mm",
            std::abs(actual_gap_mm - c.min_joint_gap_mm),
            "mm"
        );
        addViolation(violation);
    }
    
    // Check weld size
    if (actual_weld_size_mm < c.min_weld_size_mm || actual_weld_size_mm > c.max_weld_size_mm) {
        valid = false;
        ConstraintViolation violation(
            "Weld Size",
            joint_id,
            ConstraintSeverity::ERROR,
            "Weld size outside specification",
            "Adjust weld size to specification",
            std::abs(actual_weld_size_mm - c.min_weld_size_mm),
            "mm"
        );
        addViolation(violation);
    }
    
    // Check root pass requirement
    if (c.requires_root_pass && !has_root_pass) {
        valid = false;
        ConstraintViolation violation(
            "Root Pass Required",
            joint_id,
            ConstraintSeverity::CRITICAL,
            "Root pass required but not specified",
            "Specify root pass in welding procedure",
            1.0,
            "pass"
        );
        addViolation(violation);
    }
    
    // Check surface prep
    if (c.requires_surface_prep && surface_prep.empty()) {
        valid = false;
        ConstraintViolation violation(
            "Surface Preparation",
            joint_id,
            ConstraintSeverity::WARNING,
            "Surface preparation required but not specified",
            "Specify surface preparation method: " + c.surface_prep_method,
            1.0,
            "prep"
        );
        addViolation(violation);
    }
    
    return valid;
}

void ManufacturingConstraintsManager::addMachiningConstraint(
    const std::string& feature_id,
    const MachiningConstraint& constraint) {
    constraint.validate();
    machining_constraints_[feature_id] = constraint;
}

std::optional<MachiningConstraint> ManufacturingConstraintsManager::getMachiningConstraint(
    const std::string& feature_id) const {
    auto it = machining_constraints_.find(feature_id);
    if (it != machining_constraints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ManufacturingConstraintsManager::validateMachining(
    const std::string& feature_id,
    double tool_clearance_mm,
    double surface_finish_ra_um,
    double tolerance_um,
    double feature_size_mm) {
    
    auto constraint = getMachiningConstraint(feature_id);
    if (!constraint) {
        return true;
    }
    
    bool valid = true;
    const auto& c = constraint.value();
    
    // Check tool clearance
    if (tool_clearance_mm < c.min_tool_clearance_mm) {
        valid = false;
        ConstraintViolation violation(
            "Tool Clearance",
            feature_id,
            ConstraintSeverity::ERROR,
            "Tool clearance insufficient",
            "Increase tool clearance to at least " + std::to_string(c.min_tool_clearance_mm) + "mm",
            c.min_tool_clearance_mm - tool_clearance_mm,
            "mm"
        );
        addViolation(violation);
    }
    
    // Check surface finish
    if (surface_finish_ra_um > c.surface_finish_ra_um) {
        valid = false;
        ConstraintViolation violation(
            "Surface Finish",
            feature_id,
            ConstraintSeverity::WARNING,
            "Surface finish requirement not met",
            "Improve surface finish to " + std::to_string(c.surface_finish_ra_um) + "µm or better",
            surface_finish_ra_um - c.surface_finish_ra_um,
            "µm"
        );
        addViolation(violation);
    }
    
    // Check tolerance
    if (tolerance_um > c.tolerance_grade_um) {
        valid = false;
        ConstraintViolation violation(
            "Tolerance Grade",
            feature_id,
            ConstraintSeverity::ERROR,
            "Tolerance tighter than achievable",
            "Relax tolerance to " + std::to_string(c.tolerance_grade_um) + "µm or looser",
            tolerance_um - c.tolerance_grade_um,
            "µm"
        );
        addViolation(violation);
    }
    
    // Check feature size
    if (feature_size_mm < c.min_feature_size_mm || feature_size_mm > c.max_feature_size_mm) {
        valid = false;
        ConstraintViolation violation(
            "Feature Size",
            feature_id,
            ConstraintSeverity::ERROR,
            "Feature size outside machinable range",
            "Adjust feature size to " + std::to_string(c.min_feature_size_mm) + "-" +
                std::to_string(c.max_feature_size_mm) + "mm",
            std::abs(feature_size_mm - c.min_feature_size_mm),
            "mm"
        );
        addViolation(violation);
    }
    
    return valid;
}

void ManufacturingConstraintsManager::addAssemblyConstraint(
    const std::string& assembly_id,
    const AssemblyConstraint& constraint) {
    constraint.validate();
    assembly_constraints_[assembly_id] = constraint;
}

std::optional<AssemblyConstraint> ManufacturingConstraintsManager::getAssemblyConstraint(
    const std::string& assembly_id) const {
    auto it = assembly_constraints_.find(assembly_id);
    if (it != assembly_constraints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ManufacturingConstraintsManager::validateAssembly(
    const std::string& assembly_id,
    double interference_mm,
    double clearance_mm,
    int assembly_steps) {
    
    auto constraint = getAssemblyConstraint(assembly_id);
    if (!constraint) {
        return true;
    }
    
    bool valid = true;
    const auto& c = constraint.value();
    
    // Check interference
    if (interference_mm > c.interference_tolerance_mm) {
        valid = false;
        ConstraintViolation violation(
            "Part Interference",
            assembly_id,
            ConstraintSeverity::CRITICAL,
            "Parts interfere beyond tolerance",
            "Redesign parts to eliminate interference",
            interference_mm - c.interference_tolerance_mm,
            "mm"
        );
        addViolation(violation);
    }
    
    // Check clearance
    if (clearance_mm < c.clearance_min_mm) {
        valid = false;
        ConstraintViolation violation(
            "Minimum Clearance",
            assembly_id,
            ConstraintSeverity::WARNING,
            "Clearance below minimum",
            "Increase clearance to at least " + std::to_string(c.clearance_min_mm) + "mm",
            c.clearance_min_mm - clearance_mm,
            "mm"
        );
        addViolation(violation);
    }
    
    // Check assembly steps
    if (assembly_steps < c.assembly_steps_minimum || assembly_steps > c.assembly_steps_maximum) {
        valid = false;
        ConstraintViolation violation(
            "Assembly Steps",
            assembly_id,
            ConstraintSeverity::WARNING,
            "Number of assembly steps outside feasible range",
            "Optimize assembly sequence to " + std::to_string(c.assembly_steps_minimum) + "-" +
                std::to_string(c.assembly_steps_maximum) + " steps",
            std::abs(assembly_steps - c.assembly_steps_minimum),
            "steps"
        );
        addViolation(violation);
    }
    
    return valid;
}

void ManufacturingConstraintsManager::addMaterialConstraint(
    const std::string& part_id,
    const MaterialConstraint& constraint) {
    constraint.validate();
    material_constraints_[part_id] = constraint;
}

std::optional<MaterialConstraint> ManufacturingConstraintsManager::getMaterialConstraint(
    const std::string& part_id) const {
    auto it = material_constraints_.find(part_id);
    if (it != material_constraints_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ManufacturingConstraintsManager::validateMaterial(
    const std::string& part_id,
    double actual_cost_usd,
    bool material_available,
    int actual_lead_time_days) {
    
    auto constraint = getMaterialConstraint(part_id);
    if (!constraint) {
        return true;
    }
    
    bool valid = true;
    const auto& c = constraint.value();
    
    // Check cost
    if (actual_cost_usd > c.max_material_cost_usd) {
        valid = false;
        ConstraintViolation violation(
            "Material Cost",
            part_id,
            ConstraintSeverity::WARNING,
            "Material cost exceeds budget",
            "Consider alternative materials or increase budget",
            actual_cost_usd - c.max_material_cost_usd,
            "USD"
        );
        addViolation(violation);
    }
    
    // Check availability
    if (!material_available && c.material_in_stock) {
        valid = false;
        ConstraintViolation violation(
            "Material Availability",
            part_id,
            ConstraintSeverity::ERROR,
            "Required material not in stock",
            "Order material or substitute with available alternative",
            1.0,
            "item"
        );
        addViolation(violation);
    }
    
    // Check lead time
    if (actual_lead_time_days > 30 && c.material_in_stock) {  // Assuming 30-day default limit
        valid = false;
        ConstraintViolation violation(
            "Lead Time",
            part_id,
            ConstraintSeverity::WARNING,
            "Material lead time exceeds project schedule",
            "Order material earlier or find expedited supplier",
            actual_lead_time_days - 30,
            "days"
        );
        addViolation(violation);
    }
    
    return valid;
}

bool ManufacturingConstraintsManager::validateAllConstraints() {
    violations_.clear();
    bool all_valid = true;
    
    // Validate all tube bending constraints
    for (const auto& [entity_id, constraint] : tube_bending_constraints_) {
        // Note: actual values would come from the design model
        // This is just a structure check
        try {
            constraint.validate();
        } catch (...) {
            all_valid = false;
        }
    }
    
    // Validate all welding constraints
    for (const auto& [joint_id, constraint] : welding_constraints_) {
        try {
            constraint.validate();
        } catch (...) {
            all_valid = false;
        }
    }
    
    // Validate all machining constraints
    for (const auto& [feature_id, constraint] : machining_constraints_) {
        try {
            constraint.validate();
        } catch (...) {
            all_valid = false;
        }
    }
    
    // Validate all assembly constraints
    for (const auto& [assembly_id, constraint] : assembly_constraints_) {
        try {
            constraint.validate();
        } catch (...) {
            all_valid = false;
        }
    }
    
    // Validate all material constraints
    for (const auto& [part_id, constraint] : material_constraints_) {
        try {
            constraint.validate();
        } catch (...) {
            all_valid = false;
        }
    }
    
    return all_valid;
}

const std::vector<ConstraintViolation>& ManufacturingConstraintsManager::getViolations() const {
    return violations_;
}

void ManufacturingConstraintsManager::clearViolations() {
    violations_.clear();
}

size_t ManufacturingConstraintsManager::getViolationCount() const {
    return violations_.size();
}

size_t ManufacturingConstraintsManager::getErrorCount() const {
    return std::count_if(violations_.begin(), violations_.end(),
        [](const ConstraintViolation& v) { return v.severity == ConstraintSeverity::ERROR; });
}

size_t ManufacturingConstraintsManager::getCriticalCount() const {
    return std::count_if(violations_.begin(), violations_.end(),
        [](const ConstraintViolation& v) { return v.severity == ConstraintSeverity::CRITICAL; });
}

size_t ManufacturingConstraintsManager::getWarningCount() const {
    return std::count_if(violations_.begin(), violations_.end(),
        [](const ConstraintViolation& v) { return v.severity == ConstraintSeverity::WARNING; });
}

std::string ManufacturingConstraintsManager::generateConstraintReport() const {
    std::ostringstream oss;
    oss << "=== MANUFACTURING CONSTRAINTS REPORT ===\n\n";
    
    oss << "TUBE BENDING CONSTRAINTS: " << tube_bending_constraints_.size() << "\n";
    for (const auto& [id, constraint] : tube_bending_constraints_) {
        oss << "  [" << id << "] " << constraint.getDescription() << "\n";
    }
    
    oss << "\nWELDING CONSTRAINTS: " << welding_constraints_.size() << "\n";
    for (const auto& [id, constraint] : welding_constraints_) {
        oss << "  [" << id << "] " << constraint.getDescription() << "\n";
    }
    
    oss << "\nMACHINING CONSTRAINTS: " << machining_constraints_.size() << "\n";
    for (const auto& [id, constraint] : machining_constraints_) {
        oss << "  [" << id << "] " << constraint.getDescription() << "\n";
    }
    
    oss << "\nASSEMBLY CONSTRAINTS: " << assembly_constraints_.size() << "\n";
    for (const auto& [id, constraint] : assembly_constraints_) {
        oss << "  [" << id << "] " << constraint.getDescription() << "\n";
    }
    
    oss << "\nMATERIAL CONSTRAINTS: " << material_constraints_.size() << "\n";
    for (const auto& [id, constraint] : material_constraints_) {
        oss << "  [" << id << "] " << constraint.getDescription() << "\n";
    }
    
    return oss.str();
}

std::string ManufacturingConstraintsManager::generateViolationReport() const {
    std::ostringstream oss;
    oss << "=== CONSTRAINT VIOLATION REPORT ===\n\n";
    
    if (violations_.empty()) {
        oss << "No violations detected.\n";
        return oss.str();
    }
    
    oss << "Total Violations: " << violations_.size() << "\n";
    oss << "Critical: " << getCriticalCount() << "\n";
    oss << "Errors: " << getErrorCount() << "\n";
    oss << "Warnings: " << getWarningCount() << "\n\n";
    
    for (const auto& violation : violations_) {
        oss << violation.toString() << "\n\n";
    }
    
    return oss.str();
}

std::string ManufacturingConstraintsManager::generateDesignRecommendations() const {
    std::ostringstream oss;
    oss << "=== DESIGN RECOMMENDATIONS ===\n\n";
    
    for (const auto& violation : violations_) {
        if (violation.severity == ConstraintSeverity::CRITICAL ||
            violation.severity == ConstraintSeverity::ERROR) {
            oss << "PRIORITY: " << violation.recommendation << "\n";
            oss << "  (Affected: " << violation.entity_id << ")\n\n";
        }
    }
    
    return oss.str();
}

void ManufacturingConstraintsManager::clearAllConstraints() {
    tube_bending_constraints_.clear();
    welding_constraints_.clear();
    machining_constraints_.clear();
    assembly_constraints_.clear();
    material_constraints_.clear();
    violations_.clear();
}

size_t ManufacturingConstraintsManager::getTotalConstraintCount() const {
    return tube_bending_constraints_.size() +
           welding_constraints_.size() +
           machining_constraints_.size() +
           assembly_constraints_.size() +
           material_constraints_.size();
}

void ManufacturingConstraintsManager::addViolation(const ConstraintViolation& violation) {
    violations_.push_back(violation);
}

void ManufacturingConstraintsManager::recordTubeBendingViolation(
    const std::string& entity_id,
    const TubeBendingConstraint& constraint,
    const std::string& violation_detail,
    double magnitude,
    const std::string& unit) {
    
    ConstraintViolation violation(
        "Tube Bending",
        entity_id,
        ConstraintSeverity::ERROR,
        violation_detail,
        "Review " + constraint.material_name + " tube bending specifications",
        magnitude,
        unit
    );
    addViolation(violation);
}

// ============================================================================
// FeasibilityAssessment Implementation
// ============================================================================

std::string FeasibilityAssessment::getSummary() const {
    std::ostringstream oss;
    oss << "Feasibility Assessment: ";
    if (overall_feasible) {
        oss << "FEASIBLE";
    } else {
        oss << "NOT FEASIBLE";
    }
    oss << "\nTube Bending: " << (tube_bending_feasible ? "OK" : "FAIL");
    oss << "\nWelding: " << (welding_feasible ? "OK" : "FAIL");
    oss << "\nMachining: " << (machining_feasible ? "OK" : "FAIL");
    oss << "\nAssembly: " << (assembly_feasible ? "OK" : "FAIL");
    oss << "\nMaterial: " << (material_feasible ? "OK" : "FAIL");
    oss << "\nViolations: " << total_violations << " (" << critical_violations << " critical)";
    return oss.str();
}

// ============================================================================
// Helper Functions
// ============================================================================

namespace helpers {

double calculateMinBendRadius(double diameter_mm, double wall_thickness_mm) {
    if (diameter_mm <= 0 || wall_thickness_mm <= 0) {
        return 0;
    }
    // Rule of thumb: min bend radius ≈ 2-3x diameter for tube bending
    return diameter_mm * 2.5;
}

double calculateSpringBackCompensation(
    double material_yield_strength_mpa,
    double bend_radius_mm,
    double tube_diameter_mm) {
    
    if (material_yield_strength_mpa <= 0 || bend_radius_mm <= 0 || tube_diameter_mm <= 0) {
        return 0;
    }
    
    // Spring-back compensation based on yield strength and bend geometry
    double ratio = bend_radius_mm / tube_diameter_mm;
    double spring_back = 1.0 - (material_yield_strength_mpa / 250.0) * 0.05;  // Simplified formula
    
    return std::max(0.0, std::min(1.0, spring_back));
}

bool checkThermalDistortion(
    double weld_heat_input_kj_per_mm,
    double plate_thickness_mm,
    double distortion_limit_mm) {
    
    if (plate_thickness_mm <= 0 || distortion_limit_mm <= 0) {
        return false;
    }
    
    // Simplified thermal distortion calculation
    double estimated_distortion = (weld_heat_input_kj_per_mm * 0.5) / plate_thickness_mm;
    
    return estimated_distortion <= distortion_limit_mm;
}

double calculateToolClearance(double tool_diameter_mm, double workpiece_height_mm) {
    if (tool_diameter_mm <= 0 || workpiece_height_mm <= 0) {
        return 0;
    }
    
    // Clearance = 20% of tool diameter, minimum 2mm
    return std::max(2.0, tool_diameter_mm * 0.2);
}

double estimateAssemblyTime(int number_of_parts, int number_of_fasteners) {
    if (number_of_parts <= 0 || number_of_fasteners <= 0) {
        return 0;
    }
    
    // Simplified estimation: 0.25 hours per part + 0.1 hours per fastener
    return (number_of_parts * 0.25) + (number_of_fasteners * 0.1);
}

} // namespace helpers

} // namespace bendcore::manufacturing
