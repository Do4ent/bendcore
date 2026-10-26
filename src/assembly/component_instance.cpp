#include "bendcore/assembly/component_instance.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace BendCore::CadCore {

constexpr double DEG_TO_RAD = 3.14159265359 / 180.0;
constexpr double RAD_TO_DEG = 180.0 / 3.14159265359;

ComponentInstance::ComponentInstance(
    const EntityId& instance_id,
    const EntityId& part_id,
    const Vector3D& position,
    const Vector3D& orientation)
    : instance_id_(instance_id)
    , part_id_(part_id)
    , position_(position)
    , orientation_(orientation)
{
}

void ComponentInstance::setPosition(const Vector3D& position) noexcept {
    position_ = position;
    incrementRevision();
}

void ComponentInstance::setOrientation(const Vector3D& orientation) noexcept {
    orientation_ = orientation;
    incrementRevision();
}

uint64_t ComponentInstance::incrementRevision() noexcept {
    return ++revision_;
}

std::optional<std::string> ComponentInstance::getMetadata(const std::string& key) const noexcept {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ComponentInstance::setMetadata(const std::string& key, const std::string& value) noexcept {
    metadata_[key] = value;
    incrementRevision();
}

std::array<double, 16> ComponentInstance::getTransformMatrix() const noexcept {
    return eulerToMatrix(position_, orientation_);
}

std::array<double, 16> ComponentInstance::eulerToMatrix(
    const Vector3D& position,
    const Vector3D& orientation) const noexcept {
    // Convert Euler angles (degrees) to radians
    double rx = orientation.x * DEG_TO_RAD;
    double ry = orientation.y * DEG_TO_RAD;
    double rz = orientation.z * DEG_TO_RAD;

    // Compute rotation matrix (ZYX convention)
    double cos_rx = std::cos(rx);
    double sin_rx = std::sin(rx);
    double cos_ry = std::cos(ry);
    double sin_ry = std::sin(ry);
    double cos_rz = std::cos(rz);
    double sin_rz = std::sin(rz);

    // ZYX Euler angles rotation matrix
    double m00 = cos_ry * cos_rz;
    double m01 = -cos_ry * sin_rz;
    double m02 = sin_ry;
    double m10 = sin_rx * sin_ry * cos_rz + cos_rx * sin_rz;
    double m11 = -sin_rx * sin_ry * sin_rz + cos_rx * cos_rz;
    double m12 = -sin_rx * cos_ry;
    double m20 = -cos_rx * sin_ry * cos_rz + sin_rx * sin_rz;
    double m21 = cos_rx * sin_ry * sin_rz + sin_rx * cos_rz;
    double m22 = cos_rx * cos_ry;

    // Build 4x4 transformation matrix (row-major)
    std::array<double, 16> matrix = {
        m00, m01, m02, position.x,
        m10, m11, m12, position.y,
        m20, m21, m22, position.z,
        0.0, 0.0, 0.0, 1.0
    };

    return matrix;
}

std::string ComponentInstance::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "ComponentInstance(id=" << instance_id_.toString()
        << ", part_id=" << part_id_.toString()
        << ", name=" << name_
        << ", pos=(" << position_.x << "," << position_.y << "," << position_.z << ")"
        << ", rot=(" << orientation_.x << "," << orientation_.y << "," << orientation_.z << ")"
        << ")";
    return oss.str();
}

std::string ComponentInstance::validate() const noexcept {
    // Check if instance ID is valid
    if (instance_id_.isNull()) {
        return "Component instance ID is null";
    }

    // Check if part ID is valid
    if (part_id_.isNull()) {
        return "Part ID is null";
    }

    // Check for NaN or Inf in position
    if (std::isnan(position_.x) || std::isnan(position_.y) || std::isnan(position_.z) ||
        std::isinf(position_.x) || std::isinf(position_.y) || std::isinf(position_.z)) {
        return "Invalid position (contains NaN or Inf)";
    }

    // Check for NaN or Inf in orientation
    if (std::isnan(orientation_.x) || std::isnan(orientation_.y) || std::isnan(orientation_.z) ||
        std::isinf(orientation_.x) || std::isinf(orientation_.y) || std::isinf(orientation_.z)) {
        return "Invalid orientation (contains NaN or Inf)";
    }

    // Check orientation is in reasonable range (-360 to 360 degrees)
    const double MAX_ANGLE = 360.0;
    if (std::abs(orientation_.x) > MAX_ANGLE * 2 ||
        std::abs(orientation_.y) > MAX_ANGLE * 2 ||
        std::abs(orientation_.z) > MAX_ANGLE * 2) {
        return "Orientation angles out of reasonable range";
    }

    return "";  // Valid
}

std::shared_ptr<ComponentInstance> createComponentInstance(
    const EntityId& part_id,
    const Vector3D& position,
    const Vector3D& orientation)
{
    return std::make_shared<ComponentInstance>(
        EntityId::generate(),
        part_id,
        position,
        orientation
    );
}

} // namespace BendCore::CadCore
