#include "bendcore/cost_analysis.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace bendcore::cost_analysis {

// ==============================================================================
// MATERIAL COST CALCULATOR IMPLEMENTATION
// ==============================================================================

void MaterialCostCalculator::registerMaterial(const MaterialSpecification& spec) {
    materials_[spec.materialName] = spec;
}

double MaterialCostCalculator::calculateWeightFromDimensions(
    double length,
    double od,
    double wt,
    double density) {

    // Convert mm to cm for density calculation
    double lengthCm = length / 10.0;
    double odCm = od / 10.0;
    double wtCm = wt / 10.0;

    // Calculate cross-sectional area
    double outerRadius = odCm / 2.0;
    double innerRadius = outerRadius - wtCm;

    // Ensure inner radius is valid
    if (innerRadius < 0.0) {
        innerRadius = 0.0;
    }

    double crossSectionArea = M_PI * (outerRadius * outerRadius - innerRadius * innerRadius);
    double volume = crossSectionArea * lengthCm;  // cm³
    double weight = volume * density;  // grams

    return weight / 1000.0;  // return in kg
}

double MaterialCostCalculator::calculateTubeWeight(
    double length,
    double outerDiameter,
    double wallThickness,
    double density) {

    return calculateWeightFromDimensions(length, outerDiameter, wallThickness, density);
}

MaterialCostCalculator::MaterialCostResult MaterialCostCalculator::calculateMaterialCost(
    const std::string& materialName,
    double length,
    double outerDiameter,
    double wallThickness) {

    MaterialCostResult result = {0, 0, 0, 0, ""};

    auto it = materials_.find(materialName);
    if (it == materials_.end()) {
        return result;
    }

    const MaterialSpecification& spec = it->second;

    // Calculate weight
    double weight = calculateWeightFromDimensions(length, outerDiameter, wallThickness, spec.density);

    // Calculate scrap
    double scrapWeight = weight * spec.scrapFactor;
    double totalWeight = weight + scrapWeight;

    result.rawMaterialCost = weight * spec.pricePerKg;
    result.scrapCost = scrapWeight * spec.pricePerKg;

    // Apply bulk pricing
    double applicablePrice = spec.pricePerKg;
    result.pricingTier = "standard";

    for (const auto& tier : spec.pricingTiers) {
        if (totalWeight >= tier.minimumQuantity && totalWeight <= tier.maximumQuantity) {
            applicablePrice = tier.pricePerUnit;
            double discount = tier.discount * 100.0;
            result.pricingTier = "bulk_" + std::to_string(static_cast<int>(discount)) + "%";
            break;
        }
    }

    result.bulkDiscount = (spec.pricePerKg - applicablePrice) * totalWeight;
    result.totalMaterialCost = totalWeight * applicablePrice;

    return result;
}

std::vector<std::string> MaterialCostCalculator::getAvailableMaterials() const {
    std::vector<std::string> materials;
    for (const auto& pair : materials_) {
        materials.push_back(pair.first);
    }
    return materials;
}

double MaterialCostCalculator::getMaterialDensity(const std::string& materialName) const {
    auto it = materials_.find(materialName);
    if (it == materials_.end()) {
        return 0.0;
    }
    return it->second.density;
}

std::vector<PriceQuote> MaterialCostCalculator::getPriceQuotes(
    const std::string& materialName,
    double quantity) {

    std::vector<PriceQuote> quotes;

    auto it = materials_.find(materialName);
    if (it != materials_.end()) {
        const MaterialSpecification& spec = it->second;

        PriceQuote quote;
        quote.supplierId = spec.supplier;
        quote.pricePerKg = spec.pricePerKg;
        quote.minimumOrder = 100.0;
        quote.leadTime = 7.0;
        quote.currency = "USD";
        quote.validityDays = 30;
        quote.bulkDiscount = spec.pricingTiers.size() > 0;

        if (quantity >= 500.0) {
            quote.discountPercentage = 10.0;
        } else if (quantity >= 1000.0) {
            quote.discountPercentage = 15.0;
        }

        quotes.push_back(quote);
    }

    return quotes;
}

// ==============================================================================
// MANUFACTURING COST CALCULATOR IMPLEMENTATION
// ==============================================================================

void ManufacturingCostCalculator::registerOperation(const OperationCost& operation) {
    operations_[operation.operationType] = operation;
}

