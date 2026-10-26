#include "../include/bendcore/manufacturing/bom.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace bendcore::manufacturing;

// Simple test utilities
int testsPassed = 0;
int testsFailed = 0;

void assertCondition(bool condition, const char* message) {
    if (condition) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << std::endl;
        testsFailed++;
    }
}

void assertEquals(const std::string& actual, const std::string& expected, const char* message) {
    if (actual == expected) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << " (expected: '" << expected << "', got: '" << actual << "')" << std::endl;
        testsFailed++;
    }
}

void assertEquals(int actual, int expected, const char* message) {
    if (actual == expected) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
        testsFailed++;
    }
}

void assertEquals(double actual, double expected, double tolerance, const char* message) {
    if (actual >= expected - tolerance && actual <= expected + tolerance) {
        std::cout << "[PASS] " << message << std::endl;
        testsPassed++;
    } else {
        std::cout << "[FAIL] " << message << " (expected: " << expected << ", got: " << actual << ")" << std::endl;
        testsFailed++;
    }
}

// Test 1: Create empty BOM
void test_createEmptyBOM() {
    BOM bom;
    bom.projectName = "Test Project";
    bom.revision = "1.0";

    assertEquals(bom.projectName, "Test Project", "BOM project name set");
    assertEquals(bom.revision, "1.0", "BOM revision set");
    assertEquals((int)bom.items.size(), 0, "BOM starts empty");
}

// Test 2: Add item to BOM
void test_addItemToBOM() {
    BOM bom;
    BOMItem item;
    item.partNumber = "TUBE-001";
    item.name = "Steel Tube";
    item.quantity = 5;

    bom.addItem(item);

    assertEquals((int)bom.items.size(), 1, "BOM contains one item after add");
    assertEquals(bom.items[0].partNumber, "TUBE-001", "BOM item part number correct");
}

// Test 3: Add multiple items
void test_addMultipleItems() {
    BOM bom;

    BOMItem item1;
    item1.partNumber = "TUBE-001";
    item1.quantity = 5;
    bom.addItem(item1);

    BOMItem item2;
    item2.partNumber = "FIT-001";
    item2.quantity = 10;
    bom.addItem(item2);

    BOMItem item3;
    item3.partNumber = "BOLT-001";
    item3.quantity = 20;
    bom.addItem(item3);

    assertEquals((int)bom.items.size(), 3, "BOM contains three items");
}

// Test 4: Find item by part number
void test_findItem() {
    BOM bom;
    BOMItem item;
    item.partNumber = "TUBE-001";
    item.name = "Steel Tube";
    bom.addItem(item);

    auto found = bom.findItem("TUBE-001");
    assertCondition(found != nullptr, "Item found by part number");
    if (found) {
        assertEquals(found->name, "Steel Tube", "Found item has correct name");
    }
}

// Test 5: Find nonexistent item
void test_findNonexistentItem() {
    BOM bom;
    BOMItem item;
    item.partNumber = "TUBE-001";
    bom.addItem(item);

    auto found = bom.findItem("NONEXISTENT");
    assertCondition(found == nullptr, "Nonexistent item returns nullptr");
}

// Test 6: Remove item
void test_removeItem() {
    BOM bom;
    BOMItem item1, item2;
    item1.partNumber = "TUBE-001";
    item2.partNumber = "FIT-001";
    bom.addItem(item1);
    bom.addItem(item2);

    bool removed = bom.removeItem("TUBE-001");

    assertCondition(removed, "removeItem returns true");
    assertEquals((int)bom.items.size(), 1, "BOM has one item after removal");
    assertEquals(bom.items[0].partNumber, "FIT-001", "Correct item remains");
}

// Test 7: Remove nonexistent item
void test_removeNonexistentItem() {
    BOM bom;
    BOMItem item;
    item.partNumber = "TUBE-001";
    bom.addItem(item);

    bool removed = bom.removeItem("NONEXISTENT");

    assertCondition(!removed, "removeItem returns false for nonexistent");
    assertEquals((int)bom.items.size(), 1, "BOM still has one item");
}

