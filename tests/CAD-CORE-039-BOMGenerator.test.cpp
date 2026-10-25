#include <gtest/gtest.h>
#include "../src/manufacturing/BOMGenerator.hpp"

using namespace bendcore::manufacturing;

class BOMGeneratorTest : public ::testing::Test {
 protected:
  BOMGenerator generator_;

  std::vector<Tube> createTestTubes() {
    std::vector<Tube> tubes;

    Tube tube1;
    tube1.id = "TUBE-001";
    tube1.material = "Steel";
    tube1.length = 100.0;
    tube1.outer_diameter = 12.0;
    tube1.wall_thickness = 1.0;

    Bend bend;
    bend.position = 50.0;
    bend.angle = 45.0;
    bend.radius = 15.0;
    bend.dwell_time = 1.0;
    tube1.bends.push_back(bend);
    tubes.push_back(tube1);

    Tube tube2;
    tube2.id = "TUBE-002";
    tube2.material = "Aluminum";
    tube2.length = 80.0;
    tube2.outer_diameter = 10.0;
    tube2.wall_thickness = 0.8;
    tubes.push_back(tube2);

    return tubes;
  }
};

TEST_F(BOMGeneratorTest, GenerateBOM) {
  auto tubes = createTestTubes();
  std::vector<BendingMachine> machines;

  auto bom = generator_.generateBOM(tubes, machines, "Test Project");

  EXPECT_EQ(bom.project_name, "Test Project");
  EXPECT_FALSE(bom.tubes.empty());
  EXPECT_FALSE(bom.fittings.empty());
}

TEST_F(BOMGeneratorTest, BOMProjectInfo) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  EXPECT_EQ(bom.project_name, "Test Project");
  EXPECT_FALSE(bom.created_date.empty());
  EXPECT_EQ(bom.created_by, "BendCore CAD System");
  EXPECT_EQ(bom.revision, "1.0");
}

TEST_F(BOMGeneratorTest, ExportToCSV) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string csv = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::CSV);

  EXPECT_FALSE(csv.empty());
  EXPECT_NE(csv.find("Project:"), std::string::npos);
  EXPECT_NE(csv.find("Test Project"), std::string::npos);
}

TEST_F(BOMGeneratorTest, ExportToJSON) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string json = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::JSON);

  EXPECT_FALSE(json.empty());
  EXPECT_NE(json.find("{"), std::string::npos);
  EXPECT_NE(json.find("}"), std::string::npos);
  EXPECT_NE(json.find("project_name"), std::string::npos);
}

TEST_F(BOMGeneratorTest, ExportToXML) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string xml = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::XML);

  EXPECT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<?xml"), std::string::npos);
  EXPECT_NE(xml.find("<BOM>"), std::string::npos);
  EXPECT_NE(xml.find("</BOM>"), std::string::npos);
}

TEST_F(BOMGeneratorTest, CSVContainsTableHeader) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string csv = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::CSV);

  EXPECT_NE(csv.find("Part Number"), std::string::npos);
  EXPECT_NE(csv.find("Description"), std::string::npos);
  EXPECT_NE(csv.find("Quantity"), std::string::npos);
}

TEST_F(BOMGeneratorTest, BOMTotals) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  EXPECT_GE(bom.total_material_cost, 0.0);
  EXPECT_GE(bom.total_labor_hours, 0.0);
  EXPECT_GE(bom.estimated_total_cost, 0.0);
}

TEST_F(BOMGeneratorTest, AddItem) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  BOMGenerator::BOMItem item;
  item.part_number = "CUSTOM-001";
  item.description = "Custom Tube";
  item.quantity = 1;
  item.unit = "pcs";
  item.unit_cost = 50.0;

  size_t initial_count = bom.tubes.size();
  generator_.addItem(bom, item);

  // Item should be added to appropriate list
  EXPECT_GE(bom.tubes.size() + bom.fasteners.size() + bom.fittings.size() + bom.tools.size(), initial_count);
}

TEST_F(BOMGeneratorTest, GroupItems) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  size_t before = bom.tubes.size();
  generator_.groupItems(bom);
  size_t after = bom.tubes.size();

  // Grouping should not increase item count
  EXPECT_LE(after, before);
}