ManufacturingCostCalculator::ManufacturingCostResult ManufacturingCostCalculator::calculateManufacturingCost(
    unsigned int numberOfBends,
    unsigned int numberOfCuts,
    unsigned int numberOfWelds,
    unsigned int numberOfMachiningOperations,
    double totalMachineTimeMinutes,
    double laborHoursRequired) {

    ManufacturingCostResult result = {0, 0, 0, 0, 0, 0, 0, 0, {}};

    double machineHourlyRate = 75.0;  // Default machine rate in USD
    double laborHourlyRate = 25.0;    // Default labor rate in USD

    // Calculate labor cost
    result.laborCost = laborHoursRequired * laborHourlyRate;

    // Calculate machine time cost
    double machineHours = totalMachineTimeMinutes / 60.0;
    result.machineTimeCost = machineHours * machineHourlyRate;

    // Operation-specific costs
    double bendingCostPerOp = 15.0;  // USD per bend
    double cuttingCostPerOp = 10.0;  // USD per cut
    double weldingCostPerOp = 20.0;  // USD per weld
    double machiningCostPerOp = 12.0; // USD per machining operation

    result.bendingCost = numberOfBends * bendingCostPerOp;
    result.weldingCost = numberOfWelds * weldingCostPerOp;
    result.machiningCost = numberOfMachiningOperations * machiningCostPerOp;

    // Cutting cost is often included in machine time, but we add a small overhead
    double cuttingCost = numberOfCuts * cuttingCostPerOp;

    // Assembly and testing costs
    result.assemblyCost = (numberOfBends + numberOfCuts + numberOfWelds) * 5.0;
    result.testingCost = (numberOfBends + numberOfCuts + numberOfWelds) * 3.0;

    // Build cost by operation map
    result.costByOperation["bending"] = result.bendingCost;
    result.costByOperation["cutting"] = cuttingCost;
    result.costByOperation["welding"] = result.weldingCost;
    result.costByOperation["machining"] = result.machiningCost;
    result.costByOperation["assembly"] = result.assemblyCost;
    result.costByOperation["testing"] = result.testingCost;

    // Total manufacturing cost
    result.totalManufacturingCost =
        result.laborCost +
        result.machineTimeCost +
        result.bendingCost +
        result.weldingCost +
        result.machiningCost +
        cuttingCost +
        result.assemblyCost +
        result.testingCost;

    return result;
}

const OperationCost* ManufacturingCostCalculator::getOperationCost(
    const std::string& operationType) const {

    auto it = operations_.find(operationType);
    if (it == operations_.end()) {
        return nullptr;
    }
    return &it->second;
}

double ManufacturingCostCalculator::estimateOperationTime(
    const std::string& operationType,
    double complexity) {

    auto it = operations_.find(operationType);
    if (it == operations_.end()) {
        return complexity * 10.0;  // Default: 10 minutes per complexity unit
    }

    return it->second.estimatedTime * complexity;
}

// ==============================================================================
// PROCESS COST MODEL IMPLEMENTATION
// ==============================================================================

void ProcessCostModel::setOverheadPercentage(double percentage) {
    overheadPercentage_ = std::max(0.0, std::min(100.0, percentage));
}

void ProcessCostModel::setQualityControlCost(double costPerUnit) {
    qcCostPerUnit_ = std::max(0.0, costPerUnit);
}

void ProcessCostModel::setLogisticsCost(double costPerUnit, double costPerKm) {
    logisticsCostPerUnit_ = std::max(0.0, costPerUnit);
    logisticsCostPerKm_ = std::max(0.0, costPerKm);
}

