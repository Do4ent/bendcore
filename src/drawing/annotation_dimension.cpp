// M4 Phase - Annotation & Dimensioning System (GAP-018)
// Implementation of annotation and dimensioning functionality
#include "bendcore/drawing/annotation_dimension.hpp"

#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace BendCore::Drawing {

// ============================================================================
// ToleranceSpec Implementation
// ============================================================================

bool ToleranceSpec::isValid() const {
    if (type == ToleranceType::None) return true;
    if (type == ToleranceType::Bilateral) return upper_limit >= 0.0 && lower_limit >= 0.0;
    if (type == ToleranceType::Unilateral) return upper_limit >= 0.0 || lower_limit >= 0.0;
    if (type == ToleranceType::Limit) return upper_limit >= lower_limit;
    return false;
}

std::string ToleranceSpec::toString() const {
    std::ostringstream oss;

    switch (type) {
    case ToleranceType::None:
        return "";
    case ToleranceType::Bilateral:
        oss << "+/-" << upper_limit;
        break;
    case ToleranceType::Unilateral:
        oss << "+";
        if (upper_limit != 0.0) oss << upper_limit;
        oss << " -" << lower_limit;
        break;
    case ToleranceType::Limit:
        oss << upper_limit << "/" << lower_limit;
        break;
    }

    if (!note.empty()) {
        oss << " (" << note << ")";
    }

    return oss.str();
}

// ============================================================================
// DimensionProperties Implementation
// ============================================================================

std::string DimensionProperties::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);

    if (!text_prefix.empty()) {
        oss << text_prefix;
    }

    oss << value << " " << unit;

    if (!text_suffix.empty()) {
        oss << " " << text_suffix;
    }

    if (tolerance.type != ToleranceType::None) {
        oss << " " << tolerance.toString();
    }

    return oss.str();
}

// ============================================================================
// Dimension Implementation
// ============================================================================

Dimension::Dimension(const std::string& id, DimensionType type)
    : id_(id), type_(type) {
}

// ============================================================================
// LinearDimension Implementation
// ============================================================================

LinearDimension::LinearDimension(const std::string& id, LinearDimensionSubType sub_type)
    : Dimension(id, DimensionType::Linear), sub_type_(sub_type) {
}

void LinearDimension::updateValue() {
    double dx = p1_[0] - p0_[0];
    double dy = p1_[1] - p0_[1];

    if (sub_type_ == LinearDimensionSubType::Horizontal) {
        properties_.value = std::abs(dx);
    } else if (sub_type_ == LinearDimensionSubType::Vertical) {
        properties_.value = std::abs(dy);
    } else { // Aligned
        properties_.value = calculateDistance(p0_, p1_);
    }
}

// ============================================================================
// RadialDimension Implementation
// ============================================================================

RadialDimension::RadialDimension(const std::string& id, bool is_diameter)
    : Dimension(id, is_diameter ? DimensionType::Diameter : DimensionType::Radial),
      is_diameter_(is_diameter) {
}

void RadialDimension::updateValue() {
    double dx = reference_point_[0] - center_[0];
    double dy = reference_point_[1] - center_[1];
    double radius = std::sqrt(dx * dx + dy * dy);

    if (is_diameter_) {
        properties_.value = radius * 2.0;
        properties_.text_prefix = "Ø";
    } else {
        properties_.value = radius;
        properties_.text_prefix = "R";
    }
}

// ============================================================================
// AngularDimension Implementation
// ============================================================================

AngularDimension::AngularDimension(const std::string& id)
    : Dimension(id, DimensionType::Angular) {
}

void AngularDimension::updateValue() {
    properties_.value = calculateAngle(p1_start_, p1_end_, p2_start_, p2_end_);
    properties_.unit = "°";
}

// ============================================================================
// OrdinateDimension Implementation
// ============================================================================

OrdinateDimension::OrdinateDimension(const std::string& id, bool is_horizontal)
    : Dimension(id, DimensionType::Ordinate), is_horizontal_(is_horizontal) {
}

void OrdinateDimension::updateValue() {
    if (is_horizontal_) {
        properties_.value = std::abs(reference_point_[0] - datum_[0]);
    } else {
        properties_.value = std::abs(reference_point_[1] - datum_[1]);
    }
}

// ============================================================================
// LeaderDimension Implementation
// ============================================================================

LeaderDimension::LeaderDimension(const std::string& id)
    : Dimension(id, DimensionType::Leader) {
}

// ============================================================================
// Annotation Implementation
// ============================================================================

Annotation::Annotation(const std::string& id, AnnotationType type)
    : id_(id), type_(type) {
}

// ============================================================================
// TextAnnotation Implementation
// ============================================================================

