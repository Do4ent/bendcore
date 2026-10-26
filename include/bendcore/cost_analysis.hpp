#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cmath>
#include <memory>
#include <algorithm>

namespace bendcore::cost_analysis {

// ==============================================================================
// BASIC TYPES AND STRUCTURES
// ==============================================================================

enum class MaterialType {
    CARBON_STEEL,
    STAINLESS_STEEL,
    ALUMINUM,
    TITANIUM,
    COPPER,
    NICKEL_ALLOY,
    BRASS,
    OTHER
};

enum class CostComponentType {
    MATERIAL,
    LABOR,
    MACHINE_TIME,
    OVERHEAD,
    QUALITY_CONTROL,
    LOGISTICS,
    PROFIT_MARGIN
};

struct PriceQuote {
    std::string supplierId;
    double pricePerKg;
    double minimumOrder;    // kg
    double leadTime;        // days
    std::string currency;
    int validityDays;
    bool bulkDiscount;
    double discountPercentage;
};

struct BulkPricingTier {
    double minimumQuantity;  // kg
    double maximumQuantity;  // kg
    double pricePerUnit;
    double discount;         // 0.0 to 1.0
};

struct MaterialSpecification {
    MaterialType type;
    std::string materialName;
    double density;          // g/cm³
    double pricePerKg;
    std::vector<BulkPricingTier> pricingTiers;
    double scrapFactor;      // 0.0 to 1.0 (waste percentage)
    std::string supplier;
};

struct OperationCost {
    std::string operationType;  // "bending", "cutting", "welding", "machining", etc.
    double laborHourlyRate;
    double estimatedTime;       // minutes
    double machineHourlyRate;
    double setupTime;           // minutes
};

struct CostDriver {
    std::string name;
    double impact;              // 0.0 to 1.0 (percentage impact on total cost)
    std::string description;
};

struct SensitivityAnalysisPoint {
    std::string parameterName;
    double percentageChange;    // -100 to +100
    double costImpact;
};

// ==============================================================================
// MATERIAL COST SYSTEM
// ==============================================================================

class MaterialCostCalculator {
public:
    struct MaterialCostResult {
        double rawMaterialCost;
        double scrapCost;
        double bulkDiscount;
        double totalMaterialCost;
        std::string pricingTier;
    };

    // Register material specifications
    void registerMaterial(const MaterialSpecification& spec);

    // Calculate material costs for a tube
    MaterialCostResult calculateMaterialCost(
        const std::string& materialName,
        double length,              // mm
        double outerDiameter,       // mm
        double wallThickness        // mm
    );

    // Get available materials
    std::vector<std::string> getAvailableMaterials() const;

    // Get density for material
    double getMaterialDensity(const std::string& materialName) const;

    // Calculate weight of tube
    double calculateTubeWeight(
        double length,
        double outerDiameter,
        double wallThickness,
        double density
    );

    // Get price quotes from suppliers
    std::vector<PriceQuote> getPriceQuotes(
        const std::string& materialName,
        double quantity
    );

private:
    std::map<std::string, MaterialSpecification> materials_;
    double calculateWeightFromDimensions(
        double length,
        double od,
        double wt,
        double density
    );
};

// ==============================================================================
// MANUFACTURING COST SYSTEM
// ==============================================================================

class ManufacturingCostCalculator {
public:
    struct ManufacturingCostResult {
        double laborCost;
        double machineTimeCost;
        double bendingCost;
        double weldingCost;
        double machiningCost;
        double assemblyCost;
        double testingCost;
        double totalManufacturingCost;
        std::map<std::string, double> costByOperation;
    };

    // Register operation costs
    void registerOperation(const OperationCost& operation);

    // Calculate manufacturing costs
    ManufacturingCostResult calculateManufacturingCost(
        unsigned int numberOfBends,
        unsigned int numberOfCuts,
        unsigned int numberOfWelds,
        unsigned int numberOfMachiningOperations,
        double totalMachineTimeMinutes,
        double laborHoursRequired
    );

