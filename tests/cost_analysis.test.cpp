#include <gtest/gtest.h>
#include "bendcore/cost_analysis.hpp"
#include <cmath>

using namespace bendcore::cost_analysis;

// ==============================================================================
// MATERIAL COST CALCULATOR TESTS
// ==============================================================================

class MaterialCostCalculatorTest : public ::testing::Test {
protected:
    MaterialCostCalculator calculator;

    void SetUp() override {
        // Register test materials
        MaterialSpecification steel;
        steel.type = MaterialType::CARBON_STEEL;
        steel.materialName = "Steel";
        steel.density = 7.85;
        steel.pricePerKg = 0.80;
        steel.scrapFactor = 0.05;
        steel.supplier = "Supplier_A";
        steel.pricingTiers.push_back({100.0, 500.0, 0.75, 0.05});
        steel.pricingTiers.push_back({500.0, 1000.0, 0.70, 0.10});
        calculator.registerMaterial(steel);

        MaterialSpecification aluminum;
        aluminum.type = MaterialType::ALUMINUM;
        aluminum.materialName = "Aluminum";
        aluminum.density = 2.70;
        aluminum.pricePerKg = 1.50;
        aluminum.scrapFactor = 0.08;
        aluminum.supplier = "Supplier_B";
        calculator.registerMaterial(aluminum);
    }
};

TEST_F(MaterialCostCalculatorTest, RegisterMaterialSuccessfully) {
    auto materials = calculator.getAvailableMaterials();
    EXPECT_GE(materials.size(), 2);
    EXPECT_NE(std::find(materials.begin(), materials.end(), "Steel"), materials.end());
}

TEST_F(MaterialCostCalculatorTest, CalculateTubeWeight) {
    double weight = calculator.calculateTubeWeight(100.0, 12.0, 1.0, 7.85);
    EXPECT_GT(weight, 0.0);
    EXPECT_LT(weight, 10.0);  // Should be less than 10kg for a small tube
}

TEST_F(MaterialCostCalculatorTest, CalculateMaterialCost) {
    auto result = calculator.calculateMaterialCost("Steel", 100.0, 12.0, 1.0);
    EXPECT_GT(result.totalMaterialCost, 0.0);
    EXPECT_GE(result.rawMaterialCost, 0.0);
    EXPECT_GE(result.scrapCost, 0.0);
}

TEST_F(MaterialCostCalculatorTest, MaterialCostWithScrap) {
    auto result = calculator.calculateMaterialCost("Steel", 100.0, 12.0, 1.0);
    EXPECT_GT(result.scrapCost, 0.0);
    EXPECT_LT(result.scrapCost, result.rawMaterialCost);
}

TEST_F(MaterialCostCalculatorTest, BulkPricingTierApplication) {
    // Large quantity should trigger bulk pricing
    auto result = calculator.calculateMaterialCost("Steel", 500.0, 12.0, 1.0);
    EXPECT_FALSE(result.pricingTier.empty());
}

TEST_F(MaterialCostCalculatorTest, GetMaterialDensity) {
    double density = calculator.getMaterialDensity("Steel");
    EXPECT_EQ(density, 7.85);
}

TEST_F(MaterialCostCalculatorTest, GetMaterialDensityNonexistent) {
    double density = calculator.getMaterialDensity("NonExistent");
    EXPECT_EQ(density, 0.0);
}

TEST_F(MaterialCostCalculatorTest, GetPriceQuotes) {
    auto quotes = calculator.getPriceQuotes("Steel", 250.0);
    EXPECT_GT(quotes.size(), 0);
    if (!quotes.empty()) {
        EXPECT_GT(quotes[0].pricePerKg, 0.0);
        EXPECT_FALSE(quotes[0].supplierId.empty());
    }
}

TEST_F(MaterialCostCalculatorTest, GetAvailableMaterials) {
    auto materials = calculator.getAvailableMaterials();
    EXPECT_EQ(materials.size(), 2);
}

