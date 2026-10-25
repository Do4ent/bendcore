#include "BOMGenerator.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace bendcore::manufacturing {

BOMGenerator::BOMGenerator() {
  loadPartLibrary();
}

BOMGenerator::BOM BOMGenerator::generateBOM(
    const std::vector<Tube>& tubes,
    const std::vector<BendingMachine>& machines,
    const std::string& project_name) {

  BOM bom;
  bom.project_name = project_name;
  bom.revision = "1.0";

  // Get current date
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d");
  bom.created_date = oss.str();
  bom.created_by = "BendCore CAD System";

  // Generate tube items
  bom.tubes = generateTubeItems(tubes);

  // Generate fitting items
  bom.fittings = generateFittingItems(tubes, machines);

  // Generate fastener items
  bom.fasteners = generateFastenerItems(tubes);

  // Group items
  groupItems(bom);

  return bom;
}

void BOMGenerator::addItem(BOM& bom, const BOMItem& item) {
  // Determine category and add to appropriate list
  if (item.description.find("Tube") != std::string::npos) {
    bom.tubes.push_back(item);
  } else if (item.description.find("Fitting") != std::string::npos) {
    bom.fittings.push_back(item);
  } else if (item.description.find("Fastener") != std::string::npos ||
             item.description.find("Bolt") != std::string::npos) {
    bom.fasteners.push_back(item);
  } else if (item.description.find("Tool") != std::string::npos) {
    bom.tools.push_back(item);
  } else {
    bom.raw_materials.push_back(item);
  }
}

void BOMGenerator::groupItems(BOM& bom) {
  // Group tubes by material
  std::map<std::string, std::vector<BOMItem*>> grouped;

  for (auto& item : bom.tubes) {
    grouped[item.description].push_back(&item);
  }

  // Consolidate duplicates
  for (auto& group : grouped) {
    if (group.second.size() > 1) {
      BOMItem& first = *group.second[0];
      for (size_t i = 1; i < group.second.size(); ++i) {
        first.quantity += group.second[i]->quantity;
        first.total_cost += group.second[i]->total_cost;
      }
    }
  }
}

bool BOMGenerator::exportBOM(
    const BOM& bom,
    const std::string& filename,
    BOMFormat format) {

  std::string content = generateBOMString(bom, format);

  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  file << content;
  file.close();
  return true;
}

std::string BOMGenerator::generateBOMString(
    const BOM& bom,
    BOMFormat format) {

  switch (format) {
    case BOMFormat::CSV:
      return exportToCSV(bom);
    case BOMFormat::JSON:
      return exportToJSON(bom);
    case BOMFormat::XML:
      return exportToXML(bom);
    case BOMFormat::PDF:
      return exportToPDF(bom);
    default:
      return exportToCSV(bom);
  }
}

void BOMGenerator::calculateCosts(
    BOM& bom,
    const std::map<std::string, double>& material_costs,
    const std::map<std::string, double>& labor_rates) {

  // Calculate tube costs
  double tube_cost = 0.0;
  for (auto& item : bom.tubes) {
    auto cost_it = material_costs.find(item.description);
    if (cost_it != material_costs.end()) {
      item.unit_cost = cost_it->second;
      item.total_cost = item.unit_cost * item.quantity;
      tube_cost += item.total_cost;
    }
  }

  // Calculate fitting costs
  for (auto& item : bom.fittings) {
    auto cost_it = material_costs.find(item.part_number);
    if (cost_it != material_costs.end()) {
      item.unit_cost = cost_it->second;
      item.total_cost = item.unit_cost * item.quantity;
    }
  }

  bom.total_material_cost = tube_cost;

  // Calculate labor cost
  for (auto& labor : labor_rates) {
    bom.total_labor_hours += labor.second;
  }

  bom.estimated_total_cost = bom.total_material_cost +
                             (bom.total_labor_hours * 50.0);  // $50/hour base rate
}

BOMGenerator::ProcurementSummary BOMGenerator::analyzeSupplyChain(
    const BOM& bom) {

  ProcurementSummary summary;

  // Analyze suppliers
  for (const auto& item : bom.tubes) {
    summary.supplier_items[item.supplier]++;
    summary.supplier_costs[item.supplier] += item.total_cost;
  }

  for (const auto& item : bom.fittings) {
    summary.supplier_items[item.supplier]++;
    summary.supplier_costs[item.supplier] += item.total_cost;
  }

  // Analyze lead times
  for (const auto& item : bom.tubes) {
    if (!item.lead_time_days.empty()) {
      int days = std::stoi(item.lead_time_days);
      summary.lead_time_analysis.push_back({item.part_number, days});
      if (item.is_critical) {
        summary.critical_items++;
      }
    }
  }

  return summary;
}