TextAnnotation::TextAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::TextNote) {
}

// ============================================================================
// RevisionCloudAnnotation Implementation
// ============================================================================

RevisionCloudAnnotation::RevisionCloudAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::RevisionCloud) {
}

// ============================================================================
// WeldSymbolAnnotation Implementation
// ============================================================================

WeldSymbolAnnotation::WeldSymbolAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::WeldSymbol) {
}

// ============================================================================
// SurfaceFinishAnnotation Implementation
// ============================================================================

SurfaceFinishAnnotation::SurfaceFinishAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::SurfaceFinish) {
}

// ============================================================================
// GDandTAnnotation Implementation
// ============================================================================

GDandTAnnotation::GDandTAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::GDandT) {
}

// ============================================================================
// ReferenceTagAnnotation Implementation
// ============================================================================

ReferenceTagAnnotation::ReferenceTagAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::ReferenceTag) {
}

// ============================================================================
// BalloonAnnotation Implementation
// ============================================================================

BalloonAnnotation::BalloonAnnotation(const std::string& id)
    : Annotation(id, AnnotationType::Balloon) {
}

// ============================================================================
// DimensionManager Implementation
// ============================================================================

DimensionManager::DimensionManager(const std::string& view_id)
    : view_id_(view_id) {
}

std::shared_ptr<Dimension> DimensionManager::addDimension(std::shared_ptr<Dimension> dim) {
    if (!dim) return nullptr;
    dim->setParentViewId(view_id_);
    dimensions_.push_back(dim);
    return dim;
}

std::shared_ptr<Dimension> DimensionManager::getDimension(const std::string& id) const {
    for (const auto& dim : dimensions_) {
        if (dim->getId() == id) return dim;
    }
    return nullptr;
}

void DimensionManager::removeDimension(const std::string& id) {
    auto it = std::remove_if(dimensions_.begin(), dimensions_.end(),
        [&id](const std::shared_ptr<Dimension>& dim) { return dim->getId() == id; });
    dimensions_.erase(it, dimensions_.end());

    // Remove from entity map
    for (auto& pair : entity_dimension_map_) {
        auto& dim_ids = pair.second;
        auto dim_it = std::remove(dim_ids.begin(), dim_ids.end(), id);
        dim_ids.erase(dim_it, dim_ids.end());
    }
}

std::shared_ptr<Annotation> DimensionManager::addAnnotation(std::shared_ptr<Annotation> annotation) {
    if (!annotation) return nullptr;
    annotation->setParentViewId(view_id_);
    annotations_.push_back(annotation);
    return annotation;
}

std::shared_ptr<Annotation> DimensionManager::getAnnotation(const std::string& id) const {
    for (const auto& annotation : annotations_) {
        if (annotation->getId() == id) return annotation;
    }
    return nullptr;
}

void DimensionManager::removeAnnotation(const std::string& id) {
    auto it = std::remove_if(annotations_.begin(), annotations_.end(),
        [&id](const std::shared_ptr<Annotation>& ann) { return ann->getId() == id; });
    annotations_.erase(it, annotations_.end());
}

std::vector<std::shared_ptr<Dimension>> DimensionManager::autoDimension(
    const AutoDimensioningConfig& config) {
    std::vector<std::shared_ptr<Dimension>> new_dims;

    // Generate automatic dimensions from geometry
    // This is a placeholder implementation - real implementation would
    // analyze the drawing geometry and extract meaningful dimensions

    for (const auto& dim : new_dims) {
        adjustDimensionPlacement(const_cast<std::shared_ptr<Dimension>&>(dim));
    }

    return new_dims;
}

