#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <cstdint>

namespace bendcore::manufacturing {

/**
 * @brief Bill of Materials (BOM) system for assembly manufacturing
 * Supports hierarchical BOMs with assembly structure preservation
 */

// Forward declarations
class BOM;
struct BOMItem;
struct BOMHierarchy;
struct BOMStatistics;

/**
 * @brief Supplier information
 */
struct SupplierInfo {
    std::string name;
    std::string partNumber;
    double unitCost = 0.0;
    std::string leadTimeDays;
    std::string website;
    std::string contactEmail;
    bool isPrimary = false;
};

/**
 * @brief Custom property for BOM items
 */
struct CustomProperty {
    std::string key;
    std::string value;
    std::string type;  // "string", "number", "boolean"
};

/**
 * @brief Manufacturing notes/comments
 */
struct ManufacturingNote {
    std::string text;
    std::string category;  // "assembly", "machining", "quality", "safety"
    bool isCritical = false;
    std::string createdDate;
};

/**
 * @brief Individual BOM item
 */
struct BOMItem {
    // Identification
    std::string partNumber;
    std::string name;
    std::string description;
    std::string category;  // "tube", "fitting", "fastener", "tool", "raw_material"

    // Quantity and unit
    int quantity = 1;
    std::string unit;  // "pcs", "meters", "kg", etc.

    // Cost information
    double unitCost = 0.0;
    double totalCost = 0.0;
    std::string currency = "USD";

    // Supplier and logistics
    std::vector<SupplierInfo> suppliers;
    std::string warehouse_location;
    std::string leadTimeDays;

    // Properties
    std::string material;
    double weight = 0.0;  // kg
    double volume = 0.0;  // cubic units
    std::string color;
    std::string finish;

    // Status
    bool isCritical = false;
    bool isSubcontracted = false;

    // Custom properties
    std::vector<CustomProperty> customProperties;

    // Manufacturing
    std::vector<ManufacturingNote> notes;

    // Hierarchy
    int hierarchyLevel = 0;
    std::string parentPartNumber;

    // Revision tracking
    std::string revision;
    std::string lastModifiedDate;
};

/**
 * @brief BOM hierarchy node for subassemblies
 */
struct BOMHierarchy {
    std::string assemblyPartNumber;
    std::string assemblyName;
    std::vector<BOMItem> items;
    std::vector<std::shared_ptr<BOMHierarchy>> subassemblies;
    int quantity = 1;
};

/**
 * @brief BOM statistics and analysis
 */
struct BOMStatistics {
    // Counts
    int totalItems = 0;
    int uniqueItems = 0;
    int totalQuantity = 0;

    // By category
    std::map<std::string, int> itemsByCategory;
    std::map<std::string, int> quantityByCategory;

    // Cost analysis
    double totalCost = 0.0;
    double totalMaterialCost = 0.0;
    double totalLaborCost = 0.0;
    std::map<std::string, double> costByCategory;
    std::map<std::string, double> costBySupplier;

    // Weight analysis
    double totalWeight = 0.0;
    std::map<std::string, double> weightByMaterial;

    // Supplier analysis
    int supplierCount = 0;
    std::map<std::string, int> itemsBySupplier;
    std::map<std::string, int> quantityBySupplier;

    // Critical items
    int criticalItemCount = 0;
    std::vector<std::string> criticalItems;

    // Lead time analysis
    int maxLeadTimeDays = 0;
    double averageLeadTimeDays = 0.0;
};

/**
 * @brief Complete Bill of Materials
 */
class BOM {
public:
    // Project information
    std::string projectName;
    std::string projectNumber;
    std::string revision;
    std::string createdDate;
    std::string createdBy;
    std::string lastModifiedDate;
    std::string description;

    // BOM structure
    std::vector<BOMItem> items;
    std::shared_ptr<BOMHierarchy> hierarchy;

    // Metadata
    std::map<std::string, std::string> metadata;