// Test 8: Get items by category
void test_getItemsByCategory() {
    BOM bom;
    BOMItem item1, item2, item3;

    item1.partNumber = "TUBE-001";
    item1.category = "tube";
    item2.partNumber = "FIT-001";
    item2.category = "fitting";
    item3.partNumber = "BOLT-001";
    item3.category = "fastener";

    bom.addItem(item1);
    bom.addItem(item2);
    bom.addItem(item3);

    auto tubes = bom.getItemsByCategory("tube");
    auto fittings = bom.getItemsByCategory("fitting");

    assertEquals((int)tubes.size(), 1, "One tube found");
    assertEquals((int)fittings.size(), 1, "One fitting found");
}

// Test 9: Calculate statistics
void test_calculateStatistics() {
    BOM bom;

    BOMItem item1;
    item1.partNumber = "TUBE-001";
    item1.quantity = 5;
    item1.unitCost = 25.0;
    item1.totalCost = 125.0;
    item1.weight = 0.5;

    BOMItem item2;
    item2.partNumber = "FIT-001";
    item2.quantity = 10;
    item2.unitCost = 5.0;
    item2.totalCost = 50.0;
    item2.weight = 0.1;

    bom.addItem(item1);
    bom.addItem(item2);

    const auto& stats = bom.getStatistics();

    assertEquals(stats.totalItems, 2, "Total items correct");
    assertEquals(stats.uniqueItems, 2, "Unique items correct");
    assertEquals(stats.totalQuantity, 15, "Total quantity correct");
    assertEquals(stats.totalCost, 175.0, 0.01, "Total cost correct");
    assertEquals(stats.totalWeight, 3.5, 0.01, "Total weight correct");
}

// Test 10: Total cost getter
void test_getTotalCost() {
    BOM bom;
    BOMItem item1, item2;

    item1.partNumber = "TUBE-001";
    item1.totalCost = 125.0;
    item2.partNumber = "FIT-001";
    item2.totalCost = 50.0;

    bom.addItem(item1);
    bom.addItem(item2);

    assertEquals(bom.getTotalCost(), 175.0, 0.01, "getTotalCost returns correct value");
}

// Test 11: Total weight getter
void test_getTotalWeight() {
    BOM bom;
    BOMItem item1, item2;

    item1.partNumber = "TUBE-001";
    item1.weight = 0.5;
    item1.quantity = 5;
    item2.partNumber = "FIT-001";
    item2.weight = 0.1;
    item2.quantity = 10;

    bom.addItem(item1);
    bom.addItem(item2);

    assertEquals(bom.getTotalWeight(), 3.5, 0.01, "getTotalWeight returns correct value");
}

// Test 12: Unique part count
void test_getUniquePartCount() {
    BOM bom;
    BOMItem item1, item2, item3;
    item1.partNumber = "TUBE-001";
    item2.partNumber = "FIT-001";
    item3.partNumber = "BOLT-001";

    bom.addItem(item1);
    bom.addItem(item2);
    bom.addItem(item3);

    assertEquals(bom.getUniquePartCount(), 3, "Unique part count correct");
}

// Test 13: Critical items
void test_criticalItems() {
    BOM bom;
    BOMItem item1, item2;

    item1.partNumber = "TUBE-001";
    item1.isCritical = true;
    item2.partNumber = "FIT-001";
    item2.isCritical = false;

    bom.addItem(item1);
    bom.addItem(item2);

    assertCondition(bom.hasCriticalItems(), "BOM has critical items");
    const auto& stats = bom.getStatistics();
    assertEquals(stats.criticalItemCount, 1, "Critical item count correct");
}

// Test 14: BOMGenerator - generate from tubes
void test_generateFromTubes() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {
        {"Steel_12_1", 5},
        {"Aluminum_10_0.8", 3}
    };

    auto bom = gen.generateFromTubes("Test Project", specs);

    assertCondition(bom != nullptr, "BOM generated");
    assertEquals(bom->projectName, "Test Project", "BOM project name");
    assertEquals((int)bom->items.size(), 2, "BOM has 2 items");
}

// Test 15: BOMGenerator - export to CSV
void test_exportToCSV() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = gen.generateFromTubes("Test Project", specs);

    std::string csv = gen.exportBOM(*bom, BOMGenerator::BOMFormat::CSV);

    assertCondition(!csv.empty(), "CSV export not empty");
    assertCondition(csv.find("Test Project") != std::string::npos, "CSV contains project name");
}

// Test 16: BOMGenerator - export to JSON
void test_exportToJSON() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = gen.generateFromTubes("Test Project", specs);

    std::string json = gen.exportBOM(*bom, BOMGenerator::BOMFormat::JSON);

    assertCondition(!json.empty(), "JSON export not empty");
    assertCondition(json.find("{") != std::string::npos, "JSON contains opening brace");
    assertCondition(json.find("}") != std::string::npos, "JSON contains closing brace");
}

