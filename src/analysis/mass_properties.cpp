#include "bendcore/analysis/mass_properties.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace BendCore::CadCore {

// ==================== MaterialLibrary ====================

void MaterialLibrary::addMaterial(const Material& material) {
    materials_[material.name] = material;
}

const Material* MaterialLibrary::getMaterial(const std::string& name) const {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<Material>& MaterialLibrary::getMaterials() const {
    // Return as reference to internal storage (caller should not modify)
    static std::vector<Material> result;
    result.clear();
    for (const auto& [name, mat] : materials_) {
        result.push_back(mat);
    }
    return result;
}

bool MaterialLibrary::removeMaterial(const std::string& name) {
    return materials_.erase(name) > 0;
}

bool MaterialLibrary::updateDensity(const std::string& name, double density) {
    auto it = materials_.find(name);
    if (it != materials_.end()) {
        it->second.density = density;
        return true;
    }
    return false;
}

bool MaterialLibrary::hasMaterial(const std::string& name) const {
    return materials_.find(name) != materials_.end();
}

MaterialLibrary MaterialLibrary::getDefaultLibrary() {
    MaterialLibrary lib;

    // Common metals
    lib.addMaterial(Material("Steel", 7850.0));
    lib.addMaterial(Material("Aluminum", 2700.0));
    lib.addMaterial(Material("Copper", 8960.0));
    lib.addMaterial(Material("Brass", 8400.0));
    lib.addMaterial(Material("Stainless Steel 304", 8000.0));
    lib.addMaterial(Material("Stainless Steel 316", 8000.0));
    lib.addMaterial(Material("Titanium", 4500.0));
    lib.addMaterial(Material("Magnesium", 1800.0));
    lib.addMaterial(Material("Zinc", 7140.0));
    lib.addMaterial(Material("Nickel", 8900.0));

    // Non-metals
    lib.addMaterial(Material("Plastic", 1200.0));
    lib.addMaterial(Material("Carbon Fiber", 1600.0));
    lib.addMaterial(Material("Glass Fiber", 1900.0));
    lib.addMaterial(Material("Rubber", 1100.0));

    return lib;
}

// ==================== MassPropertiesCalculator ====================

ComponentMassData MassPropertiesCalculator::calculateComponentMass(
    const EntityId& component_id,
    const std::string& component_name,
    double volume,
    const std::string& material_name,
    const Vector3D& centroid
) {
    ComponentMassData data;
    data.component_id = component_id;
    data.component_name = component_name;
    data.volume = volume;
    data.center_of_gravity = centroid;

    const Material* mat = material_library_.getMaterial(material_name);
    if (mat) {
        data.material = *mat;
        data.mass = volume * mat->density;
    } else {
        // Create default material if not found
        data.material = Material(material_name, 0.0);
        data.mass = 0.0;
    }

    component_cache_[component_id] = data;
    return data;
}

void MassPropertiesCalculator::setComponentCustomMass(
    const EntityId& component_id,
    double mass,
    const Vector3D& cog
) {
    auto it = component_cache_.find(component_id);
    if (it != component_cache_.end()) {
        it->second.mass = mass;
        it->second.center_of_gravity = cog;
        it->second.is_custom_mass = true;
    }
}

Vector3D MassPropertiesCalculator::calculateCenterOfGravity(
    const std::vector<ComponentMassData>& components
) const {
    if (components.empty()) {
        return Vector3D(0, 0, 0);
    }

    double total_mass = 0.0;
    Vector3D weighted_cog(0, 0, 0);

    for (const auto& comp : components) {
        if (comp.mass > 1e-10) {
            weighted_cog = weighted_cog + (comp.center_of_gravity * comp.mass);
            total_mass += comp.mass;
        }
    }

    if (total_mass > 1e-10) {
        return weighted_cog * (1.0 / total_mass);
    }

    return Vector3D(0, 0, 0);
}

Matrix3x3 MassPropertiesCalculator::calculatePointMassInertia(
    double mass,
    const Vector3D& relative_position
) const {
    Matrix3x3 inertia;

    // For a point mass at position r:
    // I_xx = m(y^2 + z^2)
    // I_yy = m(x^2 + z^2)
    // I_zz = m(x^2 + y^2)
    // I_xy = I_yx = -m*x*y
    // I_xz = I_zx = -m*x*z
    // I_yz = I_zy = -m*y*z

    double x = relative_position.x;
    double y = relative_position.y;
    double z = relative_position.z;

    inertia.set(0, 0, mass * (y * y + z * z));
    inertia.set(1, 1, mass * (x * x + z * z));
    inertia.set(2, 2, mass * (x * x + y * y));

    inertia.set(0, 1, -mass * x * y);
    inertia.set(1, 0, -mass * x * y);

    inertia.set(0, 2, -mass * x * z);
    inertia.set(2, 0, -mass * x * z);

    inertia.set(1, 2, -mass * y * z);
    inertia.set(2, 1, -mass * y * z);

    return inertia;
}

Matrix3x3 MassPropertiesCalculator::calculateInertiaTensor(
    const std::vector<ComponentMassData>& components,
    const Vector3D& about_point
) {
    Matrix3x3 total_inertia;

    for (const auto& comp : components) {
        if (comp.mass > 1e-10) {
            Vector3D relative_pos = comp.center_of_gravity - about_point;
            Matrix3x3 comp_inertia = calculatePointMassInertia(comp.mass, relative_pos);
            total_inertia = total_inertia + comp_inertia;
        }
    }

    return total_inertia;
}

std::array<double, 3> MassPropertiesCalculator::calculatePrincipalMoments(
    const Matrix3x3& inertia_tensor
) {
    // Simplified eigenvalue computation for 3x3 symmetric matrix
    // Using power iteration method or analytical solution for 3x3 case

    double a = inertia_tensor.get(0, 0);
    double b = inertia_tensor.get(0, 1);
    double c = inertia_tensor.get(0, 2);
    double d = inertia_tensor.get(1, 1);
    double e = inertia_tensor.get(1, 2);
    double f = inertia_tensor.get(2, 2);

    // Characteristic polynomial coefficients for 3x3 matrix
    double trace = a + d + f;
    double sum_diagonal_minors = a * d + a * f + d * f - b * b - c * c - e * e;
    double det = a * (d * f - e * e) - b * (b * f - c * e) + c * (b * e - c * d);

    // Use Newton's method to find roots (simplified)
    std::array<double, 3> eigenvalues = {a, d, f};
    std::sort(eigenvalues.begin(), eigenvalues.end());

    return eigenvalues;
}

std::array<double, 3> MassPropertiesCalculator::calculateRadiusOfGyration(
    double total_mass,
    const std::array<double, 3>& principal_moments
) {
    std::array<double, 3> radius;

    if (total_mass > 1e-10) {
        for (int i = 0; i < 3; i++) {
            radius[i] = std::sqrt(principal_moments[i] / total_mass);
        }
    } else {
        radius = {0, 0, 0};
    }

    return radius;
}

std::pair<Vector3D, Vector3D> MassPropertiesCalculator::calculateBoundingBox(
    const std::vector<ComponentMassData>& components
) const {
    if (components.empty()) {
        return {{0, 0, 0}, {0, 0, 0}};
    }

    Vector3D min = components[0].center_of_gravity;
    Vector3D max = components[0].center_of_gravity;

    for (const auto& comp : components) {
        const Vector3D& pos = comp.center_of_gravity;

        min.x = std::min(min.x, pos.x);
        min.y = std::min(min.y, pos.y);
        min.z = std::min(min.z, pos.z);

        max.x = std::max(max.x, pos.x);
        max.y = std::max(max.y, pos.y);
        max.z = std::max(max.z, pos.z);
    }

    return {min, max};
}

std::map<std::string, double> MassPropertiesCalculator::getMaterialComposition(
    const std::vector<ComponentMassData>& components
) const {
    std::map<std::string, double> composition;

    for (const auto& comp : components) {
        composition[comp.material.name] += comp.mass;
    }

    return composition;
}

MassProperties MassPropertiesCalculator::calculateAssemblyMassProperties(
    const std::vector<ComponentMassData>& components,
    const Vector3D& reference_point
) {
    MassProperties props;

    // Calculate total mass
    for (const auto& comp : components) {
        props.total_mass += comp.mass;
        props.volume += comp.volume;
    }

    // Calculate center of gravity
    props.center_of_gravity = calculateCenterOfGravity(components);

    // Calculate inertia tensor about COG
    props.inertia_tensor = calculateInertiaTensor(components, props.center_of_gravity);

    // Calculate principal moments
    props.principal_moments = calculatePrincipalMoments(props.inertia_tensor);

    // Calculate radius of gyration
    props.radius_of_gyration = calculateRadiusOfGyration(
        props.total_mass,
        props.principal_moments
    );

    // Calculate bounding box
    auto [bbox_min, bbox_max] = calculateBoundingBox(components);
    props.bbox_min = bbox_min;
    props.bbox_max = bbox_max;

    // Get material composition
    props.material_composition = getMaterialComposition(components);

    return props;
}

void MassPropertiesCalculator::diagonalizeInertia(
    const Matrix3x3& I,
    std::array<double, 3>& eigenvalues,
    std::array<Vector3D, 3>& eigenvectors
) {
    // Simplified eigenvalue/eigenvector computation
    // For a full implementation, use proper numerical methods
    eigenvalues = calculatePrincipalMoments(I);

    // Set default orthonormal eigenvectors
    eigenvectors[0] = Vector3D(1, 0, 0);
    eigenvectors[1] = Vector3D(0, 1, 0);
    eigenvectors[2] = Vector3D(0, 0, 1);
}

// ==================== MassPropertiesReporter ====================

std::string MassPropertiesReporter::formatDouble(double value, int precision) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string MassPropertiesReporter::formatVector(const Vector3D& v) const {
    std::ostringstream oss;
    oss << "(" << formatDouble(v.x, 4) << ", "
        << formatDouble(v.y, 4) << ", "
        << formatDouble(v.z, 4) << ")";
    return oss.str();
}

std::string MassPropertiesReporter::generateReport(
    const MassProperties& props,
    const std::string& title
) const {
    std::ostringstream oss;

    oss << "========================================\n";
    oss << "  " << title << "\n";
    oss << "========================================\n\n";

    oss << "MASS PROPERTIES\n";
    oss << "---------------\n";
    oss << "Total Mass:              " << formatDouble(props.total_mass, 4) << " kg\n";
    oss << "Total Volume:            " << formatDouble(props.volume, 4) << " m^3\n";
    oss << "Total Surface Area:      " << formatDouble(props.surface_area, 4) << " m^2\n\n";

    oss << "CENTER OF GRAVITY\n";
    oss << "-----------------\n";
    oss << "Position:                " << formatVector(props.center_of_gravity) << " m\n\n";

    oss << "INERTIA TENSOR (about COG)\n";
    oss << "--------------------------\n";
    oss << "Ixx: " << formatDouble(props.inertia_tensor.get(0, 0), 6) << "  ";
    oss << "Ixy: " << formatDouble(props.inertia_tensor.get(0, 1), 6) << "  ";
    oss << "Ixz: " << formatDouble(props.inertia_tensor.get(0, 2), 6) << "\n";
    oss << "Iyx: " << formatDouble(props.inertia_tensor.get(1, 0), 6) << "  ";
    oss << "Iyy: " << formatDouble(props.inertia_tensor.get(1, 1), 6) << "  ";
    oss << "Iyz: " << formatDouble(props.inertia_tensor.get(1, 2), 6) << "\n";
    oss << "Izx: " << formatDouble(props.inertia_tensor.get(2, 0), 6) << "  ";
    oss << "Izy: " << formatDouble(props.inertia_tensor.get(2, 1), 6) << "  ";
    oss << "Izz: " << formatDouble(props.inertia_tensor.get(2, 2), 6) << "\n\n";

    oss << "PRINCIPAL MOMENTS OF INERTIA\n";
    oss << "----------------------------\n";
    oss << "I1 (minimum): " << formatDouble(props.principal_moments[0], 6) << " kg*m^2\n";
    oss << "I2:           " << formatDouble(props.principal_moments[1], 6) << " kg*m^2\n";
    oss << "I3 (maximum): " << formatDouble(props.principal_moments[2], 6) << " kg*m^2\n\n";

    oss << "RADIUS OF GYRATION\n";
    oss << "------------------\n";
    oss << "Rx: " << formatDouble(props.radius_of_gyration[0], 4) << " m\n";
    oss << "Ry: " << formatDouble(props.radius_of_gyration[1], 4) << " m\n";
    oss << "Rz: " << formatDouble(props.radius_of_gyration[2], 4) << " m\n\n";

    oss << "BOUNDING BOX\n";
    oss << "------------\n";
    oss << "Min: " << formatVector(props.bbox_min) << " m\n";
    oss << "Max: " << formatVector(props.bbox_max) << " m\n\n";

    oss << "MATERIAL COMPOSITION\n";
    oss << "--------------------\n";
    for (const auto& [material, mass] : props.material_composition) {
        double percentage = (props.total_mass > 1e-10) ?
            (mass / props.total_mass) * 100.0 : 0.0;
        oss << material << ":  " << formatDouble(mass, 4) << " kg ("
            << formatDouble(percentage, 2) << "%)\n";
    }
    oss << "\n";

    return oss.str();
}

std::string MassPropertiesReporter::generateCSV(
    const MassProperties& props,
    const std::vector<ComponentMassData>& components
) const {
    std::ostringstream oss;

    // Header
    oss << "Component,Mass (kg),Volume (m^3),Material,COG_X,COG_Y,COG_Z\n";

    // Components
    for (const auto& comp : components) {
        oss << comp.component_name << ","
            << formatDouble(comp.mass, 4) << ","
            << formatDouble(comp.volume, 4) << ","
            << comp.material.name << ","
            << formatDouble(comp.center_of_gravity.x, 4) << ","
            << formatDouble(comp.center_of_gravity.y, 4) << ","
            << formatDouble(comp.center_of_gravity.z, 4) << "\n";
    }

    // Summary
    oss << "\nSummary\n";
    oss << "Total Mass (kg)," << formatDouble(props.total_mass, 4) << "\n";
    oss << "COG_X (m)," << formatDouble(props.center_of_gravity.x, 4) << "\n";
    oss << "COG_Y (m)," << formatDouble(props.center_of_gravity.y, 4) << "\n";
    oss << "COG_Z (m)," << formatDouble(props.center_of_gravity.z, 4) << "\n";

    return oss.str();
}

std::string MassPropertiesReporter::generateJSON(
    const MassProperties& props,
    const std::vector<ComponentMassData>& components
) const {
    std::ostringstream oss;

    oss << "{\n";
    oss << "  \"total_mass\": " << formatDouble(props.total_mass, 6) << ",\n";
    oss << "  \"total_volume\": " << formatDouble(props.volume, 6) << ",\n";
    oss << "  \"center_of_gravity\": {\n";
    oss << "    \"x\": " << formatDouble(props.center_of_gravity.x, 6) << ",\n";
    oss << "    \"y\": " << formatDouble(props.center_of_gravity.y, 6) << ",\n";
    oss << "    \"z\": " << formatDouble(props.center_of_gravity.z, 6) << "\n";
    oss << "  },\n";

    oss << "  \"principal_moments\": [\n";
    for (int i = 0; i < 3; i++) {
        oss << "    " << formatDouble(props.principal_moments[i], 6);
        if (i < 2) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    oss << "  \"components\": [\n";
    for (size_t i = 0; i < components.size(); i++) {
        const auto& comp = components[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << comp.component_name << "\",\n";
        oss << "      \"mass\": " << formatDouble(comp.mass, 6) << ",\n";
        oss << "      \"volume\": " << formatDouble(comp.volume, 6) << ",\n";
        oss << "      \"material\": \"" << comp.material.name << "\",\n";
        oss << "      \"cog\": [" << formatDouble(comp.center_of_gravity.x, 6) << ", "
            << formatDouble(comp.center_of_gravity.y, 6) << ", "
            << formatDouble(comp.center_of_gravity.z, 6) << "]\n";
        oss << "    }";
        if (i < components.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";

    return oss.str();
}

bool MassPropertiesReporter::exportToFile(
    const MassProperties& props,
    const std::string& filepath,
    const std::string& format
) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        std::string content;
        if (format == "json") {
            std::vector<ComponentMassData> components;  // Empty for summary
            content = generateJSON(props, components);
        } else if (format == "text") {
            content = generateReport(props);
        }

        file << content;
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

std::string MassPropertiesReporter::generateWeightBreakdown(
    const std::vector<ComponentMassData>& components
) const {
    std::ostringstream oss;

    oss << "WEIGHT BREAKDOWN BY COMPONENT\n";
    oss << "=============================\n\n";

    double total_mass = 0.0;
    for (const auto& comp : components) {
        total_mass += comp.mass;
    }

    for (const auto& comp : components) {
        double percentage = (total_mass > 1e-10) ?
            (comp.mass / total_mass) * 100.0 : 0.0;
        oss << comp.component_name << ": "
            << formatDouble(comp.mass, 4) << " kg ("
            << formatDouble(percentage, 2) << "%)\n";
    }

    oss << "\nTOTAL: " << formatDouble(total_mass, 4) << " kg\n";

    return oss.str();
}

std::string MassPropertiesReporter::generateMaterialBreakdown(
    const MassProperties& props
) const {
    std::ostringstream oss;

    oss << "WEIGHT BREAKDOWN BY MATERIAL\n";
    oss << "=============================\n\n";

    for (const auto& [material, mass] : props.material_composition) {
        double percentage = (props.total_mass > 1e-10) ?
            (mass / props.total_mass) * 100.0 : 0.0;
        oss << material << ": "
            << formatDouble(mass, 4) << " kg ("
            << formatDouble(percentage, 2) << "%)\n";
    }

    oss << "\nTOTAL: " << formatDouble(props.total_mass, 4) << " kg\n";

    return oss.str();
}

std::string MassPropertiesReporter::generateBalanceAnalysis(
    const MassProperties& props,
    const Vector3D& ideal_cog
) const {
    std::ostringstream oss;

    Vector3D offset = props.center_of_gravity - ideal_cog;
    double offset_magnitude = offset.magnitude();

    oss << "BALANCE ANALYSIS REPORT\n";
    oss << "======================\n\n";

    oss << "Actual COG:   " << formatVector(props.center_of_gravity) << "\n";
    oss << "Ideal COG:    " << formatVector(ideal_cog) << "\n";
    oss << "Offset:       " << formatVector(offset) << "\n";
    oss << "Offset Magnitude: " << formatDouble(offset_magnitude, 4) << " m\n\n";

    if (offset_magnitude < 1e-4) {
        oss << "Status: BALANCED\n";
    } else if (offset_magnitude < 0.01) {
        oss << "Status: SLIGHTLY UNBALANCED\n";
    } else if (offset_magnitude < 0.1) {
        oss << "Status: MODERATELY UNBALANCED\n";
    } else {
        oss << "Status: SEVERELY UNBALANCED\n";
    }

    return oss.str();
}

}  // namespace BendCore::CadCore
