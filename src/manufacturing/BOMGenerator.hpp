#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include "ManufacturingPlanner.hpp"

namespace bendcore::manufacturing {

// Bill of Materials (BOM) Generation
class BOMGenerator {
 public:
  enum class BOMFormat {
    CSV,      // Comma-separated values
    JSON,     // JSON format
    EXCEL,    // Excel format (XLSX)
    XML,      // XML format
    PDF       // PDF report
  };

  struct BOMItem {
    std::string part_number;
    std::string description;
    int quantity = 0;
    std::string unit = "pcs";
    double unit_cost = 0.0;
    double total_cost = 0.0;
    std::string supplier;
    std::string lead_time_days;
    std::string warehouse_location;
    bool is_critical = false;  // Critical for production
  };

  struct BOM {
    std::string project_name;
    std::string revision;
    std::string created_date;
    std::string created_by;
    std::vector<BOMItem> tubes;
    std::vector<BOMItem> fittings;
    std::vector<BOMItem> fasteners;
    std::vector<BOMItem> tools;
    std::vector<BOMItem> raw_materials;
    double total_material_cost = 0.0;
    double total_labor_hours = 0.0;
    double estimated_total_cost = 0.0;
  };

  struct ProcurementSummary {
    std::map<std::string, int> supplier_items;  // supplier -> item count
    std::map<std::string, double> supplier_costs;  // supplier -> total cost
    std::vector<std::pair<std::string, int>> lead_time_analysis;  // item -> lead_time_days
    int critical_items = 0;
    double total_lead_time_days = 0;
  };

  BOMGenerator();

  // Generate BOM from tube list
  BOM generateBOM(
    const std::vector<Tube>& tubes,
    const std::vector<BendingMachine>& machines,
    const std::string& project_name
  );

  // Add custom items to BOM
  void addItem(
    BOM& bom,
    const BOMItem& item
  );

  // Group items by category
  void groupItems(BOM& bom);

  // Export BOM to file
  bool exportBOM(
    const BOM& bom,
    const std::string& filename,
    BOMFormat format
  );

  // Generate BOM string (CSV, JSON, XML, etc.)
  std::string generateBOMString(
    const BOM& bom,
    BOMFormat format
  );

  // Calculate costs and totals
  void calculateCosts(
    BOM& bom,
    const std::map<std::string, double>& material_costs,
    const std::map<std::string, double>& labor_rates
  );

  // Generate procurement analysis
  ProcurementSummary analyzeSupplyChain(const BOM& bom);

  // Get supplier recommendations
  std::vector<std::string> getSuppliersForItem(const std::string& part_number);

  // Merge multiple BOMs
  BOM mergeBOMs(
    const std::vector<BOM>& boms,
    const std::string& merged_project_name
  );

 private:
  // BOM export formats
  std::string exportToCSV(const BOM& bom);
  std::string exportToJSON(const BOM& bom);
  std::string exportToXML(const BOM& bom);
  std::string exportToPDF(const BOM& bom);

  // Item generation for standard components
  std::vector<BOMItem> generateTubeItems(const std::vector<Tube>& tubes);
  std::vector<BOMItem> generateFittingItems(
    const std::vector<Tube>& tubes,
    const std::vector<BendingMachine>& machines
  );
  std::vector<BOMItem> generateFastenerItems(const std::vector<Tube>& tubes);

  // Standard part libraries
  struct StandardPart {
    std::string part_number;
    std::string description;
    double unit_cost = 0.0;
    std::string supplier;
    std::string lead_time_days;
  };

  std::map<std::string, StandardPart> part_library_;

  // Load standard part library
  void loadPartLibrary();
};

}  // namespace bendcore::manufacturing