TEST_F(MaterialCostCalculatorTest, CalculateCostForDifferentMaterials) {
    auto steelCost = calculator.calculateMaterialCost("Steel", 100.0, 12.0, 1.0);
    auto aluminumCost = calculator.calculateMaterialCost("Aluminum", 100.0, 12.0, 1.0);

    EXPECT_NE(steelCost.totalMaterialCost, aluminumCost.totalMaterialCost);
}

TEST_F(MaterialCostCalculatorTest, LargeTubeCost) {
    auto result = calculator.calculateMaterialCost("Steel", 1000.0, 25.0, 2.0);
    EXPECT_GT(result.totalMaterialCost, 50.0);
}

TEST_F(MaterialCostCalculatorTest, SmallTubeCost) {
    auto result = calculator.calculateMaterialCost("Steel", 10.0, 5.0, 0.5);
    EXPECT_GT(result.totalMaterialCost, 0.0);
    EXPECT_LT(result.totalMaterialCost, 1.0);
}

// ==============================================================================
// MANUFACTURING COST CALCULATOR TESTS
// ==============================================================================

class ManufacturingCostCalculatorTest : public ::testing::Test {
protected:
    ManufacturingCostCalculator calculator;
};

TEST_F(ManufacturingCostCalculatorTest, RegisterOperation) {
    OperationCost bending;
    bending.operationType = "bending";
    bending.laborHourlyRate = 25.0;
    bending.estimatedTime = 30.0;
    bending.machineHourlyRate = 75.0;
    bending.setupTime = 10.0;

    calculator.registerOperation(bending);
    auto retrievedOp = calculator.getOperationCost("bending");

    EXPECT_NE(retrievedOp, nullptr);
    EXPECT_EQ(retrievedOp->operationType, "bending");
}

TEST_F(ManufacturingCostCalculatorTest, CalculateManufacturingCost) {
    auto result = calculator.calculateManufacturingCost(5, 3, 2, 1, 180.0, 3.0);

    EXPECT_GT(result.totalManufacturingCost, 0.0);
    EXPECT_GE(result.laborCost, 0.0);
    EXPECT_GE(result.machineTimeCost, 0.0);
}

TEST_F(ManufacturingCostCalculatorTest, BendingOperationCost) {
    auto result = calculator.calculateManufacturingCost(10, 0, 0, 0, 0.0, 0.0);

    EXPECT_GT(result.bendingCost, 0.0);
    EXPECT_EQ(result.weldingCost, 0.0);
}

TEST_F(ManufacturingCostCalculatorTest, WeldingOperationCost) {
    auto result = calculator.calculateManufacturingCost(0, 0, 5, 0, 0.0, 0.0);

    EXPECT_GT(result.weldingCost, 0.0);
    EXPECT_EQ(result.bendingCost, 0.0);
}

TEST_F(ManufacturingCostCalculatorTest, AssemblyAndTestingCosts) {
    auto result = calculator.calculateManufacturingCost(5, 2, 3, 0, 0.0, 0.0);

    EXPECT_GT(result.assemblyCost, 0.0);
    EXPECT_GT(result.testingCost, 0.0);
}

TEST_F(ManufacturingCostCalculatorTest, EstimateOperationTime) {
    double time = calculator.estimateOperationTime("unknown", 2.5);
    EXPECT_EQ(time, 25.0);  // Default: 10 * complexity
}

TEST_F(ManufacturingCostCalculatorTest, CostBreakdownByOperation) {
    auto result = calculator.calculateManufacturingCost(3, 2, 1, 0, 120.0, 2.0);

    EXPECT_GT(result.costByOperation.size(), 0);
    EXPECT_NE(result.costByOperation.find("bending"), result.costByOperation.end());
}