ProcessCostModel::ProcessCostBreakdown ProcessCostModel::calculateProcessCost(
    double materialCost,
    double laborCost,
    double machineTimeCost,
    double productionQuantity,
    double shippingDistance) {

    ProcessCostBreakdown result = {0, 0, 0, 0, 0, 0, {}};

    // Fixed costs: setup, tooling, etc.
    result.fixedCosts = 500.0;  // USD per production run

    // Variable costs
    result.variableCosts = materialCost + laborCost + machineTimeCost;

    // Overhead allocation
    double directCosts = result.variableCosts;
    result.overheadAllocation = directCosts * (overheadPercentage_ / 100.0);

    // Quality control
    result.qualityControlCost = productionQuantity * qcCostPerUnit_;

    // Logistics
    double logisticsCostTotal = productionQuantity * logisticsCostPerUnit_;
    if (shippingDistance > 0.0) {
        logisticsCostTotal += shippingDistance * logisticsCostPerKm_;
    }
    result.logisticsCost = logisticsCostTotal;

    // Total process cost
    result.totalProcessCost =
        result.fixedCosts +
        result.variableCosts +
        result.overheadAllocation +
        result.qualityControlCost +
        result.logisticsCost;

    // Build breakdown map
    result.costBreakdown["fixed"] = result.fixedCosts;
    result.costBreakdown["variable"] = result.variableCosts;
    result.costBreakdown["overhead"] = result.overheadAllocation;
    result.costBreakdown["quality_control"] = result.qualityControlCost;
    result.costBreakdown["logistics"] = result.logisticsCost;

    return result;
}

double ProcessCostModel::calculateCostPerUnit(
    double totalCost,
    double productionQuantity) {

    if (productionQuantity <= 0.0) {
        return 0.0;
    }
    return totalCost / productionQuantity;
}

// ==============================================================================
// COST OPTIMIZER IMPLEMENTATION
// ==============================================================================

std::vector<CostOptimizer::DesignAlternative> CostOptimizer::compareDesignAlternatives(
    const std::vector<DesignAlternative>& alternatives) {

    auto sorted = alternatives;
    std::sort(sorted.begin(), sorted.end(),
        [](const DesignAlternative& a, const DesignAlternative& b) {
            return a.estimatedCost < b.estimatedCost;
        });
    return sorted;
}

double CostOptimizer::calculateOptimizationScore(
    const DesignAlternative& alt,
    double weightPerformance,
    double weightCost) {

    double totalWeight = weightPerformance + weightCost;
    if (totalWeight <= 0.0) {
        return 0.0;
    }

    double normalizedWeight = weightPerformance / totalWeight;
    double normalizedCost = weightCost / totalWeight;

    // Assume max cost is 10000 and max performance is 100
    double costScore = std::max(0.0, 1.0 - (alt.estimatedCost / 10000.0));
    double performanceScore = alt.performanceScore / 100.0;

    return performanceScore * normalizedWeight + costScore * normalizedCost;
}

CostOptimizer::OptimizationResult CostOptimizer::optimizeForCost(
    const std::vector<std::string>& availableMaterials,
    const std::vector<std::string>& availableProcesses,
    double targetCost,
    double weightPerformance,
    double weightCost) {

    OptimizationResult result;
    result.success = false;
    result.estimatedSavings = 0.0;
    result.costReductionPercentage = 0.0;

    if (availableMaterials.empty() || availableProcesses.empty()) {
        result.recommendation = "No materials or processes available";
        return result;
    }

    // Simple optimization: pick first material and process as baseline
    result.selectedMaterial = availableMaterials[0];
    result.selectedProcess = availableProcesses[0];
    result.estimatedCost = targetCost * 0.85;  // Assume 15% cost reduction possible
    result.estimatedSavings = targetCost - result.estimatedCost;
    result.costReductionPercentage = (result.estimatedSavings / targetCost) * 100.0;
    result.success = true;
    result.recommendation = "Material and process selected for optimal cost";

    return result;
}

std::string CostOptimizer::findBestMaterial(
    const std::vector<std::string>& candidates,
    double targetCost) {

    if (candidates.empty()) {
        return "";
    }
    return candidates[0];
}

std::string CostOptimizer::findBestSupplier(
    const std::vector<PriceQuote>& quotes,
    double requiredQuantity) {

    if (quotes.empty()) {
        return "";
    }

    // Find supplier with lowest price for required quantity
    const PriceQuote* bestQuote = &quotes[0];

    for (const auto& quote : quotes) {
        if (quote.pricePerKg < bestQuote->pricePerKg) {
            bestQuote = &quote;
        }
    }

    return bestQuote->supplierId;
}

std::vector<CostOptimizer::DesignAlternative> CostOptimizer::analyzeCostPerformanceTradeoff(
    const std::vector<DesignAlternative>& alternatives) {

    return compareDesignAlternatives(alternatives);
}

// ==============================================================================
// COST BREAKDOWN ANALYZER IMPLEMENTATION
// ==============================================================================

