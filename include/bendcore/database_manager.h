#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace bendcore::database {

// Material database
struct Material {
    std::string id;
    std::string name;
    std::string type;           // steel, aluminum, copper, titanium, composite
    double density;             // g/cm³
    double youngModulus;        // GPa
    double yieldStrength;       // MPa
    double ultimateStrength;    // MPa
    double elongationAtBreak;   // %
    double pricePerKg;          // currency
    std::string supplier;
    bool inStock;
};

class MaterialDatabase {
public:
    static MaterialDatabase& getInstance();

    void addMaterial(const Material& material);
    std::optional<Material> getMaterial(const std::string& id);
    std::vector<Material> getAllMaterials();
    std::vector<Material> getMaterialsByType(const std::string& type);
    std::vector<Material> getMaterialsInStock();

private:
    std::map<std::string, Material> materials_;
    void initializeDefaults();
};

// Machine database
struct Machine {
    std::string id;
    std::string manufacturer;
    std::string model;
    std::string machineType;    // bender, cutter, welder, etc.
    double maxBendDiameter;     // mm
    double maxBendForce;        // N
    double maxBendRadius;       // mm
    double maxBendAngle;        // degrees
    double workspaceX;          // mm
    double workspaceY;          // mm
    double workspaceZ;          // mm
    double costPerHour;         // currency
    bool isAvailable;
};

class MachineDatabase {
public:
    static MachineDatabase& getInstance();

    void addMachine(const Machine& machine);
    std::optional<Machine> getMachine(const std::string& id);
    std::vector<Machine> getAllMachines();
    std::vector<Machine> getAvailableMachines();
    std::vector<Machine> getMachinesByType(const std::string& type);
    std::vector<Machine> getMachinesForCapabilities(
        double requiredDiameter,
        double requiredForce,
        double requiredRadius
    );

private:
    std::map<std::string, Machine> machines_;
    void initializeDefaults();
};

// Template database
struct PipeTemplate {
    std::string id;
    std::string name;
    std::string description;
    double tubeDiameter;        // mm
    double wallThickness;       // mm
    double totalLength;         // mm
    std::string material;
    std::vector<double> bendAngles;
    std::vector<double> bendRadii;
    std::vector<double> bendPositions;
    double estimatedCost;       // currency
    std::string category;       // common, custom, standard
};

class TemplateDatabase {
public:
    static TemplateDatabase& getInstance();

    void addTemplate(const PipeTemplate& tmpl);
    std::optional<PipeTemplate> getTemplate(const std::string& id);
    std::vector<PipeTemplate> getAllTemplates();
    std::vector<PipeTemplate> getTemplatesByCategory(const std::string& category);
    std::vector<PipeTemplate> findSimilarTemplates(
        double diameter,
        double wallThickness,
        unsigned int bendCount
    );

private:
    std::map<std::string, PipeTemplate> templates_;
    void initializeDefaults();
};

// Supplier database
struct Supplier {
    std::string id;
    std::string name;
    std::string type;           // material, tools, services
    std::string country;
    std::string email;
    std::string phone;
    double leadTimeInDays;
    bool acceptsSmallOrders;
    std::vector<std::string> specialties;
};

class SupplierDatabase {
public:
    static SupplierDatabase& getInstance();

    void addSupplier(const Supplier& supplier);
    std::optional<Supplier> getSupplier(const std::string& id);
    std::vector<Supplier> getAllSuppliers();
    std::vector<Supplier> getSuppliersByType(const std::string& type);
    std::vector<Supplier> getSuppliersBySpecialty(const std::string& specialty);

private:
    std::map<std::string, Supplier> suppliers_;
    void initializeDefaults();
};

} // namespace bendcore::database