TEST_F(ManufacturingCostCalculatorTest, ZeroCostForNoOperations) {
    auto result = calculator.calculateManufacturingCost(0, 0, 0, 0, 0.0, 0.0);

    EXPECT_EQ(result.bendingCost, 0.0);
    EXPECT_EQ(result.weldingCost, 0.0);
}

// ==============================================================================
// PROCESS COST MODEL TESTS
// ==============================================================================

class ProcessCostModelTest : public ::testing::Test {
protected:
    ProcessCostModel model;
};

TEST_F(ProcessCostModelTest, SetOverheadPercentage) {
    model.setOverheadPercentage(20.0);
    auto result = model.calculateProcessCost(100.0, 50.0, 50.0, 10.0);

    EXPECT_GT(result.overheadAllocation, 0.0);
}

TEST_F(ProcessCostModelTest, SetQualityControlCost) {
    model.setQualityControlCost(10.0);
    auto result = model.calculateProcessCost(100.0, 50.0, 50.0, 5.0);

    EXPECT_EQ(result.qualityControlCost, 50.0);  // 5 units * 10
}

TEST_F(ProcessCostModelTest, SetLogisticsCost) {
    model.setLogisticsCost(5.0, 0.1);
    auto result = model.calculateProcessCost(100.0, 50.0, 50.0, 10.0, 100.0);

    EXPECT_GT(result.logisticsCost, 0.0);
}

TEST_F(ProcessCostModelTest, CalculateProcessCost) {
    auto result = model.calculateProcessCost(200.0, 100.0, 150.0, 5.0);

    EXPECT_GT(result.totalProcessCost, 0.0);
    EXPECT_GT(result.fixedCosts, 0.0);
    EXPECT_GT(result.variableCosts, 0.0);
}

TEST_F(ProcessCostModelTest, CalculateCostPerUnit) {
    double costPerUnit = model.calculateCostPerUnit(500.0, 10.0);
    EXPECT_EQ(costPerUnit, 50.0);
}

TEST_F(ProcessCostModelTest, CostPerUnitZeroQuantity) {
    double costPerUnit = model.calculateCostPerUnit(500.0, 0.0);
    EXPECT_EQ(costPerUnit, 0.0);
}

TEST_F(ProcessCostModelTest, ProcessCostBreakdown) {
    auto result = model.calculateProcessCost(200.0, 100.0, 150.0, 5.0);

    EXPECT_GT(result.costBreakdown.size(), 0);
    EXPECT_NE(result.costBreakdown.find("overhead"), result.costBreakdown.end());
}

// ==============================================================================
// COST OPTIMIZER TESTS
// ==============================================================================

class CostOptimizerTest : public ::testing::Test {
protected:
    CostOptimizer optimizer;
};

TEST_F(CostOptimizerTest, CompareDesignAlternatives) {
    std::vector<CostOptimizer::DesignAlternative> alternatives = {
        {"Alt1", "Material1", "Process1", 500.0, 80.0, 85.0},
        {"Alt2", "Material2", "Process2", 300.0, 70.0, 90.0},
        {"Alt3", "Material3", "Process3", 400.0, 90.0, 80.0}
    };

    auto sorted = optimizer.compareDesignAlternatives(alternatives);

    EXPECT_EQ(sorted.size(), 3);
    EXPECT_EQ(sorted[0].estimatedCost, 300.0);  // Lowest cost first
}

TEST_F(CostOptimizerTest, OptimizeForCost) {
    std::vector<std::string> materials = {"Steel", "Aluminum"};
    std::vector<std::string> processes = {"Standard", "Fast"};

    auto result = optimizer.optimizeForCost(materials, processes, 1000.0);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.selectedMaterial.empty());
    EXPECT_FALSE(result.selectedProcess.empty());
}

TEST_F(CostOptimizerTest, FindBestMaterial) {
    std::vector<std::string> materials = {"Steel", "Aluminum", "Titanium"};
    auto best = optimizer.findBestMaterial(materials, 1000.0);

    EXPECT_FALSE(best.empty());
}