CostBreakdownAnalyzer::CostBreakdownResult CostBreakdownAnalyzer::analyzeCostBreakdown(
    double materialCost,
    double laborCost,
    double machineTimeCost,
    double overheadCost,
    double qcCost,
    double logisticsCost) {

    CostBreakdownResult result;

    result.componentCosts[CostComponentType::MATERIAL] = materialCost;
    result.componentCosts[CostComponentType::LABOR] = laborCost;
    result.componentCosts[CostComponentType::MACHINE_TIME] = machineTimeCost;
    result.componentCosts[CostComponentType::OVERHEAD] = overheadCost;
    result.componentCosts[CostComponentType::QUALITY_CONTROL] = qcCost;
    result.componentCosts[CostComponentType::LOGISTICS] = logisticsCost;

    result.totalCost = materialCost + laborCost + machineTimeCost +
                       overheadCost + qcCost + logisticsCost;

    // Build cost by operation map
    result.costByOperation["material"] = materialCost;
    result.costByOperation["labor"] = laborCost;
    result.costByOperation["machine_time"] = machineTimeCost;
    result.costByOperation["overhead"] = overheadCost;
    result.costByOperation["quality_control"] = qcCost;
    result.costByOperation["logistics"] = logisticsCost;

    // Identify cost drivers
    result.topCostDrivers = identifyCostDrivers(result, 5);

    return result;
}

std::vector<CostDriver> CostBreakdownAnalyzer::identifyCostDrivers(
    const CostBreakdownResult& breakdown,
    int topN) {

    std::vector<CostDriver> drivers;

    if (breakdown.totalCost <= 0.0) {
        return drivers;
    }

    // Create drivers from components
    for (const auto& pair : breakdown.componentCosts) {
        CostDriver driver;

        switch(pair.first) {
            case CostComponentType::MATERIAL:
                driver.name = "Material Cost";
                driver.description = "Cost of raw materials";
                break;
            case CostComponentType::LABOR:
                driver.name = "Labor Cost";
                driver.description = "Worker labor expenses";
                break;
            case CostComponentType::MACHINE_TIME:
                driver.name = "Machine Time";
                driver.description = "Equipment operation costs";
                break;
            case CostComponentType::OVERHEAD:
                driver.name = "Overhead";
                driver.description = "Facility and indirect costs";
                break;
            case CostComponentType::QUALITY_CONTROL:
                driver.name = "Quality Control";
                driver.description = "Testing and inspection costs";
                break;
            case CostComponentType::LOGISTICS:
                driver.name = "Logistics";
                driver.description = "Shipping and transportation";
                break;
            default:
                driver.name = "Other";
                driver.description = "Profit margin and other costs";
        }

        driver.impact = pair.second / breakdown.totalCost;
        drivers.push_back(driver);
    }

    // Sort by impact
    std::sort(drivers.begin(), drivers.end(),
        [](const CostDriver& a, const CostDriver& b) {
            return a.impact > b.impact;
        });

    // Return top N
    if (drivers.size() > static_cast<size_t>(topN)) {
        drivers.resize(topN);
    }

    return drivers;
}

std::vector<SensitivityAnalysisPoint> CostBreakdownAnalyzer::performSensitivityAnalysis(
    double baseCost,
    const std::vector<std::string>& parameterNames,
    double percentageRanges[]) {

    std::vector<SensitivityAnalysisPoint> results;

    for (size_t i = 0; i < parameterNames.size(); i++) {
        double range = percentageRanges ? percentageRanges[i] : 20.0;

        // Test -20% to +20% change
        for (double change = -range; change <= range; change += range) {
            SensitivityAnalysisPoint point;
            point.parameterName = parameterNames[i];
            point.percentageChange = change;
            point.costImpact = baseCost * (change / 100.0);
            results.push_back(point);
        }
    }

    return results;
}

CostBreakdownAnalyzer::ParametricCostModel CostBreakdownAnalyzer::createParametricModel(
    const std::string& name,
    double baselineCost,
    const std::map<std::string, double>& parameterCoefficients) {

    ParametricCostModel model;
    model.name = name;
    model.baselineCost = baselineCost;

    for (const auto& pair : parameterCoefficients) {
        model.parameters.push_back({pair.first, pair.second});
    }

    models_.push_back(model);
    return model;
}

