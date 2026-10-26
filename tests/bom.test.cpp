#include <gtest/gtest.h>
#include "../include/bendcore/manufacturing/bom.hpp"
#include <sstream>
#include <map>

using namespace bendcore::manufacturing;

class BOMTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create some test items
        testItem1.partNumber = "TUBE-001";
        testItem1.name = "Steel Tube 12x1";
        testItem1.description = "Steel tube OD 12mm WT 1mm";
        testItem1.category = "tube";
        testItem1.quantity = 5;
        testItem1.unit = "pcs";
        testItem1.unitCost = 25.00;
        testItem1.totalCost = 125.00;
        testItem1.material = "Steel";
        testItem1.weight = 0.5;
        testItem1.revision = "1.0";
        testItem1.isCritical = true;

        SupplierInfo supplier1;
        supplier1.name = "MetalSupplies Inc.";
        supplier1.unitCost = 25.00;
        supplier1.leadTimeDays = "7-10";
        supplier1.isPrimary = true;
        testItem1.suppliers.push_back(supplier1);
        testItem1.leadTimeDays = "7-10";

        testItem2.partNumber = "FIT-001";
        testItem2.name = "45 Degree Elbow";
        testItem2.description = "45 degree elbow fitting";
        testItem2.category = "fitting";
        testItem2.quantity = 10;
        testItem2.unit = "pcs";
        testItem2.unitCost = 5.00;
        testItem2.totalCost = 50.00;
        testItem2.material = "Brass";
        testItem2.weight = 0.1;
        testItem2.revision = "1.0";

        SupplierInfo supplier2;
        supplier2.name = "FittingSupplies Ltd.";
        supplier2.unitCost = 5.00;
        supplier2.leadTimeDays = "3-5";
        supplier2.isPrimary = true;
        testItem2.suppliers.push_back(supplier2);
        testItem2.leadTimeDays = "3-5";

        testItem3.partNumber = "BOLT-001";
        testItem3.name = "Bolt M10";
        testItem3.description = "Bolt M10 x 30 Grade 8.8";
        testItem3.category = "fastener";
        testItem3.quantity = 20;
        testItem3.unit = "pcs";
        testItem3.unitCost = 0.50;
        testItem3.totalCost = 10.00;
        testItem3.material = "Steel";
        testItem3.weight = 0.05;
        testItem3.revision = "1.0";

        SupplierInfo supplier3;
        supplier3.name = "FastenerCorp";
        supplier3.unitCost = 0.50;
        supplier3.leadTimeDays = "2-3";
        supplier3.isPrimary = true;
        testItem3.suppliers.push_back(supplier3);
        testItem3.leadTimeDays = "2-3";
    }

    BOMItem testItem1, testItem2, testItem3;
};

class BOMGeneratorTest : public ::testing::Test {
protected:
    BOMGenerator generator_;
};

// ============================================================================
// BOM Creation and Item Management Tests
// ============================================================================

TEST_F(BOMTest, CreateEmptyBOM) {
    BOM bom;
    bom.projectName = "Test Project";
    bom.revision = "1.0";

    EXPECT_EQ(bom.projectName, "Test Project");
    EXPECT_EQ(bom.revision, "1.0");
    EXPECT_EQ(bom.items.size(), 0);
}

TEST_F(BOMTest, AddItemToBOM) {
    BOM bom;
    bom.addItem(testItem1);

    EXPECT_EQ(bom.items.size(), 1);
    EXPECT_EQ(bom.items[0].partNumber, "TUBE-001");
}

TEST_F(BOMTest, AddMultipleItems) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    EXPECT_EQ(bom.items.size(), 3);
}

TEST_F(BOMTest, FindItemByPartNumber) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);

    auto found = bom.findItem("TUBE-001");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "Steel Tube 12x1");
}

TEST_F(BOMTest, FindNonexistentItem) {
    BOM bom;
    bom.addItem(testItem1);

    auto found = bom.findItem("NONEXISTENT");
    EXPECT_EQ(found, nullptr);
}

TEST_F(BOMTest, RemoveItem) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);

    bool removed = bom.removeItem("TUBE-001");
    EXPECT_TRUE(removed);
    EXPECT_EQ(bom.items.size(), 1);
    EXPECT_EQ(bom.items[0].partNumber, "FIT-001");
}

