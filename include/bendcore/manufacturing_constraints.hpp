#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <map>
#include <stdexcept>
#include <cmath>

namespace bendcore::manufacturing {

// ============================================================================
// Material Type Enumeration
// ============================================================================

enum class MaterialType {
    STEEL,
    STAINLESS_STEEL,
    ALUMINUM,
    COPPER,
    TITANIUM,
    NICKEL_ALLOY,
    COMPOSITE,
    OTHER
};

// ============================================================================
// Constraint Violation Severity
// ============================================================================

enum class ConstraintSeverity {
    INFO,           // Non-blocking informational message
    WARNING,        // Warning but design still feasible
    ERROR,          // Constraint violated - design change required
    CRITICAL        // Critical failure - design cannot be manufactured
};

// ============================================================================
// Tube Bending Constraints
// ============================================================================

struct TubeBendingConstraint {
    double min_bend_radius_mm;              // Minimum bend radius based on tube diameter
    double max_bend_radius_mm;              // Maximum bend radius before excessive stress
    double min_wall_thickness_mm;           // Minimum wall thickness to avoid collapse
    double max_wall_thickness_mm;           // Maximum wall thickness for tool access
    double min_diameter_mm;                 // Minimum tube diameter
    double max_diameter_mm;                 // Maximum tube diameter
    double diameter_to_thickness_ratio;     // D/t ratio limit
    double spring_back_factor;              // Spring-back compensation factor (0.0-1.0)
    double min_bend_angle_deg;              // Minimum bend angle (typically > 5°)
    double max_bend_angle_deg;              // Maximum bend angle (typically < 175°)
    std::string material_name;              // Material designation
    
    bool validate() const;
    std::string getDescription() const;
};

// ============================================================================
// Welding Constraints
// ============================================================================

struct WeldingConstraint {
    double min_joint_gap_mm;                // Minimum gap for weld access
    double max_joint_gap_mm;                // Maximum gap before root pass issues
    double min_weld_size_mm;                // Minimum weld bead size
    double max_weld_size_mm;                // Maximum weld bead size
    double min_weld_length_mm;              // Minimum weld length
    bool requires_root_pass;                // Whether multi-pass welding needed
    bool requires_stress_relief;            // Post-weld stress relief requirement
    double preheat_temperature_c;           // Preheat temperature requirement (0 if none)
    bool requires_surface_prep;             // Surface preparation requirement
    std::string surface_prep_method;        // Grinding, wire brush, pickling, etc.
    double thermal_distortion_limit_mm;     // Maximum acceptable thermal distortion
    bool accessible_from_front;             // Can access joint from front
    bool accessible_from_back;              // Can access joint from back
    
    bool validate() const;
    std::string getDescription() const;
};

// ============================================================================
// Machining Constraints
// ============================================================================

struct MachiningConstraint {
    double tool_diameter_mm;                // Cutting tool diameter
    double min_tool_clearance_mm;           // Minimum clearance around tool
    double max_tool_engagement_mm;          // Maximum tool engagement
    double surface_finish_ra_um;            // Surface finish requirement (Ra micrometers)
    double tolerance_grade_um;              // Tolerance specification
    double min_feature_size_mm;             // Minimum feature size machinable
    double max_feature_size_mm;             // Maximum feature size machinable
    bool requires_special_coolant;          // Special coolant requirement
    std::string coolant_type;               // Type of coolant needed
    double spindle_speed_rpm;               // Spindle speed requirement
    double feed_rate_mm_per_min;            // Feed rate
    int tool_changes_required;              // Number of tool changes needed
    
    bool validate() const;
    std::string getDescription() const;
};

// ============================================================================
// Assembly Constraints
// ============================================================================

struct AssemblyConstraint {
    double interference_tolerance_mm;       // Max allowable interference
    double clearance_min_mm;                // Minimum required clearance
    double fastener_access_diameter_mm;     // Minimum hole for fastener access
    double maintenance_clearance_mm;        // Space needed for maintenance
    double service_access_width_mm;         // Width needed for service access
    bool requires_sequence_verification;    // Assembly sequence must be verified
    int assembly_steps_minimum;             // Minimum assembly steps
    int assembly_steps_maximum;             // Maximum acceptable steps
    bool disassembly_required_for_maintenance;  // Can parts be maintained?
    double estimated_assembly_time_hours;   // Estimated time to assemble
    
    bool validate() const;
    std::string getDescription() const;
};

// ============================================================================
// Material Constraints
// ============================================================================

struct MaterialConstraint {
    MaterialType material_type;
    double density_g_per_cc;                // Material density
    double yield_strength_mpa;              // Yield strength
    double tensile_strength_mpa;            // Tensile strength
    double elongation_percent;              // Elongation at break
    bool certified_material;                // Must use certified material
    std::string certification_standard;     // e.g., "ASTM A276", "AMS 4928"
    double material_cost_per_kg;            // Cost per kilogram
    double max_material_cost_usd;           // Maximum allowed cost
    bool material_in_stock;                 // Availability
    int lead_time_days;                     // Procurement lead time
    double thermal_conductivity_w_per_mk;   // Thermal conductivity
    double max_operating_temperature_c;     // Maximum operating temperature
    
    bool validate() const;
    std::string getDescription() const;
};

// ============================================================================
// Constraint Violation Report
// ============================================================================

struct ConstraintViolation {
    std::string constraint_name;
    std::string entity_id;
    ConstraintSeverity severity;
    std::string description;
    std::string recommendation;
    double violation_magnitude;             // How much the constraint is violated by
    std::string violation_unit;             // Unit of measurement (mm, degrees, etc.)
    
