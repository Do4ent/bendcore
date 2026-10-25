#include "bendcore/database_manager.h"
#include <algorithm>

namespace bendcore::database {

// Material Database Implementation
MaterialDatabase& MaterialDatabase::getInstance() {
    static MaterialDatabase instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initializeDefaults();
        initialized = true;
    }
    return instance;
}

void MaterialDatabase::initializeDefaults() {
    materials_["steel_mild"] = {
        "steel_mild", "Mild Steel", "steel", 7.85, 210, 250, 400, 25.0, 0.45, "Generic", true
    };
    materials_["aluminum_6061"] = {
        "aluminum_6061", "Aluminum 6061-T6", "aluminum", 2.70, 69, 240, 290, 18.0, 2.50, "Generic", true
    };
    materials_["copper_pure"] = {
        "copper_pure", "Pure Copper", "copper", 8.96, 130, 200, 220, 35.0, 8.50, "Generic", false
    };
    materials_["titanium_gr2"] = {
        "titanium_gr2", "Titanium Grade 2", "titanium", 4.51, 103, 880, 930, 18.0, 15.0, "Specialty", false
    };
}

void MaterialDatabase::addMaterial(const Material& material) {
    materials_[material.id] = material;
}

std::optional<Material> MaterialDatabase::getMaterial(const std::string& id) {
    auto it = materials_.find(id);
    if (it != materials_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Material> MaterialDatabase::getAllMaterials() {
    std::vector<Material> result;
    for (const auto& pair : materials_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<Material> MaterialDatabase::getMaterialsByType(const std::string& type) {
    std::vector<Material> result;
    for (const auto& pair : materials_) {
        if (pair.second.type == type) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<Material> MaterialDatabase::getMaterialsInStock() {
    std::vector<Material> result;
    for (const auto& pair : materials_) {
        if (pair.second.inStock) {
            result.push_back(pair.second);
        }
    }
    return result;
}

// Machine Database Implementation
MachineDatabase& MachineDatabase::getInstance() {
    static MachineDatabase instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initializeDefaults();
        initialized = true;
    }
    return instance;
}

void MachineDatabase::initializeDefaults() {
    machines_["cnc_bender_1"] = {
        "cnc_bender_1", "Schwer", "TB240", "bender",
        50.0, 200000.0, 150.0, 180.0,
        3000, 2000, 1500, 150.0, true
    };
    machines_["rotary_draw_1"] = {
        "rotary_draw_1", "Emerson", "BendPro 3000", "bender",
        42.0, 180000.0, 120.0, 180.0,
        2500, 1800, 1200, 120.0, true
    };
}

void MachineDatabase::addMachine(const Machine& machine) {
    machines_[machine.id] = machine;
}

std::optional<Machine> MachineDatabase::getMachine(const std::string& id) {
    auto it = machines_.find(id);
    if (it != machines_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Machine> MachineDatabase::getAllMachines() {
    std::vector<Machine> result;
    for (const auto& pair : machines_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<Machine> MachineDatabase::getAvailableMachines() {
    std::vector<Machine> result;
    for (const auto& pair : machines_) {
        if (pair.second.isAvailable) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<Machine> MachineDatabase::getMachinesByType(const std::string& type) {
    std::vector<Machine> result;
    for (const auto& pair : machines_) {
        if (pair.second.machineType == type) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<Machine> MachineDatabase::getMachinesForCapabilities(
    double requiredDiameter,
    double requiredForce,
    double requiredRadius)
{
    std::vector<Machine> result;
    for (const auto& pair : machines_) {
        const auto& m = pair.second;
        if (m.isAvailable &&
            m.maxBendDiameter >= requiredDiameter &&
            m.maxBendForce >= requiredForce &&
            m.maxBendRadius >= requiredRadius) {
            result.push_back(m);
        }
    }
    return result;
}

// Template Database Implementation
TemplateDatabase& TemplateDatabase::getInstance() {
    static TemplateDatabase instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initializeDefaults();
        initialized = true;
    }
    return instance;
}

void TemplateDatabase::initializeDefaults() {
    templates_["standard_90_elbow"] = {
        "standard_90_elbow", "Standard 90° Elbow", "Common 90 degree tube bend",
        12.0, 1.5, 100.0, "steel",
        {90.0}, {18.0}, {50.0},
        15.50, "common"
    };
    templates_["u_bend_pipe"] = {
        "u_bend_pipe", "U-Bend Assembly", "Classic U-shaped tube assembly",
        16.0, 2.0, 200.0, "steel",
        {90.0, 90.0}, {24.0, 24.0}, {50.0, 150.0},
        45.00, "common"
    };
    templates_["custom_race_seat"] = {
        "custom_race_seat", "Race Seat Frame", "Custom performance frame",
        25.0, 2.5, 500.0, "aluminum",
        {45.0, 90.0, 45.0}, {35.0, 50.0, 30.0}, {100.0, 250.0, 400.0},
        250.00, "custom"
    };
}

void TemplateDatabase::addTemplate(const PipeTemplate& tmpl) {
    templates_[tmpl.id] = tmpl;
}

std::optional<PipeTemplate> TemplateDatabase::getTemplate(const std::string& id) {
    auto it = templates_.find(id);
    if (it != templates_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PipeTemplate> TemplateDatabase::getAllTemplates() {
    std::vector<PipeTemplate> result;
    for (const auto& pair : templates_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<PipeTemplate> TemplateDatabase::getTemplatesByCategory(const std::string& category) {
    std::vector<PipeTemplate> result;
    for (const auto& pair : templates_) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<PipeTemplate> TemplateDatabase::findSimilarTemplates(
    double diameter,
    double wallThickness,
    unsigned int bendCount)
{
    std::vector<PipeTemplate> result;
    const double diameterTolerance = 5.0;  // mm
    const double wallTolerance = 0.5;      // mm

    for (const auto& pair : templates_) {
        const auto& t = pair.second;
        if (std::abs(t.tubeDiameter - diameter) < diameterTolerance &&
            std::abs(t.wallThickness - wallThickness) < wallTolerance &&
            t.bendAngles.size() == bendCount) {
            result.push_back(t);
        }
    }
    return result;
}

// Supplier Database Implementation
SupplierDatabase& SupplierDatabase::getInstance() {
    static SupplierDatabase instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initializeDefaults();
        initialized = true;
    }
    return instance;
}

void SupplierDatabase::initializeDefaults() {
    suppliers_["steel_supplier_1"] = {
        "steel_supplier_1", "ArcelorMittal", "material", "Luxembourg",
        "sales@arcelormittal.com", "+352-1234-5678", 7.0, false,
        {"steel", "tubes", "pipes"}
    };
    suppliers_["bending_services_1"] = {
        "bending_services_1", "TubeLand Inc.", "services", "USA",
        "contact@tubeland.com", "+1-800-555-0100", 3.0, true,
        {"bending", "custom-shapes", "small-batches"}
    };
}

void SupplierDatabase::addSupplier(const Supplier& supplier) {
    suppliers_[supplier.id] = supplier;
}

std::optional<Supplier> SupplierDatabase::getSupplier(const std::string& id) {
    auto it = suppliers_.find(id);
    if (it != suppliers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Supplier> SupplierDatabase::getAllSuppliers() {
    std::vector<Supplier> result;
    for (const auto& pair : suppliers_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<Supplier> SupplierDatabase::getSuppliersByType(const std::string& type) {
    std::vector<Supplier> result;
    for (const auto& pair : suppliers_) {
        if (pair.second.type == type) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<Supplier> SupplierDatabase::getSuppliersBySpecialty(const std::string& specialty) {
    std::vector<Supplier> result;
    for (const auto& pair : suppliers_) {
        const auto& s = pair.second;
        auto it = std::find(s.specialties.begin(), s.specialties.end(), specialty);
        if (it != s.specialties.end()) {
            result.push_back(s);
        }
    }
    return result;
}

} // namespace bendcore::database
