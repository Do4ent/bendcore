#include "../../include/bendcore/manufacturing/bom.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cmath>

namespace bendcore::manufacturing {

// ============================================================================
// BOM Implementation
// ============================================================================

void BOM::addItem(const BOMItem& item) {
    items.push_back(item);
    invalidateCache();
}

bool BOM::removeItem(const std::string& partNumber) {
    auto it = std::find_if(items.begin(), items.end(),
        [&partNumber](const BOMItem& item) {
            return item.partNumber == partNumber;
        });

    if (it != items.end()) {
        items.erase(it);
        invalidateCache();
        return true;
    }
    return false;
}

BOMItem* BOM::findItem(const std::string& partNumber) {
    auto it = std::find_if(items.begin(), items.end(),
        [&partNumber](BOMItem& item) {
            return item.partNumber == partNumber;
        });
    return it != items.end() ? &(*it) : nullptr;
}

const BOMItem* BOM::findItem(const std::string& partNumber) const {
    auto it = std::find_if(items.begin(), items.end(),
        [&partNumber](const BOMItem& item) {
            return item.partNumber == partNumber;
        });
    return it != items.end() ? &(*it) : nullptr;
}

std::vector<BOMItem*> BOM::getItemsByCategory(const std::string& category) {
    std::vector<BOMItem*> result;
    for (auto& item : items) {
        if (item.category == category) {
            result.push_back(&item);
        }
    }
    return result;
}

bool BOM::updateItem(const std::string& partNumber, const BOMItem& newItem) {
    auto item = findItem(partNumber);
    if (item) {
        *item = newItem;
        invalidateCache();
        return true;
    }
    return false;
}

const BOMStatistics& BOM::getStatistics() const {
    if (!cachedStatistics) {
        cachedStatistics = BOMStatistics();
        auto& stats = cachedStatistics.value();

        // Count items
        stats.totalItems = items.size();
        stats.totalQuantity = 0;
        std::map<std::string, bool> uniqueParts;

        for (const auto& item : items) {
            // Count unique parts
            uniqueParts[item.partNumber] = true;

            // Accumulate quantities
            stats.totalQuantity += item.quantity;
            stats.totalCost += item.totalCost;
            stats.totalWeight += item.weight * item.quantity;

            // By category
            stats.itemsByCategory[item.category]++;
            stats.quantityByCategory[item.category] += item.quantity;
            stats.costByCategory[item.category] += item.totalCost;

            // By material
            if (!item.material.empty()) {
                stats.weightByMaterial[item.material] += item.weight * item.quantity;
            }

            // By supplier
            if (!item.suppliers.empty()) {
                stats.itemsBySupplier[item.suppliers[0].name]++;
                stats.quantityBySupplier[item.suppliers[0].name] += item.quantity;
                stats.costBySupplier[item.suppliers[0].name] += item.totalCost;
            }

            // Critical items
            if (item.isCritical) {
                stats.criticalItemCount++;
                stats.criticalItems.push_back(item.partNumber);
            }

            // Lead time analysis
            if (!item.leadTimeDays.empty()) {
                try {
                    int leadDays = std::stoi(item.leadTimeDays);
                    if (leadDays > stats.maxLeadTimeDays) {
                        stats.maxLeadTimeDays = leadDays;
                    }
                } catch (...) {
                    // Skip invalid lead time values
                }
            }
        }

        stats.uniqueItems = uniqueParts.size();
        stats.supplierCount = stats.itemsBySupplier.size();

        // Calculate average lead time
        if (stats.totalItems > 0 && stats.maxLeadTimeDays > 0) {
            stats.averageLeadTimeDays = static_cast<double>(stats.maxLeadTimeDays) * 0.8;
        }
    }

    return cachedStatistics.value();
}

void BOM::invalidateCache() {
    cachedStatistics.reset();
}

std::vector<BOMItem> BOM::filterByProperty(
    const std::string& propertyKey,
    const std::string& propertyValue) const {

    std::vector<BOMItem> result;

    for (const auto& item : items) {
        for (const auto& prop : item.customProperties) {
            if (prop.key == propertyKey && prop.value == propertyValue) {
                result.push_back(item);
                break;
            }
        }
    }

    return result;
}

double BOM::getTotalCost() const {
    return getStatistics().totalCost;
}

double BOM::getTotalWeight() const {
    return getStatistics().totalWeight;
}

int BOM::getUniquePartCount() const {
    return getStatistics().uniqueItems;
}

bool BOM::hasCriticalItems() const {
    return getStatistics().criticalItemCount > 0;
}

// ============================================================================
// BOMGenerator Implementation
// ============================================================================

std::string BOMGenerator::getCurrentDate() const {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string BOMGenerator::generatePartNumber() {
    std::ostringstream oss;
    oss << partNumberPrefix_ << std::setfill('0') << std::setw(4) << partNumberCounter_ << partNumberSuffix_;
    partNumberCounter_++;
    return oss.str();
}

void BOMGenerator::setPartNumberingScheme(
    const std::string& prefix,
    int startNumber,
    const std::string& suffix) {

    partNumberPrefix_ = prefix;
    partNumberCounter_ = startNumber;
    partNumberSuffix_ = suffix;
}

std::shared_ptr<BOM> BOMGenerator::generateFromTubes(
    const std::string& projectName,
    const std::vector<std::pair<std::string, int>>& tubeSpecs) {

    auto bom = std::make_shared<BOM>();
    bom->projectName = projectName;
    bom->revision = "1.0";
    bom->createdDate = getCurrentDate();
    bom->createdBy = "BendCore CAD System";

    // Generate tube items
    int itemNum = 1;
    std::map<std::string, int> aggregatedQuantities;

    for (const auto& spec : tubeSpecs) {
        aggregatedQuantities[spec.first] += spec.second;
    }

    for (const auto& [spec, totalQty] : aggregatedQuantities) {
        BOMItem item;
        // Use custom part numbering if scheme is set
        if (!partNumberPrefix_.empty()) {
            item.partNumber = generatePartNumber();
        } else {
            item.partNumber = "TUBE-" + std::to_string(itemNum);
        }
        itemNum++;

        item.name = "Tube: " + spec;
        item.description = spec;
        item.category = "tube";
        item.quantity = totalQty;
        item.unit = "pcs";
        item.unitCost = 25.0;
        item.totalCost = item.unitCost * item.quantity;
        item.revision = "1.0";
        item.lastModifiedDate = bom->createdDate;

        // Set a default weight (0.5 kg per piece for steel tube)
        item.weight = 0.5;

        // Parse spec for material info
        auto parts = spec;
        item.material = "Steel";  // Default
        if (parts.find("Aluminum") != std::string::npos) {
            item.material = "Aluminum";
            item.weight = 0.3;  // Lighter than steel
        } else if (parts.find("Stainless") != std::string::npos) {
            item.material = "Stainless Steel";
            item.weight = 0.7;  // Heavier than regular steel
        }

        SupplierInfo supplier;
        supplier.name = "MetalSupplies Inc.";
        supplier.isPrimary = true;
        supplier.unitCost = item.unitCost;
        supplier.leadTimeDays = "7-10";
        item.suppliers.push_back(supplier);

        bom->addItem(item);
    }

    return bom;
}

std::shared_ptr<BOM> BOMGenerator::generateFromAssembly(
    const std::string& projectName,
    const std::string& assemblyData) {

    // This is a simplified implementation
    // In a full implementation, this would parse the assemblyData
    // and traverse the assembly tree from bendcore::assembly::Registry

    auto bom = std::make_shared<BOM>();
    bom->projectName = projectName;
    bom->revision = "1.0";
    bom->createdDate = getCurrentDate();
    bom->createdBy = "BendCore CAD System";

    return bom;
}

std::string BOMGenerator::exportBOM(
    const BOM& bom,
    BOMFormat format) const {

    switch (format) {
        case BOMFormat::CSV:
            return exportToCSV(bom);
        case BOMFormat::JSON:
            return exportToJSON(bom);
        case BOMFormat::XML:
            return exportToXML(bom);
        case BOMFormat::HTML:
            return exportToHTML(bom);
        default:
            return exportToCSV(bom);
    }
}

bool BOMGenerator::exportBOMToFile(
    const BOM& bom,
    const std::string& filename,
    BOMFormat format) const {

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << exportBOM(bom, format);
    file.close();
    return true;
}

std::shared_ptr<BOM> BOMGenerator::mergeBOMs(
    const std::vector<const BOM*>& boms,
    const std::string& mergedProjectName) const {

    auto merged = std::make_shared<BOM>();
    merged->projectName = mergedProjectName;
    merged->revision = "1.0";
    merged->createdDate = getCurrentDate();
    merged->createdBy = "BendCore CAD System";

    for (const auto bom : boms) {
        for (const auto& item : bom->items) {
            merged->addItem(item);
        }
    }

    // Aggregate duplicate parts
    std::map<std::string, BOMItem*> itemMap;
    for (auto& item : merged->items) {
        if (itemMap.find(item.partNumber) != itemMap.end()) {
            itemMap[item.partNumber]->quantity += item.quantity;
            itemMap[item.partNumber]->totalCost += item.totalCost;
        } else {
            itemMap[item.partNumber] = &item;
        }
    }

    // Remove duplicates
    std::vector<BOMItem> uniqueItems;
    for (const auto& [pn, item] : itemMap) {
        uniqueItems.push_back(*item);
    }
    merged->items = uniqueItems;

    return merged;
}

std::shared_ptr<BOMHierarchy> BOMGenerator::createHierarchy(
    const std::vector<BOMItem>& items,
    const std::string& assemblyPartNumber,
    const std::string& assemblyName) const {

    auto hierarchy = std::make_shared<BOMHierarchy>();
    hierarchy->assemblyPartNumber = assemblyPartNumber;
    hierarchy->assemblyName = assemblyName;
    hierarchy->items = items;

    return hierarchy;
}

std::vector<SupplierInfo> BOMGenerator::getSuppliersForCategory(
    const std::string& category) const {

    std::vector<SupplierInfo> suppliers;

    // Return some default suppliers based on category
    if (category == "tube") {
        SupplierInfo s;
        s.name = "MetalSupplies Inc.";
        s.leadTimeDays = "7-10";
        s.website = "www.metalsupplies.com";
        suppliers.push_back(s);
    } else if (category == "fitting") {
        SupplierInfo s;
        s.name = "FittingSupplies Ltd.";
        s.leadTimeDays = "3-5";
        s.website = "www.fittingsupplies.com";
        suppliers.push_back(s);
    } else if (category == "fastener") {
        SupplierInfo s;
        s.name = "FastenerCorp";
        s.leadTimeDays = "2-3";
        s.website = "www.fastenercorp.com";
        suppliers.push_back(s);
    }

    return suppliers;
}

std::string BOMGenerator::formatCost(double cost, const std::string& currency) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << cost << " " << currency;
    return oss.str();
}

std::string BOMGenerator::exportToCSV(const BOM& bom) const {
    std::ostringstream csv;
    csv << std::fixed << std::setprecision(2);

    // Header
    csv << "Project," << bom.projectName << "\n";
    csv << "Revision," << bom.revision << "\n";
    csv << "Created Date," << bom.createdDate << "\n";
    csv << "Created By," << bom.createdBy << "\n";
    csv << "\n";

    // Column headers
    csv << "Part Number,Name,Description,Category,Quantity,Unit,Unit Cost,Total Cost,";
    csv << "Supplier,Lead Time (days),Material,Weight (kg),Revision,Critical,Notes\n";

    // Items
    for (const auto& item : bom.items) {
        csv << item.partNumber << ","
            << item.name << ","
            << item.description << ","
            << item.category << ","
            << item.quantity << ","
            << item.unit << ","
            << item.unitCost << ","
            << item.totalCost << ",";

        if (!item.suppliers.empty()) {
            csv << item.suppliers[0].name;
        }
        csv << ","
            << item.leadTimeDays << ","
            << item.material << ","
            << item.weight << ","
            << item.revision << ","
            << (item.isCritical ? "Yes" : "No") << ",";

        if (!item.notes.empty()) {
            csv << item.notes[0].text;
        }
        csv << "\n";
    }

    // Summary
    const auto& stats = bom.getStatistics();
    csv << "\n,Summary\n";
    csv << "Total Items," << stats.totalItems << "\n";
    csv << "Unique Items," << stats.uniqueItems << "\n";
    csv << "Total Quantity," << stats.totalQuantity << "\n";
    csv << "Total Cost," << stats.totalCost << "\n";
    csv << "Total Weight (kg)," << stats.totalWeight << "\n";

    return csv.str();
}

std::string BOMGenerator::exportToJSON(const BOM& bom) const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);

    json << "{\n";
    json << "  \"projectName\": \"" << bom.projectName << "\",\n";
    json << "  \"revision\": \"" << bom.revision << "\",\n";
    json << "  \"createdDate\": \"" << bom.createdDate << "\",\n";
    json << "  \"createdBy\": \"" << bom.createdBy << "\",\n";
    json << "  \"items\": [\n";

    for (size_t i = 0; i < bom.items.size(); ++i) {
        const auto& item = bom.items[i];
        json << "    {\n";
        json << "      \"partNumber\": \"" << item.partNumber << "\",\n";
        json << "      \"name\": \"" << item.name << "\",\n";
        json << "      \"description\": \"" << item.description << "\",\n";
        json << "      \"category\": \"" << item.category << "\",\n";
        json << "      \"quantity\": " << item.quantity << ",\n";
        json << "      \"unit\": \"" << item.unit << "\",\n";
        json << "      \"unitCost\": " << item.unitCost << ",\n";
        json << "      \"totalCost\": " << item.totalCost << ",\n";
        json << "      \"material\": \"" << item.material << "\",\n";
        json << "      \"weight\": " << item.weight << ",\n";
        json << "      \"isCritical\": " << (item.isCritical ? "true" : "false") << ",\n";
        json << "      \"revision\": \"" << item.revision << "\"\n";
        json << "    }";
        if (i < bom.items.size() - 1) json << ",";
        json << "\n";
    }

    json << "  ],\n";

    const auto& stats = bom.getStatistics();
    json << "  \"statistics\": {\n";
    json << "    \"totalItems\": " << stats.totalItems << ",\n";
    json << "    \"uniqueItems\": " << stats.uniqueItems << ",\n";
    json << "    \"totalQuantity\": " << stats.totalQuantity << ",\n";
    json << "    \"totalCost\": " << stats.totalCost << ",\n";
    json << "    \"totalWeight\": " << stats.totalWeight << ",\n";
    json << "    \"criticalItemCount\": " << stats.criticalItemCount << "\n";
    json << "  }\n";
    json << "}\n";

    return json.str();
}