TEST_F(CostOptimizerTest, FindBestSupplier) {
    std::vector<PriceQuote> quotes = {
        {"Supplier_A", 1.00, 100.0, 5.0, "USD", 30, true, 10.0},
        {"Supplier_B", 0.80, 100.0, 7.0, "USD", 30, true, 15.0},
        {"Supplier_C", 0.95, 100.0, 3.0, "USD", 30, true, 5.0}
    };

    auto best = optimizer.findBestSupplier(quotes, 500.0);

    EXPECT_EQ(best, "Supplier_B");  // Lowest price
}

TEST_F(CostOptimizerTest, AnalyzeCostPerformanceTradeoff) {
    std::vector<CostOptimizer::DesignAlternative> alternatives = {
        {"HighPerf", "Material1", "Process1", 2000.0, 95.0, 80.0},
        {"LowCost", "Material2", "Process2", 500.0, 60.0, 95.0},
        {"Balanced", "Material3", "Process3", 1200.0, 80.0, 90.0}
    };

    auto sorted = optimizer.analyzeCostPerformanceTradeoff(alternatives);

    EXPECT_EQ(sorted.size(), 3);
}

// ==============================================================================
// COST BREAKDOWN ANALYZER TESTS
// ==============================================================================

class CostBreakdownAnalyzerTest : public ::testing::Test {
protected:
    CostBreakdownAnalyzer analyzer;
};

TEST_F(CostBreakdownAnalyzerTest, AnalyzeCostBreakdown) {
    auto result = analyzer.analyzeCostBreakdown(
        500.0,   // material
        300.0,   // labor
        200.0,   // machine time
        150.0,   // overhead
        100.0,   // QC
        50.0     // logistics
    );

    EXPECT_GT(result.totalCost, 0.0);
    EXPECT_EQ(result.totalCost, 1300.0);
    EXPECT_GT(result.componentCosts.size(), 0);
}

TEST_F(CostBreakdownAnalyzerTest, IdentifyCostDrivers) {
    auto result = analyzer.analyzeCostBreakdown(1000.0, 300.0, 200.0, 100.0, 50.0, 50.0);
    auto drivers = analyzer.identifyCostDrivers(result, 3);

    EXPECT_LE(drivers.size(), 3);
    if (!drivers.empty()) {
        EXPECT_GT(drivers[0].impact, 0.0);
        EXPECT_LE(drivers[0].impact, 1.0);
    }
}

TEST_F(CostBreakdownAnalyzerTest, CostDriverOrder) {
    auto result = analyzer.analyzeCostBreakdown(1000.0, 100.0, 100.0, 100.0, 50.0, 50.0);
    auto drivers = analyzer.identifyCostDrivers(result, 5);

    // First driver should have highest impact
    if (drivers.size() >= 2) {
        EXPECT_GE(drivers[0].impact, drivers[1].impact);
    }
}

TEST_F(CostBreakdownAnalyzerTest, PerformSensitivityAnalysis) {
    std::vector<std::string> params = {"Material", "Labor"};
    auto results = analyzer.performSensitivityAnalysis(1000.0, params);

    EXPECT_GT(results.size(), 0);
    for (const auto& point : results) {
        EXPECT_FALSE(point.parameterName.empty());
    }
}

TEST_F(CostBreakdownAnalyzerTest, CreateParametricModel) {
    std::map<std::string, double> coeffs;
    coeffs["length"] = 5.0;
    coeffs["weight"] = 10.0;

    auto model = analyzer.createParametricModel("TestModel", 100.0, coeffs);

    EXPECT_EQ(model.name, "TestModel");
    EXPECT_EQ(model.baselineCost, 100.0);
}