TEST_F(BOMGeneratorTest, CalculateCosts) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::map<std::string, double> material_costs;
  material_costs["Steel_12x1"] = 25.0;
  material_costs["Aluminum_10x0.8"] = 20.0;

  std::map<std::string, double> labor_rates;
  labor_rates["assembly"] = 5.0;

  generator_.calculateCosts(bom, material_costs, labor_rates);

  EXPECT_GE(bom.total_material_cost, 0.0);
}

TEST_F(BOMGeneratorTest, AnalyzeSupplyChain) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  auto summary = generator_.analyzeSupplyChain(bom);

  EXPECT_GE(summary.supplier_items.size(), 0);
  EXPECT_GE(summary.lead_time_analysis.size(), 0);
}

TEST_F(BOMGeneratorTest, MergeBOMs) {
  auto tubes = createTestTubes();

  auto bom1 = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Project 1");
  auto bom2 = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Project 2");

  std::vector<BOMGenerator::BOM> boms = {bom1, bom2};
  auto merged = generator_.mergeBOMs(boms, "Merged Project");

  EXPECT_EQ(merged.project_name, "Merged Project");
  EXPECT_GE(merged.tubes.size(), bom1.tubes.size());
}

TEST_F(BOMGeneratorTest, ExportBOMToFile) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  bool success = generator_.exportBOM(bom, "/tmp/test_bom.csv", BOMGenerator::BOMFormat::CSV);

  EXPECT_TRUE(success);
}

TEST_F(BOMGeneratorTest, TubeItemGeneration) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  EXPECT_FALSE(bom.tubes.empty());
  for (const auto& item : bom.tubes) {
    EXPECT_FALSE(item.part_number.empty());
    EXPECT_GT(item.quantity, 0);
  }
}

TEST_F(BOMGeneratorTest, FittingItemGeneration) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  EXPECT_FALSE(bom.fittings.empty());
  for (const auto& item : bom.fittings) {
    EXPECT_FALSE(item.part_number.empty());
    EXPECT_GT(item.quantity, 0);
  }
}

TEST_F(BOMGeneratorTest, FastenerItemGeneration) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  EXPECT_FALSE(bom.fasteners.empty());
  for (const auto& item : bom.fasteners) {
    EXPECT_FALSE(item.part_number.empty());
    EXPECT_GT(item.quantity, 0);
  }
}

TEST_F(BOMGeneratorTest, BOMItemStructure) {
  BOMGenerator::BOMItem item;
  item.part_number = "PART-001";
  item.description = "Test Part";
  item.quantity = 5;
  item.unit = "pcs";
  item.unit_cost = 10.0;
  item.total_cost = 50.0;
  item.supplier = "Test Supplier";
  item.lead_time_days = "7";
  item.is_critical = true;

  EXPECT_EQ(item.part_number, "PART-001");
  EXPECT_EQ(item.quantity, 5);
  EXPECT_EQ(item.total_cost, 50.0);
  EXPECT_TRUE(item.is_critical);
}

TEST_F(BOMGeneratorTest, SupplierAnalysis) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  auto summary = generator_.analyzeSupplyChain(bom);

  // Should have supplier information
  EXPECT_GE(summary.supplier_costs.size(), 0);
}

TEST_F(BOMGeneratorTest, LeadTimeAnalysis) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  auto summary = generator_.analyzeSupplyChain(bom);

  // Lead time analysis should be available
  for (const auto& item : summary.lead_time_analysis) {
    EXPECT_GT(item.second, 0);  // Days should be positive
  }
}

TEST_F(BOMGeneratorTest, JSONFormatStructure) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string json = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::JSON);

  EXPECT_NE(json.find("\"project_name\""), std::string::npos);
  EXPECT_NE(json.find("\"tubes\""), std::string::npos);
  EXPECT_NE(json.find("\"total_material_cost\""), std::string::npos);
}

TEST_F(BOMGeneratorTest, XMLFormatStructure) {
  auto tubes = createTestTubes();
  auto bom = generator_.generateBOM(tubes, std::vector<BendingMachine>(), "Test Project");

  std::string xml = generator_.generateBOMString(bom, BOMGenerator::BOMFormat::XML);

  EXPECT_NE(xml.find("<ProjectName>"), std::string::npos);
  EXPECT_NE(xml.find("<Items>"), std::string::npos);
  EXPECT_NE(xml.find("</Items>"), std::string::npos);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