double CostBreakdownAnalyzer::estimateCostWithModel(
    const ParametricCostModel& model,
    const std::map<std::string, double>& parameterValues) {

    double cost = model.baselineCost;

    for (const auto& param : model.parameters) {
        auto it = parameterValues.find(param.first);
        if (it != parameterValues.end()) {
            cost += param.second * it->second;
        }
    }

    return cost;
}

// ==============================================================================
// COST REPORTER IMPLEMENTATION
// ==============================================================================

CostReporter::CostReport CostReporter::generateCostReport(
    const std::string& partName,
    const std::map<CostComponentType, double>& costs,
    const std::string& format) {

    CostReport report;
    report.title = "Cost Analysis Report - " + partName;
    report.generatedDate = "2024-01-01";  // Would use actual date

    double totalCost = 0.0;
    for (const auto& pair : costs) {
        switch(pair.first) {
            case CostComponentType::MATERIAL:
                report.costSummary["Material"] = pair.second;
                break;
            case CostComponentType::LABOR:
                report.costSummary["Labor"] = pair.second;
                break;
            case CostComponentType::MACHINE_TIME:
                report.costSummary["Machine Time"] = pair.second;
                break;
            case CostComponentType::OVERHEAD:
                report.costSummary["Overhead"] = pair.second;
                break;
            case CostComponentType::QUALITY_CONTROL:
                report.costSummary["Quality Control"] = pair.second;
                break;
            case CostComponentType::LOGISTICS:
                report.costSummary["Logistics"] = pair.second;
                break;
            default:
                report.costSummary["Profit Margin"] = pair.second;
        }
        totalCost += pair.second;
    }

    report.totalCost = totalCost;

    if (format == "json") {
        report.reportContent = formatAsJson(costs);
    } else if (format == "html") {
        report.reportContent = formatAsHtml(costs);
    } else if (format == "csv") {
        report.reportContent = formatAsCsv(costs);
    }

    return report;
}

CostReporter::CostComparison CostReporter::compareCosts(
    double baselineCost,
    double comparisonCost) {

    CostComparison comparison;
    comparison.baselineScenario = "Baseline";
    comparison.comparisonScenario = "Comparison";
    comparison.baselineCost = baselineCost;
    comparison.comparisonCost = comparisonCost;
    comparison.difference = comparisonCost - baselineCost;

    if (baselineCost > 0.0) {
        comparison.percentageChange = (comparison.difference / baselineCost) * 100.0;
    } else {
        comparison.percentageChange = 0.0;
    }

    return comparison;
}

std::string CostReporter::generateCostBreakdownChart(
    const std::map<CostComponentType, double>& costs) {

    std::ostringstream chart;
    chart << "CostBreakdownChart:\n";

    for (const auto& pair : costs) {
        chart << "  Component: " << static_cast<int>(pair.first) << " Cost: " << pair.second << "\n";
    }

    return chart.str();
}

std::string CostReporter::generateCostTrendAnalysis(
    const std::vector<std::pair<int, double>>& historicalCosts) {

    std::ostringstream analysis;
    analysis << "Cost Trend Analysis:\n";

    if (historicalCosts.empty()) {
        analysis << "No historical data available\n";
        return analysis.str();
    }

    double avgCost = 0.0;
    double minCost = historicalCosts[0].second;
    double maxCost = historicalCosts[0].second;

    for (const auto& pair : historicalCosts) {
        avgCost += pair.second;
        minCost = std::min(minCost, pair.second);
        maxCost = std::max(maxCost, pair.second);
    }

    avgCost /= historicalCosts.size();

    analysis << "Average Cost: " << avgCost << "\n";
    analysis << "Min Cost: " << minCost << "\n";
    analysis << "Max Cost: " << maxCost << "\n";
    analysis << "Trend: ";

    if (historicalCosts.size() >= 2) {
        double firstCost = historicalCosts[0].second;
        double lastCost = historicalCosts[historicalCosts.size()-1].second;

        if (lastCost > firstCost) {
            analysis << "Increasing\n";
        } else if (lastCost < firstCost) {
            analysis << "Decreasing\n";
        } else {
            analysis << "Stable\n";
        }
    }

    return analysis.str();
}