std::vector<std::shared_ptr<Dimension>> DimensionManager::getDimensionsByType(DimensionType type) const {
    std::vector<std::shared_ptr<Dimension>> result;
    for (const auto& dim : dimensions_) {
        if (dim->getType() == type) {
            result.push_back(dim);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Dimension>> DimensionManager::getDimensionsForEntity(
    const std::string& entity_id) const {
    std::vector<std::shared_ptr<Dimension>> result;

    auto it = entity_dimension_map_.find(entity_id);
    if (it != entity_dimension_map_.end()) {
        for (const auto& dim_id : it->second) {
            auto dim = getDimension(dim_id);
            if (dim) result.push_back(dim);
        }
    }

    return result;
}

void DimensionManager::linkDimensionToEntity(const std::string& dim_id, const std::string& entity_id) {
    auto dim = getDimension(dim_id);
    if (!dim) return;

    dim->addLinkedEntity(entity_id);
    entity_dimension_map_[entity_id].push_back(dim_id);
}

void DimensionManager::unlinkDimensionFromEntity(const std::string& dim_id, const std::string& entity_id) {
    auto dim = getDimension(dim_id);
    if (!dim) return;

    // Remove from entity map
    auto it = entity_dimension_map_.find(entity_id);
    if (it != entity_dimension_map_.end()) {
        auto& dim_ids = it->second;
        auto dim_it = std::remove(dim_ids.begin(), dim_ids.end(), dim_id);
        dim_ids.erase(dim_it, dim_ids.end());
    }
}

bool DimensionManager::checkDimensionOverlap(const std::shared_ptr<Dimension>& dim) const {
    if (!dim) return false;

    // Simple overlap check - in real implementation would check bounding boxes
    // of dimension text and lines against other dimensions

    return false;
}

void DimensionManager::adjustDimensionPlacement(std::shared_ptr<Dimension>& dim) {
    if (!dim) return;

    // Adjust dimension placement to avoid overlaps
    while (checkDimensionOverlap(dim)) {
        auto& props = dim->getProperties();
        props.offset[1] += 5.0;  // Move dimension further away
    }
}

void DimensionManager::clear() {
    dimensions_.clear();
    annotations_.clear();
    entity_dimension_map_.clear();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::shared_ptr<LinearDimension> createLinearDimension(
    const std::string& id,
    LinearDimensionSubType sub_type,
    const Vec2& p0,
    const Vec2& p1) {
    auto dim = std::make_shared<LinearDimension>(id, sub_type);
    dim->setReferencePoints(p0, p1);
    dim->updateValue();
    return dim;
}

std::shared_ptr<RadialDimension> createRadialDimension(
    const std::string& id,
    const Vec2& center,
    const Vec2& reference_point,
    bool is_diameter) {
    auto dim = std::make_shared<RadialDimension>(id, is_diameter);
    dim->setCenter(center);
    dim->setReferencePoint(reference_point);
    dim->updateValue();
    return dim;
}

std::shared_ptr<AngularDimension> createAngularDimension(
    const std::string& id,
    const Vec2& p1_start,
    const Vec2& p1_end,
    const Vec2& p2_start,
    const Vec2& p2_end) {
    auto dim = std::make_shared<AngularDimension>(id);
    dim->setReferenceLines(p1_start, p1_end, p2_start, p2_end);
    dim->updateValue();
    return dim;
}

double calculateDistance(const Vec2& p0, const Vec2& p1) {
    double dx = p1[0] - p0[0];
    double dy = p1[1] - p0[1];
    return std::sqrt(dx * dx + dy * dy);
}

double calculateAngle(const Vec2& p1_start, const Vec2& p1_end,
                     const Vec2& p2_start, const Vec2& p2_end) {
    // Calculate vectors
    double v1x = p1_end[0] - p1_start[0];
    double v1y = p1_end[1] - p1_start[1];
    double v2x = p2_end[0] - p2_start[0];
    double v2y = p2_end[1] - p2_start[1];

    // Normalize
    double len1 = std::sqrt(v1x * v1x + v1y * v1y);
    double len2 = std::sqrt(v2x * v2x + v2y * v2y);

    if (len1 < 1e-10 || len2 < 1e-10) return 0.0;

    v1x /= len1;
    v1y /= len1;
    v2x /= len2;
    v2y /= len2;

    // Dot product
    double dot = v1x * v2x + v1y * v2y;
    dot = std::max(-1.0, std::min(1.0, dot));  // Clamp to [-1, 1]

    // Angle in radians, convert to degrees
    double angle_rad = std::acos(dot);
    double angle_deg = angle_rad * 180.0 / M_PI;

    return angle_deg;
}

std::string formatDimensionValue(double value, int precision, const std::string& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value << " " << unit;
    return oss.str();
}

std::string dimensionTypeToString(DimensionType type) {
    switch (type) {
    case DimensionType::Linear: return "Linear";
    case DimensionType::Radial: return "Radial";
    case DimensionType::Diameter: return "Diameter";
    case DimensionType::Angular: return "Angular";
    case DimensionType::Ordinate: return "Ordinate";
    case DimensionType::Leader: return "Leader";
    case DimensionType::Reference: return "Reference";
    }
    return "Unknown";
}

std::string annotationTypeToString(AnnotationType type) {
    switch (type) {
    case AnnotationType::TextNote: return "TextNote";
    case AnnotationType::RevisionCloud: return "RevisionCloud";
    case AnnotationType::WeldSymbol: return "WeldSymbol";
    case AnnotationType::SurfaceFinish: return "SurfaceFinish";
    case AnnotationType::GDandT: return "GDandT";
    case AnnotationType::ReferenceTag: return "ReferenceTag";
    case AnnotationType::Balloon: return "Balloon";
    }
    return "Unknown";
}

} // namespace BendCore::Drawing
