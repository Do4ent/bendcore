#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cmath>
#include <memory>
#include <algorithm>

namespace bendcore::welding {

// ==============================================================================
// ENUMERATIONS AND CONSTANTS
// ==============================================================================

enum class WeldingProcess {
    MIG_MAG,                    // Metal Inert Gas / Metal Active Gas
    TIG,                        // Tungsten Inert Gas
    STICK,                      // Shielded Metal Arc Welding (SMAW)
    FLUX_CORE_ARC,              // Flux-Cored Arc Welding (FCAW)
    LASER                       // Laser Welding
};

enum class JointType {
    BUTT,                       // End-to-end joint
    LAP,                        // Overlapping joint
    T_JOINT,                    // T-shaped joint
    CORNER,                     // Corner joint
    EDGE,                       // Edge joint
    FILLET                      // Fillet weld
};

enum class WeldingPosition {
    FLAT,                       // 1F: Flat position
    HORIZONTAL,                 // 2F: Horizontal position
    VERTICAL,                   // 3F: Vertical position
    OVERHEAD,                   // 4F: Overhead position
    HORIZONTAL_AXIS,            // 1G: Horizontal axis
    VERTICAL_AXIS,              // 2G: Vertical axis
    OVERHEAD_AXIS,              // 4G: Overhead axis
    PIPE_ROLL                   // 1G: Rotating pipe
};

enum class FillerMetalType {
    CARBON_STEEL,
    STAINLESS_STEEL,
    ALUMINUM,
    NICKEL_ALLOY,
    TITANIUM,
    COPPER_ALLOY,
    COBALT_ALLOY,
    EXOTIC
};

enum class ShieldingGasType {
    ARGON,
    HELIUM,
    CARBON_DIOXIDE,
    ARGON_CARBON_DIOXIDE,
    ARGON_HELIUM,
    NITROGEN,
    NONE
};

enum class HeatTreatmentType {
    STRESS_RELIEF,
    NORMALIZATION,
    HARDENING,
    TEMPERING,
    ANNEALING,
    SOLUTION_TREATMENT,
    AGE_HARDENING,
    NONE
};

enum class NDTMethod {
    VISUAL_INSPECTION,
    RADIOGRAPHY,
    ULTRASONIC,
    MAGNETIC_PARTICLE,
    LIQUID_PENETRANT,
    EDDY_CURRENT,
    THERMOGRAPHY,
    ACOUSTIC_EMISSION
};

// ==============================================================================
// BASIC STRUCTURES
// ==============================================================================

struct WeldingParameters {
    double voltage;             // V
    double current;             // A
    double travelSpeed;         // mm/min
    double wireSpeed;           // m/min (for MIG/MAG)
    double preHeatTemperature;  // °C
    double interpassTemperature;// °C
    double postHeatTemperature; // °C
    double shieldingGasFlow;    // L/min (CFH)
    double stickoutLength;      // mm (for MIG/MAG)
    WeldingPosition position;
};

struct FillerMetal {
    std::string designation;    // AWS designation (e.g., "ER70S-6")
    FillerMetalType type;
    double diameter;            // mm
    double density;             // g/cm³
    double melting_temperature; // °C
    double carbon_content;      // %
    double yield_strength;      // MPa
    double tensile_strength;    // MPa
    double elongation;          // %
};

struct ShieldingGas {
    ShieldingGasType type;
    double purityPercentage;    // 99.0 to 99.99%
    double density;             // kg/m³ at STP
    double thermalConductivity; // W/(m*K)
    double cost_per_liter;      // currency units
};

struct BaseMetalProperties {
    std::string name;
    double melting_temperature;  // °C
    double thermal_conductivity; // W/(m*K)
    double specific_heat;        // J/(kg*K)
    double coefficient_of_thermal_expansion; // 1/K
    double yield_strength;       // MPa
    double tensile_strength;     // MPa
    double ductility;            // %
    double hardness;             // HV
};

struct JointGeometry {
    JointType type;
    double gapSize;             // mm (0-5mm typical)
    double rootOpening;         // mm
    double root_face;           // mm
    double included_angle;      // degrees
    double backingStripWidth;   // mm
    double chamferDepth;        // mm
    bool backingRequired;
};

struct WeldBeadSpecification {
    double length;              // mm
    double width;               // mm
    double height;              // mm
    double penetrationDepth;    // mm
    double numberOfPasses;
    std::vector<double> passHeights; // mm per pass
};

struct ThermalHistory {
    double peakTemperature;     // °C
    double cooling_rate_at_700; // °C/s (critical for some steels)
    double cooling_rate_at_400; // °C/s
    double time_above_ac3;      // seconds
    double time_at_peak;        // seconds
    double total_heating_time;  // seconds
};

struct ResidualStress {
    double longitudinal;        // MPa
    double transverse;          // MPa
    double shear;              // MPa
    double principal_1;        // MPa
    double principal_2;        // MPa
    double principal_3;        // MPa
    double equivalent_von_mises; // MPa
};

struct WeldQuality {
    double porosity_percentage;
    double inclusions_percentage;
    double cracks_detected;     // number of cracks
    double hardness_uniformity; // 0-1 scale
    double penetration_uniformity; // 0-1 scale
    bool passes_acceptance_criteria;
    std::string defect_description;
};

struct WeldingSequence {
    struct Pass {
        int pass_number;
        double wire_length;      // mm
        double estimated_time;   // seconds
        double estimated_heat_input; // kJ
        WeldingParameters parameters;
        HeatTreatmentType inter_pass_treatment;
        double inter_pass_temperature; // °C
    };