TEST_F(BOMTest, RemoveNonexistentItem) {
    BOM bom;
    bom.addItem(testItem1);

    bool removed = bom.removeItem("NONEXISTENT");
    EXPECT_FALSE(removed);
    EXPECT_EQ(bom.items.size(), 1);
}

TEST_F(BOMTest, UpdateItem) {
    BOM bom;
    bom.addItem(testItem1);

    BOMItem updated = testItem1;
    updated.quantity = 10;
    updated.totalCost = 250.00;

    bool success = bom.updateItem("TUBE-001", updated);
    EXPECT_TRUE(success);

    auto found = bom.findItem("TUBE-001");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->quantity, 10);
    EXPECT_EQ(found->totalCost, 250.00);
}

TEST_F(BOMTest, GetItemsByCategory) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    auto tubes = bom.getItemsByCategory("tube");
    EXPECT_EQ(tubes.size(), 1);
    EXPECT_EQ(tubes[0]->partNumber, "TUBE-001");

    auto fasteners = bom.getItemsByCategory("fastener");
    EXPECT_EQ(fasteners.size(), 1);
    EXPECT_EQ(fasteners[0]->partNumber, "BOLT-001");
}

// ============================================================================
// BOM Statistics Tests
// ============================================================================

TEST_F(BOMTest, CalculateStatisticsBasic) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();

    EXPECT_EQ(stats.totalItems, 3);
    EXPECT_EQ(stats.uniqueItems, 3);
    EXPECT_EQ(stats.totalQuantity, 35);  // 5 + 10 + 20
}

TEST_F(BOMTest, CalculateTotalCost) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();
    double expectedCost = 125.00 + 50.00 + 10.00;

    EXPECT_DOUBLE_EQ(stats.totalCost, expectedCost);
}

TEST_F(BOMTest, CalculateTotalWeight) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();
    double expectedWeight = (0.5 * 5) + (0.1 * 10) + (0.05 * 20);

    EXPECT_DOUBLE_EQ(stats.totalWeight, expectedWeight);
}

TEST_F(BOMTest, StatisticsByCategory) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();

    EXPECT_EQ(stats.itemsByCategory["tube"], 1);
    EXPECT_EQ(stats.itemsByCategory["fitting"], 1);
    EXPECT_EQ(stats.itemsByCategory["fastener"], 1);

    EXPECT_EQ(stats.quantityByCategory["tube"], 5);
    EXPECT_EQ(stats.quantityByCategory["fitting"], 10);
    EXPECT_EQ(stats.quantityByCategory["fastener"], 20);
}

TEST_F(BOMTest, StatisticsByCostCategory) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();

    EXPECT_DOUBLE_EQ(stats.costByCategory["tube"], 125.00);
    EXPECT_DOUBLE_EQ(stats.costByCategory["fitting"], 50.00);
    EXPECT_DOUBLE_EQ(stats.costByCategory["fastener"], 10.00);
}

TEST_F(BOMTest, StatisticsBySupplier) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    const auto& stats = bom.getStatistics();

    EXPECT_EQ(stats.itemsBySupplier["MetalSupplies Inc."], 1);
    EXPECT_EQ(stats.itemsBySupplier["FittingSupplies Ltd."], 1);
    EXPECT_EQ(stats.itemsBySupplier["FastenerCorp"], 1);
}

TEST_F(BOMTest, CriticalItemsIdentification) {
    BOM bom;
    bom.addItem(testItem1);  // Critical
    bom.addItem(testItem2);  // Not critical

    const auto& stats = bom.getStatistics();

    EXPECT_EQ(stats.criticalItemCount, 1);
    EXPECT_TRUE(bom.hasCriticalItems());
    EXPECT_EQ(stats.criticalItems.size(), 1);
    EXPECT_EQ(stats.criticalItems[0], "TUBE-001");
}

TEST_F(BOMTest, StatisticsCache) {
    BOM bom;
    bom.addItem(testItem1);

    const auto& stats1 = bom.getStatistics();
    const auto& stats2 = bom.getStatistics();

    // Should return same cached object
    EXPECT_EQ(&stats1, &stats2);
}

