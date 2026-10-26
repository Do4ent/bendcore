#include "../../include/bendcore/assembly/assembly_constraints.hpp"
#include <sstream>
#include <cmath>

namespace BendCore::CadCore {

// ==================== FixedConstraint ====================

bool FixedConstraint::validate() const {
    // Fixed constraint is valid if component ID is valid
    if (component_id_.toString().empty()) {
        throw std::invalid_argument("FixedConstraint: component_id is invalid");
    }
    return true;
}

std::string FixedConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Fixed: Component " << component_id_.toString() << " locked at position ("
        << position_.x << ", " << position_.y << ", " << position_.z << ")";
    return oss.str();
}

// ==================== CoincidentConstraint ====================

bool CoincidentConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("CoincidentConstraint: entity IDs are invalid");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument(
            "CoincidentConstraint: cannot make entity coincident with itself");
    }
    return true;
}

std::string CoincidentConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Coincident: " << entity1_id_.toString() << "[" << geometry1_index_ << "] <-> "
        << entity2_id_.toString() << "[" << geometry2_index_ << "]";
    return oss.str();
}

// ==================== DistanceConstraint ====================

bool DistanceConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("DistanceConstraint: entity IDs are invalid");
    }
    if (distance_ < 0.0) {
        throw std::invalid_argument("DistanceConstraint: distance cannot be negative");
    }
    if (entity1_id_ == entity2_id_ && geometry1_index_ == geometry2_index_) {
        throw std::invalid_argument(
            "DistanceConstraint: cannot constrain same geometry to itself");
    }
    return true;
}

std::string DistanceConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Distance: " << entity1_id_.toString() << " <-> " << entity2_id_.toString()
        << " = " << distance_;
    return oss.str();
}

// ==================== ParallelConstraint ====================

bool ParallelConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("ParallelConstraint: entity IDs are invalid");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument("ParallelConstraint: cannot parallel entity with itself");
    }
    return true;
}

std::string ParallelConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Parallel: " << entity1_id_.toString() << "[" << geometry1_index_ << "] || "
        << entity2_id_.toString() << "[" << geometry2_index_ << "]";
    return oss.str();
}

// ==================== PerpendicularConstraint ====================

bool PerpendicularConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("PerpendicularConstraint: entity IDs are invalid");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument(
            "PerpendicularConstraint: cannot perpendicular entity with itself");
    }
    return true;
}

std::string PerpendicularConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Perpendicular: " << entity1_id_.toString() << "[" << geometry1_index_
        << "] ⊥ " << entity2_id_.toString() << "[" << geometry2_index_ << "]";
    return oss.str();
}

// ==================== TangentConstraint ====================

bool TangentConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("TangentConstraint: entity IDs are invalid");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument("TangentConstraint: cannot tangent entity with itself");
    }
    return true;
}

std::string TangentConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Tangent: " << entity1_id_.toString() << "[" << geometry1_index_
        << "] tangent to " << entity2_id_.toString() << "[" << geometry2_index_ << "]";
    return oss.str();
}

// ==================== SymmetricConstraint ====================

bool SymmetricConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty() ||
        symmetry_plane_id_.toString().empty()) {
        throw std::invalid_argument("SymmetricConstraint: entity IDs are invalid");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument(
            "SymmetricConstraint: cannot make entity symmetric with itself");
    }
    return true;
}

std::string SymmetricConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Symmetric: " << entity1_id_.toString() << " <-> " << entity2_id_.toString()
        << " about plane " << symmetry_plane_id_.toString();
    return oss.str();
}

// ==================== AngleConstraint ====================

bool AngleConstraint::validate() const {
    if (entity1_id_.toString().empty() || entity2_id_.toString().empty()) {
        throw std::invalid_argument("AngleConstraint: entity IDs are invalid");
    }
    if (angle_ < 0.0 || angle_ > 180.0) {
        throw std::invalid_argument("AngleConstraint: angle must be between 0 and 180");
    }
    if (entity1_id_ == entity2_id_) {
        throw std::invalid_argument("AngleConstraint: cannot angle entity with itself");
    }
    return true;
}

std::string AngleConstraint::getDescription() const noexcept {
    std::ostringstream oss;
    oss << "Angle: " << entity1_id_.toString() << " <> " << entity2_id_.toString()
        << " = " << angle_ << "°";
    return oss.str();
}

} // namespace BendCore::CadCore