    std::vector<Pass> passes;
    double total_time;          // seconds
    double total_wire_consumed; // mm
    HeatTreatmentType post_weld_heat_treatment;
};

// ==============================================================================
// WELDING PROCESS SYSTEM
// ==============================================================================

class WeldingProcessSelector {
public:
    struct ProcessRecommendation {
        WeldingProcess process;
        double suitability_score; // 0.0-1.0
        std::string reason;
        double estimated_productivity; // mm/min
        double estimated_cost;
        bool recommended;
    };

    // Get recommended processes for a given application
    std::vector<ProcessRecommendation> recommendProcesses(
        const std::string& baseMetal,
        const JointGeometry& joint,
        double thickness,
        double length,
        int quantity
    );

    // Get optimal parameters for a process
    WeldingParameters getOptimalParameters(
        WeldingProcess process,
        const FillerMetal& filler,
        double thickness,
        JointType joint_type
    );

    // Evaluate process suitability
    double evaluateProcessSuitability(
        WeldingProcess process,
        const std::string& baseMetal,
        double thickness,
        JointType joint_type
    );

private:
    double calculateProductivity(WeldingProcess process, double thickness);
};

// ==============================================================================
// WELD DESIGN SYSTEM
// ==============================================================================

class WeldDesigner {
public:
    struct DesignOptimization {
        WeldBeadSpecification specification;
        double safety_factor;
        double weight_reduction_percentage;
        double cost_reduction_percentage;
        bool is_optimized;
    };

    // Design weld bead
    WeldBeadSpecification designWeldBead(
        const JointGeometry& joint,
        double required_strength,
        const BaseMetalProperties& baseMetal,
        const FillerMetal& filler,
        double safety_factor = 1.5
    );

    // Calculate required penetration
    double calculateRequiredPenetration(
        double thickness,
        double joint_efficiency,
        JointType joint_type
    );

    // Check gap tolerance
    bool checkGapTolerance(
        const JointGeometry& joint,
        double tolerance = 0.5
    );

    // Optimize bead specification
    DesignOptimization optimizeBeadSpecification(
        const WeldBeadSpecification& baseline,
        const BaseMetalProperties& baseMetal,
        double cost_limit = 0.0
    );

    // Select filler metal
    FillerMetal selectFillerMetal(
        const BaseMetalProperties& baseMetal,
        double required_strength,
        WeldingProcess process
    );

    // Calculate heat input
    double calculateHeatInput(
        double voltage,
        double current,
        double travel_speed
    );

private:
    double calculateMinimumPenetrationDepth(JointType type, double thickness);
};

// ==============================================================================
// WELDING SIMULATION SYSTEM
// ==============================================================================

class WeldingSimulator {
public:
    struct HeatDistributionResult {
        std::vector<double> temperature_profile; // °C at various distances from weld
        double peak_temperature;
        double affected_zone_width; // mm (HAZ)
        double cooling_rate_average; // °C/s
        ThermalHistory thermal_history;
    };

    struct DistortionPrediction {
        double longitudinal_shrinkage; // mm
        double transverse_shrinkage;   // mm
        double angular_distortion;     // degrees
        double warping;                // mm
        double total_distortion;       // mm
        bool within_tolerance;
    };

    // Simulate heat distribution
    HeatDistributionResult simulateHeatDistribution(
        const WeldingParameters& params,
        const BaseMetalProperties& baseMetal,
        double thickness,
        double length
    );

    // Predict cooling rates
    std::vector<double> predictCoolingRates(
        const ThermalHistory& thermal_history,
        const BaseMetalProperties& baseMetal
    );

    // Predict distortion
    DistortionPrediction predictDistortion(
        double bead_length,
        double thickness,
        const JointGeometry& joint,
        double ambient_temperature = 25.0
    );

    // Calculate residual stress
    ResidualStress calculateResidualStress(
        const ThermalHistory& thermal_history,
        const BaseMetalProperties& baseMetal,
        double thickness,
        double bead_width
    );