TEST_F(BOMTest, CacheClearedAfterModification) {
    BOM bom;
    bom.addItem(testItem1);

    const auto& stats1 = bom.getStatistics();
    const auto* ptr1 = &stats1;

    bom.addItem(testItem2);

    const auto& stats2 = bom.getStatistics();
    const auto* ptr2 = &stats2;

    // Cache should be invalidated and new object created
    EXPECT_NE(ptr1, ptr2);
}

// ============================================================================
// BOM Properties and Filtering Tests
// ============================================================================

TEST_F(BOMTest, AddCustomProperty) {
    CustomProperty prop;
    prop.key = "color";
    prop.value = "red";
    prop.type = "string";

    BOMItem item = testItem1;
    item.customProperties.push_back(prop);

    BOM bom;
    bom.addItem(item);

    EXPECT_EQ(bom.items[0].customProperties.size(), 1);
    EXPECT_EQ(bom.items[0].customProperties[0].key, "color");
}

TEST_F(BOMTest, FilterByCustomProperty) {
    CustomProperty prop;
    prop.key = "material_type";
    prop.value = "steel";
    prop.type = "string";

    BOMItem item1 = testItem1;
    item1.customProperties.push_back(prop);

    BOMItem item2 = testItem2;
    CustomProperty prop2;
    prop2.key = "material_type";
    prop2.value = "brass";
    prop2.type = "string";
    item2.customProperties.push_back(prop2);

    BOM bom;
    bom.addItem(item1);
    bom.addItem(item2);

    auto filtered = bom.filterByProperty("material_type", "steel");
    EXPECT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].partNumber, "TUBE-001");
}

TEST_F(BOMTest, AddManufacturingNote) {
    ManufacturingNote note;
    note.text = "Handle with care - fragile";
    note.category = "safety";
    note.isCritical = true;

    BOMItem item = testItem1;
    item.notes.push_back(note);

    BOM bom;
    bom.addItem(item);

    EXPECT_EQ(bom.items[0].notes.size(), 1);
    EXPECT_EQ(bom.items[0].notes[0].text, "Handle with care - fragile");
    EXPECT_TRUE(bom.items[0].notes[0].isCritical);
}

// ============================================================================
// BOM Accessor Tests
// ============================================================================

TEST_F(BOMTest, GetTotalCost) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    double expectedCost = 125.00 + 50.00 + 10.00;
    EXPECT_DOUBLE_EQ(bom.getTotalCost(), expectedCost);
}

TEST_F(BOMTest, GetTotalWeight) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    double expectedWeight = (0.5 * 5) + (0.1 * 10) + (0.05 * 20);
    EXPECT_DOUBLE_EQ(bom.getTotalWeight(), expectedWeight);
}

TEST_F(BOMTest, GetUniquePartCount) {
    BOM bom;
    bom.addItem(testItem1);
    bom.addItem(testItem2);
    bom.addItem(testItem3);

    EXPECT_EQ(bom.getUniquePartCount(), 3);
}

// ============================================================================
// BOMGenerator Tests
// ============================================================================

TEST_F(BOMGeneratorTest, GenerateFromTubesSimple) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5},
        {"Aluminum_10_0.8", 3}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);

    ASSERT_NE(bom, nullptr);
    EXPECT_EQ(bom->projectName, "Test Project");
    EXPECT_EQ(bom->revision, "1.0");
    EXPECT_EQ(bom->items.size(), 2);
}

TEST_F(BOMGeneratorTest, GenerateFromTubesAggregates) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5},
        {"Steel_12_1", 3},
        {"Aluminum_10_0.8", 2}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);

    ASSERT_NE(bom, nullptr);
    // Should aggregate same specs
    EXPECT_EQ(bom->items.size(), 2);

    // Find the aggregated steel tube
    auto steelItem = bom->findItem("TUBE-1");
    ASSERT_NE(steelItem, nullptr);
    EXPECT_EQ(steelItem->quantity, 8);  // 5 + 3
}

TEST_F(BOMGeneratorTest, ExportToCSV) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);
    std::string csv = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::CSV);

    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("Test Project"), std::string::npos);
    EXPECT_NE(csv.find("Part Number"), std::string::npos);
}

TEST_F(BOMGeneratorTest, ExportToJSON) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);
    std::string json = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::JSON);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("{"), std::string::npos);
    EXPECT_NE(json.find("}"), std::string::npos);
    EXPECT_NE(json.find("projectName"), std::string::npos);
}