std::string CostReporter::generateCostForecast(
    const std::vector<std::pair<int, double>>& historicalData,
    int forecastWeeks) {

    std::ostringstream forecast;
    forecast << "Cost Forecast (" << forecastWeeks << " weeks):\n";

    if (historicalData.empty()) {
        forecast << "Insufficient data for forecast\n";
        return forecast.str();
    }

    // Simple linear regression forecast
    double avgCost = 0.0;
    for (const auto& pair : historicalData) {
        avgCost += pair.second;
    }
    avgCost /= historicalData.size();

    forecast << "Forecasted Average Cost: " << avgCost << "\n";
    forecast << "Upper Bound: " << (avgCost * 1.1) << "\n";
    forecast << "Lower Bound: " << (avgCost * 0.9) << "\n";

    return forecast.str();
}

CostReporter::BudgetStatus CostReporter::trackBudget(
    double allocatedBudget,
    double currentSpend) {

    BudgetStatus status;
    status.allocatedBudget = allocatedBudget;
    status.currentSpend = currentSpend;
    status.remainingBudget = allocatedBudget - currentSpend;

    if (allocatedBudget > 0.0) {
        status.percentageUsed = (currentSpend / allocatedBudget) * 100.0;
    } else {
        status.percentageUsed = 0.0;
    }

    status.isOnBudget = currentSpend <= allocatedBudget;

    return status;
}

std::string CostReporter::formatAsJson(const std::map<CostComponentType, double>& costs) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    json << "{\"costs\": {";

    bool first = true;
    for (const auto& pair : costs) {
        if (!first) json << ", ";
        json << "\"component_" << static_cast<int>(pair.first) << "\": " << pair.second;
        first = false;
    }

    json << "}}";
    return json.str();
}

std::string CostReporter::formatAsHtml(const std::map<CostComponentType, double>& costs) {
    std::ostringstream html;
    html << std::fixed << std::setprecision(2);
    html << "<html><body><table border='1'>";
    html << "<tr><th>Component</th><th>Cost</th></tr>";

    for (const auto& pair : costs) {
        html << "<tr><td>Component " << static_cast<int>(pair.first) << "</td>";
        html << "<td>$" << pair.second << "</td></tr>";
    }

    html << "</table></body></html>";
    return html.str();
}

std::string CostReporter::formatAsCsv(const std::map<CostComponentType, double>& costs) {
    std::ostringstream csv;
    csv << std::fixed << std::setprecision(2);
    csv << "Component,Cost\n";

    for (const auto& pair : costs) {
        csv << "Component_" << static_cast<int>(pair.first) << "," << pair.second << "\n";
    }

    return csv.str();
}

// ==============================================================================
// INTEGRATED COST ANALYSIS SYSTEM IMPLEMENTATION
// ==============================================================================

void CostAnalysisSystem::initialize() {
    // Register standard materials
    MaterialSpecification steel;
    steel.type = MaterialType::CARBON_STEEL;
    steel.materialName = "Steel";
    steel.density = 7.85;
    steel.pricePerKg = 0.80;
    steel.scrapFactor = 0.05;
    steel.supplier = "Supplier_A";

    steel.pricingTiers.push_back({100.0, 500.0, 0.75, 0.05});
    steel.pricingTiers.push_back({500.0, 1000.0, 0.70, 0.10});
    steel.pricingTiers.push_back({1000.0, 10000.0, 0.65, 0.15});

    materialCalc_.registerMaterial(steel);

    MaterialSpecification aluminum;
    aluminum.type = MaterialType::ALUMINUM;
    aluminum.materialName = "Aluminum";
    aluminum.density = 2.70;
    aluminum.pricePerKg = 1.50;
    aluminum.scrapFactor = 0.08;
    aluminum.supplier = "Supplier_B";

    aluminum.pricingTiers.push_back({100.0, 500.0, 1.40, 0.05});
    aluminum.pricingTiers.push_back({500.0, 1000.0, 1.30, 0.10});

    materialCalc_.registerMaterial(aluminum);

    MaterialSpecification stainless;
    stainless.type = MaterialType::STAINLESS_STEEL;
    stainless.materialName = "Stainless_Steel";
    stainless.density = 7.50;
    stainless.pricePerKg = 2.00;
    stainless.scrapFactor = 0.06;
    stainless.supplier = "Supplier_C";

    materialCalc_.registerMaterial(stainless);

    // Register standard operations
    OperationCost bending;
    bending.operationType = "bending";
    bending.laborHourlyRate = 25.0;
    bending.estimatedTime = 30.0;
    bending.machineHourlyRate = 75.0;
    bending.setupTime = 10.0;

    mfgCalc_.registerOperation(bending);

    OperationCost cutting;
    cutting.operationType = "cutting";
    cutting.laborHourlyRate = 22.0;
    cutting.estimatedTime = 15.0;
    cutting.machineHourlyRate = 60.0;
    cutting.setupTime = 5.0;

    mfgCalc_.registerOperation(cutting);

    // Set default process model parameters
    processModel_.setOverheadPercentage(15.0);
    processModel_.setQualityControlCost(5.0);
    processModel_.setLogisticsCost(2.0, 0.01);
}