    // Predict microstructure
    struct MicrostructurePredicton {
        std::string weld_metal_structure;
        std::string haz_structure;
        double hardness_in_haz;
        double hardness_in_weld;
        double ductility_reduction;
        bool susceptible_to_cracking;
    };

    MicrostructurePredicton predictMicrostructureChanges(
        const BaseMetalProperties& baseMetal,
        const ThermalHistory& thermal_history,
        const FillerMetal& filler
    );

private:
    double calculateHeatInputDensity(
        double voltage,
        double current,
        double travel_speed,
        double bead_width
    );
};

// ==============================================================================
// PROCESS PLANNING SYSTEM
// ==============================================================================

class ProcessPlanner {
public:
    struct ProcessPlan {
        std::vector<WeldingSequence> sequences;
        std::vector<HeatTreatmentType> heat_treatments;
        std::vector<std::string> inspection_checkpoints;
        double estimated_total_time; // hours
        double estimated_material_cost;
        double estimated_labor_cost;
        std::string quality_assurance_plan;
    };

    // Plan welding sequence
    WeldingSequence planWeldingSequence(
        const JointGeometry& joint,
        const WeldBeadSpecification& bead_spec,
        const WeldingParameters& params,
        const BaseMetalProperties& baseMetal
    );

    // Optimize welding sequence for minimum distortion
    WeldingSequence optimizeForMinimumDistortion(
        const WeldingSequence& baseline,
        const BaseMetalProperties& baseMetal,
        double thickness
    );

    // Schedule heat treatment
    std::vector<HeatTreatmentType> scheduleHeatTreatment(
        const BaseMetalProperties& baseMetal,
        const FillerMetal& filler,
        const ThermalHistory& thermal_history,
        bool stress_relief_required = true
    );

    // Generate multi-pass plan
    std::vector<WeldBeadSpecification> planMultiPassWeld(
        double total_thickness,
        double max_pass_height,
        const BaseMetalProperties& baseMetal,
        const FillerMetal& filler
    );

    // Create complete process plan
    ProcessPlan createCompleteProcessPlan(
        const JointGeometry& joint,
        const BaseMetalProperties& baseMetal,
        const FillerMetal& filler,
        WeldingProcess process,
        double total_length,
        double quantity = 1.0
    );

private:
    bool isPreHeatRequired(const BaseMetalProperties& baseMetal, double thickness);
    double calculatePreHeatTemperature(const BaseMetalProperties& baseMetal, double thickness);
};

// ==============================================================================
// QUALITY CONTROL SYSTEM
// ==============================================================================

class QualityControlSystem {
public:
    struct InspectionPlan {
        std::vector<NDTMethod> methods;
        std::vector<double> inspection_locations; // mm along weld
        double sample_percentage;
        std::string acceptance_standard; // e.g., "AWS D1.1"
        std::vector<std::string> required_tests;
    };

    struct AcceptanceCriteria {
        double max_porosity_percentage;
        double max_inclusion_size; // mm
        double max_crack_length;   // mm
        double min_penetration_percentage;
        double max_hardness; // HV
        std::string standard; // e.g., "AWS D1.1"
    };

    // Generate inspection checklist
    InspectionPlan generateInspectionPlan(
        const JointGeometry& joint,
        double length,
        const std::string& acceptance_standard = "AWS D1.1"
    );

    // Predict weld quality
    WeldQuality predictWeldQuality(
        const WeldingParameters& params,
        const BaseMetalProperties& baseMetal,
        const FillerMetal& filler,
        double length
    );

    // Get acceptance criteria
    AcceptanceCriteria getAcceptanceCriteria(
        const std::string& standard = "AWS D1.1",
        const JointGeometry& joint = {},
        double thickness = 0.0
    );

    // Plan NDT strategy
    std::vector<NDTMethod> planNDTStrategy(
        const JointGeometry& joint,
        const BaseMetalProperties& baseMetal,
        const std::string& application
    );

    // Evaluate defect severity
    struct DefectSeverity {
        std::string defect_type;
        double severity_score;  // 0-1 scale
        bool acceptable;
        std::string remedial_action;
    };

    DefectSeverity evaluateDefectSeverity(
        const std::string& defect_type,
        double defect_size,
        const AcceptanceCriteria& criteria
    );

private:
    double calculateSamplingPercentage(double length);
};

// ==============================================================================
// COST & TIME ANALYSIS SYSTEM
// ==============================================================================

class WeldingCostAnalyzer {
public:
    struct CostAnalysis {
        double labor_cost;
        double material_cost;      // filler metal + shielding gas
        double equipment_cost;
        double heat_treatment_cost;
        double inspection_cost;
        double overhead_cost;
        double total_cost;
        std::string cost_breakdown;
    };