TEST_F(BOMGeneratorTest, ExportToXML) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);
    std::string xml = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::XML);

    EXPECT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<?xml"), std::string::npos);
    EXPECT_NE(xml.find("<BOM>"), std::string::npos);
    EXPECT_NE(xml.find("</BOM>"), std::string::npos);
}

TEST_F(BOMGeneratorTest, ExportToHTML) {
    std::vector<std::pair<std::string, int>> tubeSpecs = {
        {"Steel_12_1", 5}
    };

    auto bom = generator_.generateFromTubes("Test Project", tubeSpecs);
    std::string html = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::HTML);

    EXPECT_FALSE(html.empty());
    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html.find("Bill of Materials"), std::string::npos);
    EXPECT_NE(html.find("<table>"), std::string::npos);
}

TEST_F(BOMGeneratorTest, MergeBOMs) {
    std::vector<std::pair<std::string, int>> specs1 = {{"Steel_12_1", 5}};
    std::vector<std::pair<std::string, int>> specs2 = {{"Aluminum_10_0.8", 3}};

    auto bom1 = generator_.generateFromTubes("Project 1", specs1);
    auto bom2 = generator_.generateFromTubes("Project 2", specs2);

    std::vector<const BOM*> boms = {bom1.get(), bom2.get()};
    auto merged = generator_.mergeBOMs(boms, "Merged Project");

    ASSERT_NE(merged, nullptr);
    EXPECT_EQ(merged->projectName, "Merged Project");
    // After merging with aggregation, should have 2 items
    EXPECT_LE(merged->items.size(), 2);
}

TEST_F(BOMGeneratorTest, SetPartNumberingScheme) {
    generator_.setPartNumberingScheme("PN-", 100, "-REV1");

    std::vector<std::pair<std::string, int>> specs = {
        {"Steel_12_1", 5},
        {"Aluminum_10_0.8", 3}
    };

    auto bom = generator_.generateFromTubes("Test Project", specs);

    // Check that part numbers follow the scheme
    bool hasCustomPartNumber = false;
    for (const auto& item : bom->items) {
        if (item.partNumber.find("PN-") == 0 && item.partNumber.find("-REV1") != std::string::npos) {
            hasCustomPartNumber = true;
            break;
        }
    }
    EXPECT_TRUE(hasCustomPartNumber);
}

TEST_F(BOMGeneratorTest, GetSuppliersForCategory) {
    auto suppliers = generator_.getSuppliersForCategory("tube");

    EXPECT_GT(suppliers.size(), 0);
    EXPECT_EQ(suppliers[0].name, "MetalSupplies Inc.");
}

TEST_F(BOMGeneratorTest, GetSuppliersForFitting) {
    auto suppliers = generator_.getSuppliersForCategory("fitting");

    EXPECT_GT(suppliers.size(), 0);
    EXPECT_EQ(suppliers[0].name, "FittingSupplies Ltd.");
}

// ============================================================================
// Edge Cases and Complex Scenarios
// ============================================================================

TEST_F(BOMTest, EmptyBOMStatistics) {
    BOM bom;
    const auto& stats = bom.getStatistics();

    EXPECT_EQ(stats.totalItems, 0);
    EXPECT_EQ(stats.uniqueItems, 0);
    EXPECT_EQ(stats.totalQuantity, 0);
    EXPECT_DOUBLE_EQ(stats.totalCost, 0.0);
    EXPECT_DOUBLE_EQ(stats.totalWeight, 0.0);
}

TEST_F(BOMTest, LargeQuantityAggregation) {
    BOM bom;

    for (int i = 0; i < 100; ++i) {
        BOMItem item = testItem1;
        item.partNumber = "TUBE-001";
        item.quantity = 1;
        item.totalCost = 25.00;
        bom.addItem(item);
    }

    // All items should be the same, just accumulated in the list
    EXPECT_EQ(bom.items.size(), 100);
}

TEST_F(BOMGeneratorTest, GenerateEmptyTubeList) {
    std::vector<std::pair<std::string, int>> tubeSpecs;
    auto bom = generator_.generateFromTubes("Empty Project", tubeSpecs);

    ASSERT_NE(bom, nullptr);
    EXPECT_EQ(bom->items.size(), 0);
}