TEST_F(CostBreakdownAnalyzerTest, EstimateCostWithModel) {
    std::map<std::string, double> coeffs;
    coeffs["length"] = 2.0;
    coeffs["complexity"] = 5.0;

    auto model = analyzer.createParametricModel("Model1", 100.0, coeffs);

    std::map<std::string, double> values;
    values["length"] = 50.0;
    values["complexity"] = 10.0;

    double cost = analyzer.estimateCostWithModel(model, values);
    double expected = 100.0 + (2.0 * 50.0) + (5.0 * 10.0);  // 100 + 100 + 50 = 250

    EXPECT_EQ(cost, expected);
}

// ==============================================================================
// COST REPORTER TESTS
// ==============================================================================

class CostReporterTest : public ::testing::Test {
protected:
    CostReporter reporter;
};

TEST_F(CostReporterTest, GenerateCostReport) {
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 500.0;
    costs[CostComponentType::LABOR] = 300.0;

    auto report = reporter.generateCostReport("TestPart", costs, "json");

    EXPECT_FALSE(report.title.empty());
    EXPECT_GT(report.totalCost, 0.0);
    EXPECT_FALSE(report.reportContent.empty());
}

TEST_F(CostReporterTest, GenerateJsonReport) {
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 100.0;

    auto report = reporter.generateCostReport("Part1", costs, "json");

    EXPECT_NE(report.reportContent.find("{"), std::string::npos);
}

TEST_F(CostReporterTest, GenerateHtmlReport) {
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 100.0;

    auto report = reporter.generateCostReport("Part1", costs, "html");

    EXPECT_NE(report.reportContent.find("<html>"), std::string::npos);
}

TEST_F(CostReporterTest, GenerateCsvReport) {
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 100.0;

    auto report = reporter.generateCostReport("Part1", costs, "csv");

    EXPECT_NE(report.reportContent.find("Component"), std::string::npos);
}

TEST_F(CostReporterTest, CompareCosts) {
    auto comparison = reporter.compareCosts(1000.0, 1200.0);

    EXPECT_EQ(comparison.baselineCost, 1000.0);
    EXPECT_EQ(comparison.comparisonCost, 1200.0);
    EXPECT_EQ(comparison.difference, 200.0);
}

TEST_F(CostReporterTest, CostComparisonPercentage) {
    auto comparison = reporter.compareCosts(1000.0, 1100.0);

    EXPECT_EQ(comparison.percentageChange, 10.0);
}

TEST_F(CostReporterTest, GenerateCostBreakdownChart) {
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 500.0;
    costs[CostComponentType::LABOR] = 300.0;

    auto chart = reporter.generateCostBreakdownChart(costs);

    EXPECT_FALSE(chart.empty());
}

TEST_F(CostReporterTest, GenerateCostTrendAnalysis) {
    std::vector<std::pair<int, double>> history;
    history.push_back({1, 1000.0});
    history.push_back({2, 950.0});
    history.push_back({3, 900.0});

    auto analysis = reporter.generateCostTrendAnalysis(history);

    EXPECT_NE(analysis.find("Trend"), std::string::npos);
}

TEST_F(CostReporterTest, GenerateCostForecast) {
    std::vector<std::pair<int, double>> history;
    for (int i = 1; i <= 12; i++) {
        history.push_back({i, 1000.0 - (i * 10.0)});
    }

    auto forecast = reporter.generateCostForecast(history, 12);

    EXPECT_NE(forecast.find("Forecast"), std::string::npos);
}

TEST_F(CostReporterTest, TrackBudget) {
    auto status = reporter.trackBudget(5000.0, 3500.0);

    EXPECT_EQ(status.allocatedBudget, 5000.0);
    EXPECT_EQ(status.currentSpend, 3500.0);
    EXPECT_EQ(status.remainingBudget, 1500.0);
    EXPECT_TRUE(status.isOnBudget);
}