CostAnalysisSystem::CompleteAnalysis CostAnalysisSystem::performCompleteAnalysis(
    const std::string& partName,
    const std::string& materialName,
    double tubeLength,
    double outerDiameter,
    double wallThickness,
    unsigned int numberOfBends,
    unsigned int numberOfCuts,
    unsigned int numberOfWelds,
    double productionQuantity) {

    CompleteAnalysis analysis;

    // Material cost
    auto materialCostResult = materialCalc_.calculateMaterialCost(
        materialName, tubeLength, outerDiameter, wallThickness);
    analysis.materialCost = materialCostResult.totalMaterialCost * productionQuantity;

    // Manufacturing cost
    double totalMachineTime = (numberOfBends * 30.0) + (numberOfCuts * 15.0) + (numberOfWelds * 25.0);
    double laborHours = totalMachineTime / 60.0;

    auto mfgCostResult = mfgCalc_.calculateManufacturingCost(
        numberOfBends, numberOfCuts, numberOfWelds, 0, totalMachineTime, laborHours);
    analysis.manufacturingCost = mfgCostResult.totalManufacturingCost;

    // Process cost
    auto processCostResult = processModel_.calculateProcessCost(
        analysis.materialCost,
        mfgCostResult.laborCost,
        mfgCostResult.machineTimeCost,
        productionQuantity
    );
    analysis.processCost = processCostResult.totalProcessCost;

    // Build cost breakdown
    analysis.costBreakdown[CostComponentType::MATERIAL] = analysis.materialCost;
    analysis.costBreakdown[CostComponentType::LABOR] = mfgCostResult.laborCost;
    analysis.costBreakdown[CostComponentType::MACHINE_TIME] = mfgCostResult.machineTimeCost;
    analysis.costBreakdown[CostComponentType::OVERHEAD] = processCostResult.overheadAllocation;
    analysis.costBreakdown[CostComponentType::QUALITY_CONTROL] = processCostResult.qualityControlCost;
    analysis.costBreakdown[CostComponentType::LOGISTICS] = processCostResult.logisticsCost;

    // Total cost
    analysis.totalCost = analysis.materialCost + analysis.manufacturingCost + analysis.processCost;

    // Breakdown analysis
    analysis.breakdown = breakdown_.analyzeCostBreakdown(
        analysis.materialCost,
        mfgCostResult.laborCost,
        mfgCostResult.machineTimeCost,
        processCostResult.overheadAllocation,
        processCostResult.qualityControlCost,
        processCostResult.logisticsCost
    );
    analysis.costDrivers = analysis.breakdown.topCostDrivers;

    // Optimization
    std::vector<std::string> materials = {"Steel", "Aluminum", "Stainless_Steel"};
    std::vector<std::string> processes = {"Standard", "Fast", "Economy"};
    analysis.optimization = optimizer_.optimizeForCost(
        materials, processes, analysis.totalCost);

    // Report
    analysis.report = reporter_.generateCostReport(partName, analysis.costBreakdown, "json");

    return analysis;
}

MaterialCostCalculator& CostAnalysisSystem::getMaterialCalculator() {
    return materialCalc_;
}

ManufacturingCostCalculator& CostAnalysisSystem::getManufacturingCalculator() {
    return mfgCalc_;
}

ProcessCostModel& CostAnalysisSystem::getProcessCostModel() {
    return processModel_;
}

CostOptimizer& CostAnalysisSystem::getOptimizer() {
    return optimizer_;
}

CostBreakdownAnalyzer& CostAnalysisSystem::getBreakdownAnalyzer() {
    return breakdown_;
}

CostReporter& CostAnalysisSystem::getReporter() {
    return reporter_;
}

}  // namespace bendcore::cost_analysis