    // Statistics (calculated on demand)
    mutable std::optional<BOMStatistics> cachedStatistics;

    /**
     * @brief Add item to BOM
     */
    void addItem(const BOMItem& item);

    /**
     * @brief Remove item from BOM
     */
    bool removeItem(const std::string& partNumber);

    /**
     * @brief Find item by part number
     */
    BOMItem* findItem(const std::string& partNumber);
    const BOMItem* findItem(const std::string& partNumber) const;

    /**
     * @brief Get all items in category
     */
    std::vector<BOMItem*> getItemsByCategory(const std::string& category);

    /**
     * @brief Update item
     */
    bool updateItem(const std::string& partNumber, const BOMItem& newItem);

    /**
     * @brief Calculate statistics
     */
    const BOMStatistics& getStatistics() const;

    /**
     * @brief Clear cached statistics (called when BOM is modified)
     */
    void invalidateCache();

    /**
     * @brief Filter BOM by property
     */
    std::vector<BOMItem> filterByProperty(
        const std::string& propertyKey,
        const std::string& propertyValue
    ) const;

    /**
     * @brief Get total cost
     */
    double getTotalCost() const;

    /**
     * @brief Get total weight
     */
    double getTotalWeight() const;

    /**
     * @brief Get unique part count
     */
    int getUniquePartCount() const;

    /**
     * @brief Check if BOM has any critical items
     */
    bool hasCriticalItems() const;
};

/**
 * @brief BOM generation from assembly structures
 */
class BOMGenerator {
public:
    enum class BOMFormat {
        CSV,
        JSON,
        XML,
        HTML
    };

    BOMGenerator() = default;
    virtual ~BOMGenerator() = default;

    /**
     * @brief Generate BOM from a flat list of tubes (simple assembly)
     */
    std::shared_ptr<BOM> generateFromTubes(
        const std::string& projectName,
        const std::vector<std::pair<std::string, int>>& tubeSpecs  // {material_od_wt, quantity}
    );

    /**
     * @brief Generate BOM from assembly registry
     * Integrates with bendcore::assembly::Registry for hierarchy support
     */
    std::shared_ptr<BOM> generateFromAssembly(
        const std::string& projectName,
        const std::string& assemblyData  // Serialized assembly structure
    );

    /**
     * @brief Export BOM to specified format
     */
    std::string exportBOM(
        const BOM& bom,
        BOMFormat format
    ) const;

    /**
     * @brief Export BOM to file
     */
    bool exportBOMToFile(
        const BOM& bom,
        const std::string& filename,
        BOMFormat format
    ) const;

    /**
     * @brief Merge multiple BOMs
     */
    std::shared_ptr<BOM> mergeBOMs(
        const std::vector<const BOM*>& boms,
        const std::string& mergedProjectName
    ) const;

    /**
     * @brief Create BOM hierarchy from flat items list
     */
    std::shared_ptr<BOMHierarchy> createHierarchy(
        const std::vector<BOMItem>& items,
        const std::string& assemblyPartNumber,
        const std::string& assemblyName
    ) const;

    /**
     * @brief Add custom part numbering scheme
     */
    void setPartNumberingScheme(
        const std::string& prefix,
        int startNumber = 1,
        const std::string& suffix = ""
    );

    /**
     * @brief Get available suppliers for a part category
     */
    std::vector<SupplierInfo> getSuppliersForCategory(
        const std::string& category
    ) const;

protected:
    std::string partNumberPrefix_;
    int partNumberCounter_ = 1;
    std::string partNumberSuffix_;

    // Format-specific export methods
    virtual std::string exportToCSV(const BOM& bom) const;
    virtual std::string exportToJSON(const BOM& bom) const;
    virtual std::string exportToXML(const BOM& bom) const;
    virtual std::string exportToHTML(const BOM& bom) const;

    // Helper methods
    std::string formatCost(double cost, const std::string& currency) const;
    std::string getCurrentDate() const;
    std::string generatePartNumber();
};

}  // namespace bendcore::manufacturing