// Test 17: BOMGenerator - export to XML
void test_exportToXML() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = gen.generateFromTubes("Test Project", specs);

    std::string xml = gen.exportBOM(*bom, BOMGenerator::BOMFormat::XML);

    assertCondition(!xml.empty(), "XML export not empty");
    assertCondition(xml.find("<?xml") != std::string::npos, "XML has XML declaration");
    assertCondition(xml.find("<BOM>") != std::string::npos, "XML has BOM tag");
}

// Test 18: BOMGenerator - export to HTML
void test_exportToHTML() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = gen.generateFromTubes("Test Project", specs);

    std::string html = gen.exportBOM(*bom, BOMGenerator::BOMFormat::HTML);

    assertCondition(!html.empty(), "HTML export not empty");
    assertCondition(html.find("<!DOCTYPE html>") != std::string::npos, "HTML has DOCTYPE");
    assertCondition(html.find("<table>") != std::string::npos, "HTML has table");
}

// Test 19: BOMGenerator - merge BOMs
void test_mergeBOMs() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs1 = {{"Steel_12_1", 5}};
    std::vector<std::pair<std::string, int>> specs2 = {{"Aluminum_10_0.8", 3}};

    auto bom1 = gen.generateFromTubes("Project 1", specs1);
    auto bom2 = gen.generateFromTubes("Project 2", specs2);

    std::vector<const BOM*> boms = {bom1.get(), bom2.get()};
    auto merged = gen.mergeBOMs(boms, "Merged Project");

    assertCondition(merged != nullptr, "Merged BOM created");
    assertEquals(merged->projectName, "Merged Project", "Merged BOM has correct name");
}

// Test 20: BOMGenerator - set part numbering scheme
void test_partNumberingScheme() {
    BOMGenerator gen;
    gen.setPartNumberingScheme("PN-", 100, "-REV1");

    std::vector<std::pair<std::string, int>> specs = {
        {"Steel_12_1", 5},
        {"Aluminum_10_0.8", 3}
    };

    auto bom = gen.generateFromTubes("Test Project", specs);

    bool hasCustomPN = false;
    for (const auto& item : bom->items) {
        if (item.partNumber.find("PN-") == 0) {
            hasCustomPN = true;
            break;
        }
    }
    assertCondition(hasCustomPN, "Custom part numbering scheme applied");
}

// Test 21: Update item
void test_updateItem() {
    BOM bom;
    BOMItem item;
    item.partNumber = "TUBE-001";
    item.quantity = 5;
    bom.addItem(item);

    BOMItem updated = item;
    updated.quantity = 10;
    bool success = bom.updateItem("TUBE-001", updated);

    assertCondition(success, "updateItem returns true");
    auto found = bom.findItem("TUBE-001");
    if (found) {
        assertEquals(found->quantity, 10, "Item quantity updated");
    }
}

// Test 22: Add supplier info
void test_supplierInfo() {
    BOMItem item;
    item.partNumber = "TUBE-001";

    SupplierInfo supplier;
    supplier.name = "MetalSupplies Inc.";
    supplier.unitCost = 25.0;
    supplier.leadTimeDays = "7-10";

    item.suppliers.push_back(supplier);

    assertCondition(item.suppliers.size() > 0, "Supplier added to item");
    assertEquals(item.suppliers[0].name, "MetalSupplies Inc.", "Supplier name correct");
}

// Test 23: Custom properties
void test_customProperties() {
    BOMItem item;
    item.partNumber = "TUBE-001";

    CustomProperty prop;
    prop.key = "color";
    prop.value = "red";
    prop.type = "string";

    item.customProperties.push_back(prop);

    assertCondition(item.customProperties.size() > 0, "Custom property added");
    assertEquals(item.customProperties[0].key, "color", "Property key correct");
}

// Test 24: Manufacturing notes
void test_manufacturingNotes() {
    BOMItem item;
    item.partNumber = "TUBE-001";

    ManufacturingNote note;
    note.text = "Handle with care";
    note.category = "safety";
    note.isCritical = true;

    item.notes.push_back(note);

    assertCondition(item.notes.size() > 0, "Manufacturing note added");
    assertEquals(item.notes[0].category, "safety", "Note category correct");
}

