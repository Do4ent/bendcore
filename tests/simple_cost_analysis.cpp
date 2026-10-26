#include "bendcore/cost_analysis.hpp"
#include <iostream>
#include <cassert>

using namespace bendcore::cost_analysis;

int main() {
    std::cout << "Cost Analysis System Test\n";
    std::cout << "======================\n\n";

    // Test Material Cost Calculator
    {
        std::cout << "1. Testing Material Cost Calculator...\n";
        MaterialCostCalculator calc;

        MaterialSpecification steel;
        steel.type = MaterialType::CARBON_STEEL;
        steel.materialName = "Steel";
        steel.density = 7.85;
        steel.pricePerKg = 0.80;
        steel.scrapFactor = 0.05;
        steel.supplier = "Supplier_A";

        calc.registerMaterial(steel);

        auto result = calc.calculateMaterialCost("Steel", 100.0, 12.0, 1.0);
        assert(result.totalMaterialCost > 0.0);
        assert(result.rawMaterialCost > 0.0);
        assert(result.scrapCost > 0.0);

        std::cout << "   Material Cost: $" << result.totalMaterialCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Manufacturing Cost Calculator
    {
        std::cout << "2. Testing Manufacturing Cost Calculator...\n";
        ManufacturingCostCalculator calc;

        auto result = calc.calculateManufacturingCost(5, 3, 2, 1, 180.0, 3.0);
        assert(result.totalManufacturingCost > 0.0);
        assert(result.bendingCost > 0.0);
        assert(result.weldingCost > 0.0);

        std::cout << "   Total Manufacturing Cost: $" << result.totalManufacturingCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Process Cost Model
    {
        std::cout << "3. Testing Process Cost Model...\n";
        ProcessCostModel model;

        model.setOverheadPercentage(15.0);
        model.setQualityControlCost(5.0);

        auto result = model.calculateProcessCost(500.0, 300.0, 200.0, 10.0);
        assert(result.totalProcessCost > 0.0);
        assert(result.overheadAllocation > 0.0);
        assert(result.qualityControlCost > 0.0);

        std::cout << "   Total Process Cost: $" << result.totalProcessCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Cost Optimizer
    {
        std::cout << "4. Testing Cost Optimizer...\n";
        CostOptimizer optimizer;

        std::vector<std::string> materials = {"Steel", "Aluminum"};
        std::vector<std::string> processes = {"Standard", "Fast"};

        auto result = optimizer.optimizeForCost(materials, processes, 1000.0);
        assert(result.success);
        assert(!result.selectedMaterial.empty());
        assert(!result.selectedProcess.empty());

        std::cout << "   Selected Material: " << result.selectedMaterial << "\n";
        std::cout << "   Estimated Cost: $" << result.estimatedCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Cost Breakdown Analyzer
    {
        std::cout << "5. Testing Cost Breakdown Analyzer...\n";
        CostBreakdownAnalyzer analyzer;

        auto result = analyzer.analyzeCostBreakdown(500.0, 300.0, 200.0, 150.0, 100.0, 50.0);
        assert(result.totalCost > 0.0);
        assert(result.componentCosts.size() > 0);
        assert(result.topCostDrivers.size() > 0);

        std::cout << "   Total Cost: $" << result.totalCost << "\n";
        std::cout << "   Top Cost Drivers: " << result.topCostDrivers.size() << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Cost Reporter
    {
        std::cout << "6. Testing Cost Reporter...\n";
        CostReporter reporter;

        std::map<CostComponentType, double> costs;
        costs[CostComponentType::MATERIAL] = 500.0;
        costs[CostComponentType::LABOR] = 300.0;
        costs[CostComponentType::MACHINE_TIME] = 200.0;

        auto report = reporter.generateCostReport("TestPart", costs, "json");
        assert(!report.title.empty());
        assert(report.totalCost > 0.0);
        assert(!report.reportContent.empty());

        std::cout << "   Report Title: " << report.title << "\n";
        std::cout << "   Total Cost: $" << report.totalCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    // Test Integrated System
    {
        std::cout << "7. Testing Integrated Cost Analysis System...\n";
        CostAnalysisSystem system;
        system.initialize();

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

        assert(analysis.totalCost > 0.0);
        assert(analysis.materialCost > 0.0);
        assert(analysis.manufacturingCost > 0.0);
        assert(analysis.costBreakdown.size() > 0);

        std::cout << "   Total Cost: $" << analysis.totalCost << "\n";
        std::cout << "   Material Cost: $" << analysis.materialCost << "\n";
        std::cout << "   Manufacturing Cost: $" << analysis.manufacturingCost << "\n";
        std::cout << "   PASSED\n\n";
    }

    std::cout << "======================\n";
    std::cout << "All tests PASSED!\n";

    return 0;
}