    struct TimeAnalysis {
        double preparation_time;   // hours
        double welding_time;       // hours
        double cooling_time;       // hours
        double heat_treatment_time; // hours
        double inspection_time;    // hours
        double total_time;         // hours
        double throughput;         // welds per hour
    };

    // Calculate welding cost
    CostAnalysis calculateCost(
        const WeldingSequence& sequence,
        const FillerMetal& filler,
        const ShieldingGas& gas,
        double labor_hourly_rate = 50.0,
        double overhead_percentage = 20.0
    );

    // Estimate welding time
    TimeAnalysis estimateTime(
        const WeldingSequence& sequence,
        bool includes_heat_treatment = true,
        bool includes_inspection = true
    );

    // Estimate material consumption
    struct MaterialConsumption {
        double filler_metal_weight; // g
        double shielding_gas_volume; // L
        double backing_strip_length; // mm
        double flux_weight;        // g (for FCAW, SMAW)
        double total_waste_percentage;
    };

    MaterialConsumption estimateMaterialConsumption(
        const WeldingSequence& sequence,
        const FillerMetal& filler,
        const ShieldingGas& gas,
        WeldingProcess process
    );

    // Estimate labor hours
    double estimateLaborHours(
        double total_weld_length,
        WeldingProcess process,
        double thickness,
        bool includes_preparation = true
    );

private:
    double calculateWiringConsumption(double length, const FillerMetal& filler);
    double calculateGasConsumption(double time, double flow_rate);
};

// ==============================================================================
// WELDING SPECIFICATION GENERATOR
// ==============================================================================

class WeldingSpecificationGenerator {
public:
    struct WeldingSpecification {
        std::string specification_id;
        std::string title;
        std::string base_metal;
        std::string filler_metal;
        WeldingProcess process;
        JointType joint_type;
        double thickness_range_min; // mm
        double thickness_range_max; // mm
        std::vector<WeldingParameters> qualified_parameters;
        AcceptanceCriteria acceptance_criteria;
        std::vector<std::string> required_tests;
        std::string notes;
    };

    // Create WPS (Welding Procedure Specification)
    WeldingSpecification createWeldingProcedureSpec(
        const std::string& base_metal,
        const FillerMetal& filler,
        WeldingProcess process,
        JointType joint_type,
        double thickness_min,
        double thickness_max
    );

    // Add qualified parameter set
    void addQualifiedParameters(
        WeldingSpecification& spec,
        const WeldingParameters& params
    );

    // Generate specification document
    std::string generateSpecificationDocument(
        const WeldingSpecification& spec,
        const std::string& format = "text"
    );

private:
    std::string formatAsText(const WeldingSpecification& spec);
};

// ==============================================================================
// INTEGRATED WELDING SYSTEM
// ==============================================================================

class WeldingSystem {
public:
    struct CompleteWeldingAnalysis {
        WeldingProcessSelector::ProcessRecommendation recommended_process;
        WeldBeadSpecification bead_design;
        WeldingSimulator::HeatDistributionResult heat_distribution;
        WeldingSimulator::DistortionPrediction distortion_prediction;
        ResidualStress residual_stress;
        ProcessPlanner::ProcessPlan process_plan;
        QualityControlSystem::InspectionPlan inspection_plan;
        WeldingCostAnalyzer::CostAnalysis cost_analysis;
        WeldingCostAnalyzer::TimeAnalysis time_analysis;
        std::string summary_report;
    };

    // Initialize system
    void initialize();

    // Perform complete welding analysis
    CompleteWeldingAnalysis performCompleteAnalysis(
        const std::string& base_metal,
        const JointGeometry& joint,
        double thickness,
        double weld_length,
        double quantity = 1.0,
        const std::string& application = "General"
    );

    // Get process selector
    WeldingProcessSelector& getProcessSelector();

    // Get weld designer
    WeldDesigner& getWeldDesigner();

    // Get simulator
    WeldingSimulator& getSimulator();

    // Get process planner
    ProcessPlanner& getProcessPlanner();

    // Get quality control system
    QualityControlSystem& getQualityControlSystem();

    // Get cost analyzer
    WeldingCostAnalyzer& getCostAnalyzer();

    // Get specification generator
    WeldingSpecificationGenerator& getSpecificationGenerator();

    // Generate executive summary
    std::string generateExecutiveSummary(const CompleteWeldingAnalysis& analysis);

private:
    WeldingProcessSelector process_selector_;
    WeldDesigner weld_designer_;
    WeldingSimulator simulator_;
    ProcessPlanner process_planner_;
    QualityControlSystem quality_control_;
    WeldingCostAnalyzer cost_analyzer_;
    WeldingSpecificationGenerator spec_generator_;
};

}  // namespace bendcore::welding