    // Get operation cost
    const OperationCost* getOperationCost(const std::string& operationType) const;

    // Estimate time for operation
    double estimateOperationTime(
        const std::string& operationType,
        double complexity
    );

private:
    std::map<std::string, OperationCost> operations_;
};

// ==============================================================================
// PROCESS COST MODEL SYSTEM
// ==============================================================================

class ProcessCostModel {
public:
    struct ProcessCostBreakdown {
        double fixedCosts;
        double variableCosts;
        double overheadAllocation;
        double qualityControlCost;
        double logisticsCost;
        double totalProcessCost;
        std::map<std::string, double> costBreakdown;
    };

    // Set overhead percentage (0-100)
    void setOverheadPercentage(double percentage);

    // Set quality control cost per unit
    void setQualityControlCost(double costPerUnit);

    // Set logistics cost parameters
    void setLogisticsCost(double costPerUnit, double costPerKm);

    // Calculate process costs
    ProcessCostBreakdown calculateProcessCost(
        double materialCost,
        double laborCost,
        double machineTimeCost,
        double productionQuantity,
        double shippingDistance = 0.0
    );

    // Calculate cost per unit
    double calculateCostPerUnit(
        double totalCost,
        double productionQuantity
    );

private:
    double overheadPercentage_ = 15.0;
    double qcCostPerUnit_ = 5.0;
    double logisticsCostPerUnit_ = 2.0;
    double logisticsCostPerKm_ = 0.01;
};

// ==============================================================================
// COST OPTIMIZATION SYSTEM
// ==============================================================================

class CostOptimizer {
public:
    struct OptimizationResult {
        bool success;
        std::string selectedMaterial;
        std::string selectedProcess;
        std::string selectedSupplier;
        double estimatedCost;
        double estimatedSavings;
        double costReductionPercentage;
        std::string recommendation;
    };

    struct DesignAlternative {
        std::string name;
        std::string material;
        std::string process;
        double estimatedCost;
        double performanceScore;
        double manufacturabilityScore;
    };

    // Compare design alternatives
    std::vector<DesignAlternative> compareDesignAlternatives(
        const std::vector<DesignAlternative>& alternatives
    );

    // Optimize for cost
    OptimizationResult optimizeForCost(
        const std::vector<std::string>& availableMaterials,
        const std::vector<std::string>& availableProcesses,
        double targetCost,
        double weightPerformance = 0.3,
        double weightCost = 0.7
    );

    // Find best material
    std::string findBestMaterial(
        const std::vector<std::string>& candidates,
        double targetCost
    );

    // Find best supplier
    std::string findBestSupplier(
        const std::vector<PriceQuote>& quotes,
        double requiredQuantity
    );

    // Analyze cost-performance trade-off
    std::vector<DesignAlternative> analyzeCostPerformanceTradeoff(
        const std::vector<DesignAlternative>& alternatives
    );

private:
    double calculateOptimizationScore(
        const DesignAlternative& alt,
        double weightPerformance,
        double weightCost
    );
};

// ==============================================================================
// COST BREAKDOWN SYSTEM
// ==============================================================================

class CostBreakdownAnalyzer {
public:
    struct CostBreakdownResult {
        std::map<CostComponentType, double> componentCosts;
        std::vector<CostDriver> topCostDrivers;
        double totalCost;
        std::map<std::string, double> costByOperation;
    };

    struct ParametricCostModel {
        std::string name;
        std::vector<std::pair<std::string, double>> parameters;  // name, coefficient
        double baselineCost;
    };

    // Analyze cost breakdown
    CostBreakdownResult analyzeCostBreakdown(
        double materialCost,
        double laborCost,
        double machineTimeCost,
        double overheadCost,
        double qcCost,
        double logisticsCost
    );

    // Identify top cost drivers
    std::vector<CostDriver> identifyCostDrivers(
        const CostBreakdownResult& breakdown,
        int topN = 5
    );

    // Perform sensitivity analysis
    std::vector<SensitivityAnalysisPoint> performSensitivityAnalysis(
        double baseCost,
        const std::vector<std::string>& parameterNames,
        double percentageRanges[] = nullptr
    );