    ConstraintViolation(
        const std::string& name,
        const std::string& entity,
        ConstraintSeverity sev,
        const std::string& desc,
        const std::string& rec,
        double magnitude,
        const std::string& unit
    ) : constraint_name(name), entity_id(entity), severity(sev),
        description(desc), recommendation(rec), violation_magnitude(magnitude),
        violation_unit(unit) {}
    
    std::string getSeverityString() const;
    std::string toString() const;
};

// ============================================================================
// Manufacturing Constraints Manager
// ============================================================================

class ManufacturingConstraintsManager {
public:
    ManufacturingConstraintsManager() = default;
    ~ManufacturingConstraintsManager() = default;
    
    // Tube bending operations
    void addTubeBendingConstraint(
        const std::string& entity_id,
        const TubeBendingConstraint& constraint
    );
    
    std::optional<TubeBendingConstraint> getTubeBendingConstraint(
        const std::string& entity_id
    ) const;
    
    bool validateTubeBending(
        const std::string& entity_id,
        double actual_bend_radius_mm,
        double actual_wall_thickness_mm,
        double actual_diameter_mm,
        double actual_bend_angle_deg
    );
    
    // Welding operations
    void addWeldingConstraint(
        const std::string& joint_id,
        const WeldingConstraint& constraint
    );
    
    std::optional<WeldingConstraint> getWeldingConstraint(
        const std::string& joint_id
    ) const;
    
    bool validateWelding(
        const std::string& joint_id,
        double actual_gap_mm,
        double actual_weld_size_mm,
        bool has_root_pass,
        const std::string& surface_prep
    );
    
    // Machining operations
    void addMachiningConstraint(
        const std::string& feature_id,
        const MachiningConstraint& constraint
    );
    
    std::optional<MachiningConstraint> getMachiningConstraint(
        const std::string& feature_id
    ) const;
    
    bool validateMachining(
        const std::string& feature_id,
        double tool_clearance_mm,
        double surface_finish_ra_um,
        double tolerance_um,
        double feature_size_mm
    );
    
    // Assembly operations
    void addAssemblyConstraint(
        const std::string& assembly_id,
        const AssemblyConstraint& constraint
    );
    
    std::optional<AssemblyConstraint> getAssemblyConstraint(
        const std::string& assembly_id
    ) const;
    
    bool validateAssembly(
        const std::string& assembly_id,
        double interference_mm,
        double clearance_mm,
        int assembly_steps
    );
    
    // Material operations
    void addMaterialConstraint(
        const std::string& part_id,
        const MaterialConstraint& constraint
    );
    
    std::optional<MaterialConstraint> getMaterialConstraint(
        const std::string& part_id
    ) const;
    
    bool validateMaterial(
        const std::string& part_id,
        double actual_cost_usd,
        bool material_available,
        int actual_lead_time_days
    );
    
    // Comprehensive validation
    bool validateAllConstraints();
    
    // Violation reporting
    const std::vector<ConstraintViolation>& getViolations() const;
    void clearViolations();
    
    size_t getViolationCount() const;
    size_t getErrorCount() const;
    size_t getCriticalCount() const;
    size_t getWarningCount() const;
    
    // Reporting
    std::string generateConstraintReport() const;
    std::string generateViolationReport() const;
    std::string generateDesignRecommendations() const;
    
    // Utilities
    void clearAllConstraints();
    size_t getTotalConstraintCount() const;
    
private:
    std::map<std::string, TubeBendingConstraint> tube_bending_constraints_;
    std::map<std::string, WeldingConstraint> welding_constraints_;
    std::map<std::string, MachiningConstraint> machining_constraints_;
    std::map<std::string, AssemblyConstraint> assembly_constraints_;
    std::map<std::string, MaterialConstraint> material_constraints_;
    
    std::vector<ConstraintViolation> violations_;
    
    void addViolation(const ConstraintViolation& violation);
    void recordTubeBendingViolation(
        const std::string& entity_id,
        const TubeBendingConstraint& constraint,
        const std::string& violation_detail,
        double magnitude,
        const std::string& unit
    );
};

// ============================================================================
// Feasibility Assessment
// ============================================================================

struct FeasibilityAssessment {
    bool overall_feasible;
    bool tube_bending_feasible;
    bool welding_feasible;
    bool machining_feasible;
    bool assembly_feasible;
    bool material_feasible;
    
    int total_violations;
    int critical_violations;
    
    std::string getSummary() const;
};

// ============================================================================
// Design Helper Functions
// ============================================================================

namespace helpers {
    // Calculate minimum bend radius for a tube
    double calculateMinBendRadius(double diameter_mm, double wall_thickness_mm);
    
    // Calculate spring-back compensation
    double calculateSpringBackCompensation(
        double material_yield_strength_mpa,
        double bend_radius_mm,
        double tube_diameter_mm
    );
    
    // Check thermal distortion feasibility
    bool checkThermalDistortion(
        double weld_heat_input_kj_per_mm,
        double plate_thickness_mm,
        double distortion_limit_mm
    );
    
    // Calculate tool clearance requirement
    double calculateToolClearance(double tool_diameter_mm, double workpiece_height_mm);
    
    // Estimate assembly time
    double estimateAssemblyTime(int number_of_parts, int number_of_fasteners);
}

} // namespace bendcore::manufacturing