TEST_F(CostReporterTest, BudgetOverrun) {
    auto status = reporter.trackBudget(2000.0, 2500.0);

    EXPECT_FALSE(status.isOnBudget);
    EXPECT_LT(status.remainingBudget, 0.0);
}

// ==============================================================================
// INTEGRATED COST ANALYSIS SYSTEM TESTS
// ==============================================================================

class CostAnalysisSystemTest : public ::testing::Test {
protected:
    CostAnalysisSystem system;

    void SetUp() override {
        system.initialize();
    }
};

TEST_F(CostAnalysisSystemTest, SystemInitialization) {
    auto materials = system.getMaterialCalculator().getAvailableMaterials();
    EXPECT_GE(materials.size(), 3);
}

TEST_F(CostAnalysisSystemTest, PerformCompleteAnalysis) {
    auto analysis = system.performCompleteAnalysis(
        "TestTube",
        "Steel",
        100.0,   // length
        12.0,    // OD
        1.0,     // WT
        5,       // bends
        2,       // cuts
        1,       // welds
        10.0     // quantity
    );

    EXPECT_GT(analysis.totalCost, 0.0);
    EXPECT_GT(analysis.materialCost, 0.0);
    EXPECT_GT(analysis.manufacturingCost, 0.0);
}

TEST_F(CostAnalysisSystemTest, CompleteAnalysisBreakdown) {
    auto analysis = system.performCompleteAnalysis(
        "TestTube",
        "Aluminum",
        200.0,
        15.0,
        1.5,
        3,
        1,
        0,
        5.0
    );

    EXPECT_GT(analysis.costBreakdown.size(), 0);
    EXPECT_GT(analysis.costDrivers.size(), 0);
}

TEST_F(CostAnalysisSystemTest, CompleteAnalysisOptimization) {
    auto analysis = system.performCompleteAnalysis(
        "Part1",
        "Steel",
        150.0,
        20.0,
        2.0,
        4,
        2,
        1,
        8.0
    );

    EXPECT_TRUE(analysis.optimization.success);
    EXPECT_FALSE(analysis.optimization.selectedMaterial.empty());
}

TEST_F(CostAnalysisSystemTest, GetMaterialCalculator) {
    auto& calc = system.getMaterialCalculator();
    auto materials = calc.getAvailableMaterials();
    EXPECT_GT(materials.size(), 0);
}

TEST_F(CostAnalysisSystemTest, GetManufacturingCalculator) {
    auto& calc = system.getManufacturingCalculator();
    auto result = calc.calculateManufacturingCost(5, 2, 1, 0, 100.0, 2.0);
    EXPECT_GT(result.totalManufacturingCost, 0.0);
}

TEST_F(CostAnalysisSystemTest, GetProcessCostModel) {
    auto& model = system.getProcessCostModel();
    auto result = model.calculateProcessCost(100.0, 50.0, 50.0, 5.0);
    EXPECT_GT(result.totalProcessCost, 0.0);
}

TEST_F(CostAnalysisSystemTest, GetOptimizer) {
    auto& opt = system.getOptimizer();
    std::vector<std::string> mats = {"Steel"};
    std::vector<std::string> procs = {"Standard"};
    auto result = opt.optimizeForCost(mats, procs, 500.0);
    EXPECT_TRUE(result.success);
}

TEST_F(CostAnalysisSystemTest, GetBreakdownAnalyzer) {
    auto& analyzer = system.getBreakdownAnalyzer();
    auto result = analyzer.analyzeCostBreakdown(100.0, 50.0, 50.0, 20.0, 10.0, 10.0);
    EXPECT_GT(result.totalCost, 0.0);
}

TEST_F(CostAnalysisSystemTest, GetReporter) {
    auto& rep = system.getReporter();
    std::map<CostComponentType, double> costs;
    costs[CostComponentType::MATERIAL] = 100.0;
    auto report = rep.generateCostReport("Part1", costs);
    EXPECT_FALSE(report.title.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