TEST_F(BOMGeneratorTest, ExportWithComplexData) {
    std::vector<std::pair<std::string, int>> specs = {
        {"Steel_12_1", 5},
        {"Steel_14_1.2", 3},
        {"Aluminum_10_0.8", 2}
    };

    auto bom = generator_.generateFromTubes("Complex Project", specs);

    // All formats should export successfully
    std::string csv = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::CSV);
    std::string json = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::JSON);
    std::string xml = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::XML);
    std::string html = generator_.exportBOM(*bom, BOMGenerator::BOMFormat::HTML);

    EXPECT_FALSE(csv.empty());
    EXPECT_FALSE(json.empty());
    EXPECT_FALSE(xml.empty());
    EXPECT_FALSE(html.empty());
}

TEST_F(BOMTest, ItemWithMultipleSuppliers) {
    BOMItem item = testItem1;

    SupplierInfo supplier2;
    supplier2.name = "AlternateMetals Inc.";
    supplier2.unitCost = 24.00;
    supplier2.leadTimeDays = "10-15";
    supplier2.isPrimary = false;

    item.suppliers.push_back(supplier2);

    BOM bom;
    bom.addItem(item);

    EXPECT_EQ(bom.items[0].suppliers.size(), 2);
}

TEST_F(BOMTest, HighValueCriticalItems) {
    BOMItem highValueItem = testItem1;
    highValueItem.totalCost = 5000.00;
    highValueItem.isCritical = true;

    BOM bom;
    bom.addItem(highValueItem);

    const auto& stats = bom.getStatistics();
    EXPECT_DOUBLE_EQ(stats.totalCost, 5000.00);
    EXPECT_EQ(stats.criticalItemCount, 1);
}

TEST_F(BOMGeneratorTest, BOMCreationDate) {
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = generator_.generateFromTubes("Test Project", specs);

    EXPECT_FALSE(bom->createdDate.empty());
    EXPECT_EQ(bom->createdBy, "BendCore CAD System");
}

TEST_F(BOMGeneratorTest, ExportBOMToFile) {
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = generator_.generateFromTubes("Test Project", specs);

    std::string tmpfile = "/tmp/test_bom.csv";
    bool success = generator_.exportBOMToFile(*bom, tmpfile, BOMGenerator::BOMFormat::CSV);

    EXPECT_TRUE(success);

    // Cleanup
    std::remove(tmpfile.c_str());
}

TEST_F(BOMTest, CostCalculationAccuracy) {
    BOM bom;

    // Create multiple items with known costs
    BOMItem item1 = testItem1;
    item1.quantity = 10;
    item1.unitCost = 100.00;
    item1.totalCost = 1000.00;

    BOMItem item2 = testItem2;
    item2.quantity = 5;
    item2.unitCost = 50.00;
    item2.totalCost = 250.00;

    bom.addItem(item1);
    bom.addItem(item2);

    EXPECT_DOUBLE_EQ(bom.getTotalCost(), 1250.00);
}

TEST_F(BOMTest, WeightCalculationAccuracy) {
    BOM bom;

    BOMItem item1 = testItem1;
    item1.quantity = 10;
    item1.weight = 0.5;  // 10 * 0.5 = 5.0

    BOMItem item2 = testItem2;
    item2.quantity = 20;
    item2.weight = 0.2;  // 20 * 0.2 = 4.0

    bom.addItem(item1);
    bom.addItem(item2);

    EXPECT_DOUBLE_EQ(bom.getTotalWeight(), 9.0);
}

TEST_F(BOMGeneratorTest, CreateHierarchy) {
    std::vector<BOMItem> items;
    items.push_back(testItem1);
    items.push_back(testItem2);

    auto hierarchy = generator_.createHierarchy(items, "ASM-001", "Main Assembly");

    ASSERT_NE(hierarchy, nullptr);
    EXPECT_EQ(hierarchy->assemblyPartNumber, "ASM-001");
    EXPECT_EQ(hierarchy->assemblyName, "Main Assembly");
    EXPECT_EQ(hierarchy->items.size(), 2);
}

TEST_F(BOMTest, MetadataStorage) {
    BOM bom;
    bom.metadata["customer"] = "Acme Corp";
    bom.metadata["order_date"] = "2026-10-26";
    bom.metadata["engineer"] = "John Doe";

    EXPECT_EQ(bom.metadata["customer"], "Acme Corp");
    EXPECT_EQ(bom.metadata["order_date"], "2026-10-26");
}