std::string BOMGenerator::exportToXML(const BOM& bom) const {
    std::ostringstream xml;
    xml << std::fixed << std::setprecision(2);

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<BOM>\n";
    xml << "  <ProjectName>" << bom.projectName << "</ProjectName>\n";
    xml << "  <Revision>" << bom.revision << "</Revision>\n";
    xml << "  <CreatedDate>" << bom.createdDate << "</CreatedDate>\n";
    xml << "  <CreatedBy>" << bom.createdBy << "</CreatedBy>\n";
    xml << "  <Items>\n";

    for (const auto& item : bom.items) {
        xml << "    <Item>\n";
        xml << "      <PartNumber>" << item.partNumber << "</PartNumber>\n";
        xml << "      <Name>" << item.name << "</Name>\n";
        xml << "      <Description>" << item.description << "</Description>\n";
        xml << "      <Category>" << item.category << "</Category>\n";
        xml << "      <Quantity>" << item.quantity << "</Quantity>\n";
        xml << "      <Unit>" << item.unit << "</Unit>\n";
        xml << "      <UnitCost>" << item.unitCost << "</UnitCost>\n";
        xml << "      <TotalCost>" << item.totalCost << "</TotalCost>\n";
        xml << "      <Material>" << item.material << "</Material>\n";
        xml << "      <Weight>" << item.weight << "</Weight>\n";
        xml << "      <IsCritical>" << (item.isCritical ? "true" : "false") << "</IsCritical>\n";

        if (!item.suppliers.empty()) {
            xml << "      <Supplier>\n";
            xml << "        <Name>" << item.suppliers[0].name << "</Name>\n";
            xml << "        <LeadTime>" << item.suppliers[0].leadTimeDays << "</LeadTime>\n";
            xml << "      </Supplier>\n";
        }

        xml << "    </Item>\n";
    }

    xml << "  </Items>\n";

    const auto& stats = bom.getStatistics();
    xml << "  <Statistics>\n";
    xml << "    <TotalItems>" << stats.totalItems << "</TotalItems>\n";
    xml << "    <UniqueItems>" << stats.uniqueItems << "</UniqueItems>\n";
    xml << "    <TotalQuantity>" << stats.totalQuantity << "</TotalQuantity>\n";
    xml << "    <TotalCost>" << stats.totalCost << "</TotalCost>\n";
    xml << "    <TotalWeight>" << stats.totalWeight << "</TotalWeight>\n";
    xml << "  </Statistics>\n";
    xml << "</BOM>\n";

    return xml.str();
}