    // Create parametric cost model
    ParametricCostModel createParametricModel(
        const std::string& name,
        double baselineCost,
        const std::map<std::string, double>& parameterCoefficients
    );

    // Estimate cost using parametric model
    double estimateCostWithModel(
        const ParametricCostModel& model,
        const std::map<std::string, double>& parameterValues
    );

private:
    std::vector<ParametricCostModel> models_;
};

// ==============================================================================
// COST REPORTING AND VISUALIZATION
// ==============================================================================

class CostReporter {
public:
    struct CostReport {
        std::string title;
        std::string generatedDate;
        double totalCost;
        std::map<std::string, double> costSummary;
        std::string reportContent;
        std::vector<std::string> charts;
    };

    struct CostComparison {
        std::string baselineScenario;
        std::string comparisonScenario;
        double baselineCost;
        double comparisonCost;
        double difference;
        double percentageChange;
    };

    // Generate cost report
    CostReport generateCostReport(
        const std::string& partName,
        const std::map<CostComponentType, double>& costs,
        const std::string& format = "json"  // "json", "html", "csv"
    );

    // Generate cost comparison
    CostComparison compareCosts(
        double baselineCost,
        double comparisonCost
    );

    // Generate cost breakdown chart
    std::string generateCostBreakdownChart(
        const std::map<CostComponentType, double>& costs
    );

    // Generate cost trend analysis
    std::string generateCostTrendAnalysis(
        const std::vector<std::pair<int, double>>& historicalCosts  // week, cost
    );

    // Generate cost forecast
    std::string generateCostForecast(
        const std::vector<std::pair<int, double>>& historicalData,
        int forecastWeeks = 12
    );

    // Budget tracking
    struct BudgetStatus {
        double allocatedBudget;
        double currentSpend;
        double remainingBudget;
        double percentageUsed;
        bool isOnBudget;
    };

    BudgetStatus trackBudget(
        double allocatedBudget,
        double currentSpend
    );

private:
    std::string formatAsJson(const std::map<CostComponentType, double>& costs);
    std::string formatAsHtml(const std::map<CostComponentType, double>& costs);
    std::string formatAsCsv(const std::map<CostComponentType, double>& costs);
};

// ==============================================================================
// INTEGRATED COST ANALYSIS SYSTEM
// ==============================================================================

class CostAnalysisSystem {
public:
    struct CompleteAnalysis {
        double materialCost;
        double manufacturingCost;
        double processCost;
        double totalCost;
        std::map<CostComponentType, double> costBreakdown;
        std::vector<CostDriver> costDrivers;
        CostOptimizer::OptimizationResult optimization;
        CostBreakdownAnalyzer::CostBreakdownResult breakdown;
        CostReporter::CostReport report;
    };

    // Initialize system
    void initialize();

    // Perform complete cost analysis
    CompleteAnalysis performCompleteAnalysis(
        const std::string& partName,
        const std::string& materialName,
        double tubeLength,
        double outerDiameter,
        double wallThickness,
        unsigned int numberOfBends,
        unsigned int numberOfCuts,
        unsigned int numberOfWelds,
        double productionQuantity = 1.0
    );

    // Get material calculator
    MaterialCostCalculator& getMaterialCalculator();

    // Get manufacturing calculator
    ManufacturingCostCalculator& getManufacturingCalculator();

    // Get process cost model
    ProcessCostModel& getProcessCostModel();

    // Get optimizer
    CostOptimizer& getOptimizer();

    // Get breakdown analyzer
    CostBreakdownAnalyzer& getBreakdownAnalyzer();

    // Get reporter
    CostReporter& getReporter();

private:
    MaterialCostCalculator materialCalc_;
    ManufacturingCostCalculator mfgCalc_;
    ProcessCostModel processModel_;
    CostOptimizer optimizer_;
    CostBreakdownAnalyzer breakdown_;
    CostReporter reporter_;
};

}  // namespace bendcore::cost_analysis