std::vector<std::string> BOMGenerator::getSuppliersForItem(
    const std::string& part_number) {

  std::vector<std::string> suppliers;

  auto it = part_library_.find(part_number);
  if (it != part_library_.end()) {
    suppliers.push_back(it->second.supplier);
  }

  return suppliers;
}

BOMGenerator::BOM BOMGenerator::mergeBOMs(
    const std::vector<BOM>& boms,
    const std::string& merged_project_name) {

  BOM merged;
  merged.project_name = merged_project_name;
  merged.revision = "1.0";

  for (const auto& bom : boms) {
    merged.tubes.insert(merged.tubes.end(), bom.tubes.begin(), bom.tubes.end());
    merged.fittings.insert(merged.fittings.end(), bom.fittings.begin(), bom.fittings.end());
    merged.fasteners.insert(merged.fasteners.end(), bom.fasteners.begin(), bom.fasteners.end());
    merged.tools.insert(merged.tools.end(), bom.tools.begin(), bom.tools.end());
    merged.raw_materials.insert(merged.raw_materials.end(), bom.raw_materials.begin(), bom.raw_materials.end());

    merged.total_material_cost += bom.total_material_cost;
    merged.total_labor_hours += bom.total_labor_hours;
    merged.estimated_total_cost += bom.estimated_total_cost;
  }

  return merged;
}

std::string BOMGenerator::exportToCSV(const BOM& bom) {
  std::ostringstream csv;
  csv << std::fixed << std::setprecision(2);

  csv << "Project: " << bom.project_name << "\n";
  csv << "Revision: " << bom.revision << "\n";
  csv << "Date: " << bom.created_date << "\n";
  csv << "Created By: " << bom.created_by << "\n\n";

  csv << "Part Number,Description,Quantity,Unit,Unit Cost,Total Cost,Supplier,Lead Time,Location,Critical\n";

  for (const auto& item : bom.tubes) {
    csv << item.part_number << ","
        << item.description << ","
        << item.quantity << ","
        << item.unit << ","
        << item.unit_cost << ","
        << item.total_cost << ","
        << item.supplier << ","
        << item.lead_time_days << ","
        << item.warehouse_location << ","
        << (item.is_critical ? "Yes" : "No") << "\n";
  }

  csv << "\n,Subtotal Material Cost,," << bom.total_material_cost << "\n";
  csv << ",Total Labor Hours,," << bom.total_labor_hours << "\n";
  csv << ",Estimated Total Cost,," << bom.estimated_total_cost << "\n";

  return csv.str();
}

std::string BOMGenerator::exportToJSON(const BOM& bom) {
  std::ostringstream json;
  json << std::fixed << std::setprecision(2);

  json << "{\n";
  json << "  \"project_name\": \"" << bom.project_name << "\",\n";
  json << "  \"revision\": \"" << bom.revision << "\",\n";
  json << "  \"created_date\": \"" << bom.created_date << "\",\n";
  json << "  \"created_by\": \"" << bom.created_by << "\",\n";
  json << "  \"tubes\": [\n";

  for (size_t i = 0; i < bom.tubes.size(); ++i) {
    const auto& item = bom.tubes[i];
    json << "    {\n";
    json << "      \"part_number\": \"" << item.part_number << "\",\n";
    json << "      \"description\": \"" << item.description << "\",\n";
    json << "      \"quantity\": " << item.quantity << ",\n";
    json << "      \"unit\": \"" << item.unit << "\",\n";
    json << "      \"unit_cost\": " << item.unit_cost << ",\n";
    json << "      \"total_cost\": " << item.total_cost << "\n";
    json << "    }";
    if (i < bom.tubes.size() - 1) json << ",";
    json << "\n";
  }

  json << "  ],\n";
  json << "  \"total_material_cost\": " << bom.total_material_cost << ",\n";
  json << "  \"total_labor_hours\": " << bom.total_labor_hours << ",\n";
  json << "  \"estimated_total_cost\": " << bom.estimated_total_cost << "\n";
  json << "}\n";

  return json.str();
}