std::string BOMGenerator::exportToHTML(const BOM& bom) const {
    std::ostringstream html;
    html << std::fixed << std::setprecision(2);

    html << "<!DOCTYPE html>\n";
    html << "<html>\n<head>\n";
    html << "  <title>Bill of Materials - " << bom.projectName << "</title>\n";
    html << "  <style>\n";
    html << "    body { font-family: Arial, sans-serif; margin: 20px; }\n";
    html << "    .header { border-bottom: 2px solid #333; margin-bottom: 20px; padding-bottom: 10px; }\n";
    html << "    table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }\n";
    html << "    th { background-color: #4CAF50; color: white; padding: 10px; text-align: left; }\n";
    html << "    td { border: 1px solid #ddd; padding: 8px; }\n";
    html << "    tr:nth-child(even) { background-color: #f2f2f2; }\n";
    html << "    tr:hover { background-color: #ddd; }\n";
    html << "    .critical { background-color: #ffcccc; }\n";
    html << "    .summary { background-color: #e8f4f8; font-weight: bold; }\n";
    html << "  </style>\n";
    html << "</head>\n<body>\n";

    // Header section
    html << "  <div class=\"header\">\n";
    html << "    <h1>Bill of Materials</h1>\n";
    html << "    <p><strong>Project:</strong> " << bom.projectName << "</p>\n";
    html << "    <p><strong>Revision:</strong> " << bom.revision << "</p>\n";
    html << "    <p><strong>Created:</strong> " << bom.createdDate << " by " << bom.createdBy << "</p>\n";
    html << "  </div>\n\n";

    // Items table
    html << "  <table>\n";
    html << "    <tr>\n";
    html << "      <th>Part Number</th>\n";
    html << "      <th>Name</th>\n";
    html << "      <th>Category</th>\n";
    html << "      <th>Quantity</th>\n";
    html << "      <th>Unit</th>\n";
    html << "      <th>Unit Cost</th>\n";
    html << "      <th>Total Cost</th>\n";
    html << "      <th>Material</th>\n";
    html << "      <th>Weight (kg)</th>\n";
    html << "      <th>Supplier</th>\n";
    html << "      <th>Lead Time</th>\n";
    html << "    </tr>\n";

    for (const auto& item : bom.items) {
        html << "    <tr" << (item.isCritical ? " class=\"critical\"" : "") << ">\n";
        html << "      <td>" << item.partNumber << "</td>\n";
        html << "      <td>" << item.name << "</td>\n";
        html << "      <td>" << item.category << "</td>\n";
        html << "      <td>" << item.quantity << "</td>\n";
        html << "      <td>" << item.unit << "</td>\n";
        html << "      <td>" << item.unitCost << "</td>\n";
        html << "      <td>" << item.totalCost << "</td>\n";
        html << "      <td>" << item.material << "</td>\n";
        html << "      <td>" << item.weight << "</td>\n";
        html << "      <td>";
        if (!item.suppliers.empty()) {
            html << item.suppliers[0].name;
        }
        html << "</td>\n";
        html << "      <td>" << item.leadTimeDays << "</td>\n";
        html << "    </tr>\n";
    }

    html << "  </table>\n\n";

    // Summary section
    const auto& stats = bom.getStatistics();
    html << "  <table>\n";
    html << "    <tr class=\"summary\">\n";
    html << "      <td>Total Items</td>\n";
    html << "      <td>" << stats.totalItems << "</td>\n";
    html << "    </tr>\n";
    html << "    <tr class=\"summary\">\n";
    html << "      <td>Unique Items</td>\n";
    html << "      <td>" << stats.uniqueItems << "</td>\n";
    html << "    </tr>\n";
    html << "    <tr class=\"summary\">\n";
    html << "      <td>Total Quantity</td>\n";
    html << "      <td>" << stats.totalQuantity << "</td>\n";
    html << "    </tr>\n";
    html << "    <tr class=\"summary\">\n";
    html << "      <td>Total Cost</td>\n";
    html << "      <td>$" << stats.totalCost << "</td>\n";
    html << "    </tr>\n";
    html << "    <tr class=\"summary\">\n";
    html << "      <td>Total Weight (kg)</td>\n";
    html << "      <td>" << stats.totalWeight << "</td>\n";
    html << "    </tr>\n";
    html << "  </table>\n";

    html << "</body>\n</html>\n";

    return html.str();
}

}  // namespace bendcore::manufacturing