// Test 25: Cache invalidation
void test_cacheInvalidation() {
    BOM bom;
    BOMItem item1;
    item1.partNumber = "TUBE-001";
    item1.quantity = 5;
    bom.addItem(item1);

    const auto& stats1 = bom.getStatistics();
    int items1 = stats1.totalItems;

    BOMItem item2;
    item2.partNumber = "FIT-001";
    item2.quantity = 10;
    bom.addItem(item2);

    const auto& stats2 = bom.getStatistics();
    int items2 = stats2.totalItems;

    // Verify that statistics were recalculated (item count should increase)
    assertCondition(items2 > items1, "Cache invalidated after modification");
}

// Test 26: Empty BOM statistics
void test_emptyBOMStatistics() {
    BOM bom;
    const auto& stats = bom.getStatistics();

    assertEquals(stats.totalItems, 0, "Empty BOM has 0 items");
    assertEquals(stats.uniqueItems, 0, "Empty BOM has 0 unique items");
    assertEquals(stats.totalCost, 0.0, 0.01, "Empty BOM has 0 cost");
}

// Test 27: Category statistics
void test_categoryStatistics() {
    BOM bom;

    BOMItem item1;
    item1.partNumber = "TUBE-001";
    item1.category = "tube";
    item1.quantity = 5;
    item1.totalCost = 125.0;

    BOMItem item2;
    item2.partNumber = "FIT-001";
    item2.category = "fitting";
    item2.quantity = 10;
    item2.totalCost = 50.0;

    bom.addItem(item1);
    bom.addItem(item2);

    const auto& stats = bom.getStatistics();

    if (stats.itemsByCategory.find("tube") != stats.itemsByCategory.end()) {
        assertEquals(stats.itemsByCategory.at("tube"), 1, "Tube category count");
    }
    if (stats.quantityByCategory.find("tube") != stats.quantityByCategory.end()) {
        assertEquals(stats.quantityByCategory.at("tube"), 5, "Tube quantity");
    }
    if (stats.costByCategory.find("tube") != stats.costByCategory.end()) {
        assertEquals(stats.costByCategory.at("tube"), 125.0, 0.01, "Tube cost");
    }
}

// Test 28: Get suppliers for category
void test_getSuppliersForCategory() {
    BOMGenerator gen;
    auto suppliers = gen.getSuppliersForCategory("tube");

    assertCondition(suppliers.size() > 0, "Suppliers returned for category");
}

// Test 29: Metadata storage
void test_metadata() {
    BOM bom;
    bom.metadata["customer"] = "Acme Corp";
    bom.metadata["order_date"] = "2026-10-26";

    assertEquals(bom.metadata["customer"], "Acme Corp", "Metadata stored");
    assertEquals(bom.metadata["order_date"], "2026-10-26", "Metadata retrieved");
}

// Test 30: BOM creation date
void test_bomCreationDate() {
    BOMGenerator gen;
    std::vector<std::pair<std::string, int>> specs = {{"Steel_12_1", 5}};
    auto bom = gen.generateFromTubes("Test Project", specs);

    assertCondition(!bom->createdDate.empty(), "BOM creation date set");
    assertEquals(bom->createdBy, "BendCore CAD System", "BOM created by set");
}

// Main test runner
int main() {
    std::cout << "========== BOM Generator Tests ==========" << std::endl;
    std::cout << "" << std::endl;

    test_createEmptyBOM();
    test_addItemToBOM();
    test_addMultipleItems();
    test_findItem();
    test_findNonexistentItem();
    test_removeItem();
    test_removeNonexistentItem();
    test_getItemsByCategory();
    test_calculateStatistics();
    test_getTotalCost();
    test_getTotalWeight();
    test_getUniquePartCount();
    test_criticalItems();
    test_generateFromTubes();
    test_exportToCSV();
    test_exportToJSON();
    test_exportToXML();
    test_exportToHTML();
    test_mergeBOMs();
    test_partNumberingScheme();
    test_updateItem();
    test_supplierInfo();
    test_customProperties();
    test_manufacturingNotes();
    test_cacheInvalidation();
    test_emptyBOMStatistics();
    test_categoryStatistics();
    test_getSuppliersForCategory();
    test_metadata();
    test_bomCreationDate();

    std::cout << "" << std::endl;
    std::cout << "========== Test Results ==========" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total: " << (testsPassed + testsFailed) << std::endl;

    if (testsFailed == 0) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