std::string BOMGenerator::exportToXML(const BOM& bom) {
  std::ostringstream xml;
  xml << std::fixed << std::setprecision(2);

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml << "<BOM>\n";
  xml << "  <ProjectName>" << bom.project_name << "</ProjectName>\n";
  xml << "  <Revision>" << bom.revision << "</Revision>\n";
  xml << "  <CreatedDate>" << bom.created_date << "</CreatedDate>\n";
  xml << "  <CreatedBy>" << bom.created_by << "</CreatedBy>\n";
  xml << "  <Items>\n";

  for (const auto& item : bom.tubes) {
    xml << "    <Item>\n";
    xml << "      <PartNumber>" << item.part_number << "</PartNumber>\n";
    xml << "      <Description>" << item.description << "</Description>\n";
    xml << "      <Quantity>" << item.quantity << "</Quantity>\n";
    xml << "      <UnitCost>" << item.unit_cost << "</UnitCost>\n";
    xml << "      <TotalCost>" << item.total_cost << "</TotalCost>\n";
    xml << "      <Supplier>" << item.supplier << "</Supplier>\n";
    xml << "    </Item>\n";
  }

  xml << "  </Items>\n";
  xml << "  <TotalMaterialCost>" << bom.total_material_cost << "</TotalMaterialCost>\n";
  xml << "  <TotalLaborHours>" << bom.total_labor_hours << "</TotalLaborHours>\n";
  xml << "  <EstimatedTotalCost>" << bom.estimated_total_cost << "</EstimatedTotalCost>\n";
  xml << "</BOM>\n";

  return xml.str();
}

std::string BOMGenerator::exportToPDF(const BOM& bom) {
  // Simplified PDF export as text (actual PDF would need a library)
  return "PDF export not fully implemented\n";
}

std::vector<BOMGenerator::BOMItem> BOMGenerator::generateTubeItems(
    const std::vector<Tube>& tubes) {

  std::vector<BOMItem> items;
  std::map<std::string, int> material_counts;

  for (const auto& tube : tubes) {
    std::string key = tube.material + "_" + std::to_string(static_cast<int>(tube.outer_diameter)) +
                      "x" + std::to_string(static_cast<int>(tube.wall_thickness));
    material_counts[key]++;
  }

  int item_num = 1;
  for (const auto& pair : material_counts) {
    BOMItem item;
    item.part_number = "TUBE-" + std::to_string(item_num++);
    item.description = pair.first;
    item.quantity = pair.second;
    item.unit = "pcs";
    item.unit_cost = 25.0 * pair.second;
    item.total_cost = item.unit_cost * item.quantity;
    item.supplier = "MetalSupplies Inc.";
    item.lead_time_days = "7-10";
    item.is_critical = true;
    items.push_back(item);
  }

  return items;
}

std::vector<BOMGenerator::BOMItem> BOMGenerator::generateFittingItems(
    const std::vector<Tube>& tubes,
    const std::vector<BendingMachine>& machines) {

  std::vector<BOMItem> items;

  // Add standard fittings
  for (const auto& tube : tubes) {
    int fitting_count = std::max(1, static_cast<int>(tube.bends.size()) / 2);

    BOMItem item;
    item.part_number = "FIT-ELBOW-45";
    item.description = "45 Degree Elbow Fitting";
    item.quantity = fitting_count;
    item.unit = "pcs";
    item.unit_cost = 5.0;
    item.total_cost = item.unit_cost * item.quantity;
    item.supplier = "FittingSupplies Ltd.";
    item.lead_time_days = "3-5";
    items.push_back(item);
  }

  return items;
}

std::vector<BOMGenerator::BOMItem> BOMGenerator::generateFastenerItems(
    const std::vector<Tube>& tubes) {

  std::vector<BOMItem> items;

  BOMItem item;
  item.part_number = "BOLT-M10";
  item.description = "Bolt M10 x 30 Grade 8.8";
  item.quantity = tubes.size() * 4;  // 4 bolts per tube
  item.unit = "pcs";
  item.unit_cost = 0.50;
  item.total_cost = item.unit_cost * item.quantity;
  item.supplier = "FastenerCorp";
  item.lead_time_days = "2-3";
  items.push_back(item);

  return items;
}

void BOMGenerator::loadPartLibrary() {
  // Load standard parts into library
  part_library_["TUBE-STEEL-12x1"] = {
    "TUBE-001", "Steel Tube 12mm OD x 1mm WT", 2.50, "MetalSupplies Inc.", "7-10"
  };

  part_library_["FIT-ELBOW-45"] = {
    "FIT-001", "45 Degree Elbow Fitting", 5.00, "FittingSupplies Ltd.", "3-5"
  };

  part_library_["BOLT-M10"] = {
    "BOLT-001", "Bolt M10 x 30 Grade 8.8", 0.50, "FastenerCorp", "2-3"
  };
}

}  // namespace bendcore::manufacturing
