// M4 Phase - Annotation & Dimensioning System (GAP-018)
// Complete annotation and dimensioning system for drawings
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <array>
#include <cmath>

#include "../vec3.hpp"

namespace BendCore::Drawing {

using Vec2 = std::array<double, 2>;
using Vec3 = bendcore::Vec3;

// ============================================================================
// Dimension Types
// ============================================================================

enum class DimensionType {
    Linear,        // Horizontal, vertical, or aligned linear dimensions
    Radial,        // Radius dimension
    Diameter,      // Diameter dimension
    Angular,       // Angular/angular dimension
    Ordinate,      // Ordinate/offset dimension
    Leader,        // Leader dimension with text
    Reference      // Reference dimension (info only, no tolerance)
};

enum class LinearDimensionSubType {
    Horizontal,    // Horizontal dimension
    Vertical,      // Vertical dimension
    Aligned        // Aligned with reference line
};

enum class ToleranceType {
    None,          // No tolerance
    Bilateral,     // +/- symmetric tolerance
    Unilateral,    // One-sided tolerance
    Limit          // Upper and lower limits
};

enum class ArrowType {
    Closed,        // Closed arrow (solid)
    OpenArrow,     // Open arrow
    Dot,           // Dot
    Slash,         // Slash
    Closed30       // Closed 30-degree arrow
};

// ============================================================================
// Annotation Types
// ============================================================================

enum class AnnotationType {
    TextNote,           // Text annotation/note
    RevisionCloud,      // Revision cloud
    WeldSymbol,         // Weld symbol
    SurfaceFinish,      // Surface finish symbol
    GDandT,             // Geometric Dimension and Tolerance symbol
    ReferenceTag,       // Reference tag/callout
    Balloon             // Balloon (number circle)
};

enum class WeldSymbolType {
    Fillet,             // Fillet weld
    Bevel,              // Bevel weld
    VGroove,            // V-groove
    UGroove,            // U-groove
    Plug                // Plug weld
};

enum class SurfaceFinishType {
    Machined,           // Machined surface
    GroundPolished,     // Ground/polished surface
    LappedHoned,        // Lapped/honed surface
    VeryRough           // Very rough surface
};

enum class GDandTType {
    Straightness,       // Straightness
    Flatness,           // Flatness
    Circularity,        // Circularity
    Cylindricity,       // Cylindricity
    Perpendicularity,   // Perpendicularity
    Angularity,         // Angularity
    Parallelism,        // Parallelism
    RunOut,             // Runout
    Position,           // Position
    Profile             // Profile of a surface/line
};

// ============================================================================
// Tolerance Specification
// ============================================================================

struct ToleranceSpec {
    ToleranceType type = ToleranceType::None;
    double upper_limit = 0.0;    // For bilateral and limit
    double lower_limit = 0.0;    // For unilateral and limit
    std::string note;            // Optional tolerance note

    bool isValid() const;
    std::string toString() const;
};

// ============================================================================
// Dimension Properties
// ============================================================================

struct DimensionProperties {
    // Value and unit
    double value = 0.0;
    std::string unit = "mm";      // e.g., "mm", "inch", "m"
    int precision = 1;             // Decimal places
    bool show_dimension = true;

    // Tolerance
    ToleranceSpec tolerance;

    // Text formatting
    std::string text_prefix;       // e.g., "Ø" for diameter
    std::string text_suffix;       // e.g., "MAX", "MIN"
    double text_height = 3.5;      // mm

    // Dimension line appearance
    ArrowType arrow_type = ArrowType::Closed;
    double arrow_size = 2.5;       // mm
    std::string line_style = "solid";
    std::string line_color = "black";
    double line_width = 0.35;      // mm

    // Dimension placement
    Vec2 offset{0, 0};             // Offset from default position
    double rotation = 0.0;         // Rotation in degrees
    bool flip_side = false;        // Dimension on other side

    std::string toString() const;
};

// ============================================================================
// Dimension Class
// ============================================================================

class Dimension {
public:
    explicit Dimension(const std::string& id, DimensionType type);
    virtual ~Dimension() = default;

    // Identity
    const std::string& getId() const { return id_; }
    DimensionType getType() const { return type_; }

    // Value and properties
    double getValue() const { return properties_.value; }
    void setValue(double value) { properties_.value = value; }

    DimensionProperties& getProperties() { return properties_; }
    const DimensionProperties& getProperties() const { return properties_; }

    // Geometry links
    void addLinkedEntity(const std::string& entity_id) { linked_entities_.push_back(entity_id); }
    const std::vector<std::string>& getLinkedEntities() const { return linked_entities_; }
    void clearLinkedEntities() { linked_entities_.clear(); }

    // Visibility and status
    bool isVisible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }

    bool isDriven() const { return driven_; }
    void setDriven(bool driven) { driven_ = driven; }

    // Parent view
    void setParentViewId(const std::string& view_id) { parent_view_id_ = view_id; }
    const std::string& getParentViewId() const { return parent_view_id_; }

    // Update when geometry changes
    virtual void updateFromGeometry() {}

protected:
    std::string id_;
    DimensionType type_;
    DimensionProperties properties_;
    std::vector<std::string> linked_entities_;  // 3D entities this dimension is linked to
    bool visible_ = true;
    bool driven_ = false;  // true = driven by geometry, false = reference
    std::string parent_view_id_;
};

// ============================================================================
// Linear Dimension
// ============================================================================

class LinearDimension : public Dimension {
public:
    explicit LinearDimension(const std::string& id, LinearDimensionSubType sub_type);

    LinearDimensionSubType getSubType() const { return sub_type_; }

    // Reference points
    void setReferencePoints(const Vec2& p0, const Vec2& p1) {
        p0_ = p0;
        p1_ = p1;
    }
    const Vec2& getP0() const { return p0_; }
    const Vec2& getP1() const { return p1_; }

    // Dimension line position
    void setDimensionLineOffset(double offset) { dimension_line_offset_ = offset; }
    double getDimensionLineOffset() const { return dimension_line_offset_; }

    // Calculate dimension value from points
    void updateValue();

private:
    LinearDimensionSubType sub_type_;
    Vec2 p0_{0, 0};
    Vec2 p1_{0, 0};
    double dimension_line_offset_ = 10.0;  // Distance from object to dimension line
};

// ============================================================================
// Radial Dimension (Radius/Diameter)
// ============================================================================

class RadialDimension : public Dimension {
public:
    explicit RadialDimension(const std::string& id, bool is_diameter = false);

    bool isDiameter() const { return is_diameter_; }

    // Circle center and reference point
    void setCenter(const Vec2& center) { center_ = center; }
    void setReferencePoint(const Vec2& point) { reference_point_ = point; }

    const Vec2& getCenter() const { return center_; }
    const Vec2& getReferencePoint() const { return reference_point_; }

    // Calculate dimension value from geometry
    void updateValue();

private:
    bool is_diameter_;
    Vec2 center_{0, 0};
    Vec2 reference_point_{0, 0};
};

// ============================================================================
// Angular Dimension
// ============================================================================

class AngularDimension : public Dimension {
public:
    explicit AngularDimension(const std::string& id);

    // Reference lines/angles
    void setReferenceLines(const Vec2& p1_start, const Vec2& p1_end,
                          const Vec2& p2_start, const Vec2& p2_end) {
        p1_start_ = p1_start;
        p1_end_ = p1_end;
        p2_start_ = p2_start;
        p2_end_ = p2_end;
    }

    Vec2 getP1Start() const { return p1_start_; }
    Vec2 getP1End() const { return p1_end_; }
    Vec2 getP2Start() const { return p2_start_; }
    Vec2 getP2End() const { return p2_end_; }

    // Vertex point (where lines meet)
    void setVertexPoint(const Vec2& vertex) { vertex_ = vertex; }
    const Vec2& getVertexPoint() const { return vertex_; }

    // Calculate angle from geometry
    void updateValue();

private:
    Vec2 p1_start_{0, 0};
    Vec2 p1_end_{0, 0};
    Vec2 p2_start_{0, 0};
    Vec2 p2_end_{0, 0};
    Vec2 vertex_{0, 0};
};

// ============================================================================
// Ordinate Dimension
// ============================================================================

class OrdinateDimension : public Dimension {
public:
    explicit OrdinateDimension(const std::string& id, bool is_horizontal = true);

    bool isHorizontal() const { return is_horizontal_; }

    // Datum and reference point
    void setDatum(const Vec2& datum) { datum_ = datum; }
    void setReferencePoint(const Vec2& point) { reference_point_ = point; }

    const Vec2& getDatum() const { return datum_; }
    const Vec2& getReferencePoint() const { return reference_point_; }

    // Calculate offset from datum
    void updateValue();

private:
    bool is_horizontal_;
    Vec2 datum_{0, 0};
    Vec2 reference_point_{0, 0};
};

// ============================================================================
// Leader Dimension
// ============================================================================

class LeaderDimension : public Dimension {
public:
    explicit LeaderDimension(const std::string& id);

    // Leader line points
    void setLeaderPoints(const std::vector<Vec2>& points) { leader_points_ = points; }
    const std::vector<Vec2>& getLeaderPoints() const { return leader_points_; }

    // Text annotation
    void setText(const std::string& text) { text_ = text; }
    const std::string& getText() const { return text_; }

private:
    std::vector<Vec2> leader_points_;
    std::string text_;
};

// ============================================================================
// Annotation Base Class
// ============================================================================

class Annotation {
public:
    explicit Annotation(const std::string& id, AnnotationType type);
    virtual ~Annotation() = default;

    // Identity
    const std::string& getId() const { return id_; }
    AnnotationType getType() const { return type_; }

    // Position
    void setPosition(const Vec2& pos) { position_ = pos; }
    const Vec2& getPosition() const { return position_; }

    // Visibility and appearance
    bool isVisible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }

    std::string getStyle() const { return style_; }
    void setStyle(const std::string& style) { style_ = style; }

    std::string getColor() const { return color_; }
    void setColor(const std::string& color) { color_ = color; }

    // Parent view
    void setParentViewId(const std::string& view_id) { parent_view_id_ = view_id; }
    const std::string& getParentViewId() const { return parent_view_id_; }

protected:
    std::string id_;
    AnnotationType type_;
    Vec2 position_{0, 0};
    bool visible_ = true;
    std::string style_;
    std::string color_ = "black";
    std::string parent_view_id_;
};

// ============================================================================
// Text Annotation
// ============================================================================

class TextAnnotation : public Annotation {
public:
    explicit TextAnnotation(const std::string& id);

    void setText(const std::string& text) { text_ = text; }
    const std::string& getText() const { return text_; }

    void setTextHeight(double height) { text_height_ = height; }
    double getTextHeight() const { return text_height_; }

    void setAnchorPoint(const std::string& anchor) { anchor_point_ = anchor; }
    const std::string& getAnchorPoint() const { return anchor_point_; }

    void setRotation(double rotation) { rotation_ = rotation; }
    double getRotation() const { return rotation_; }

private:
    std::string text_;
    double text_height_ = 3.5;
    std::string anchor_point_ = "middle";  // start, middle, end
    double rotation_ = 0.0;
};

// ============================================================================
// Revision Cloud Annotation
// ============================================================================

class RevisionCloudAnnotation : public Annotation {
public:
    explicit RevisionCloudAnnotation(const std::string& id);

    void setContourPoints(const std::vector<Vec2>& points) { contour_points_ = points; }
    const std::vector<Vec2>& getContourPoints() const { return contour_points_; }

    void setRevisionNumber(int number) { revision_number_ = number; }
    int getRevisionNumber() const { return revision_number_; }

private:
    std::vector<Vec2> contour_points_;
    int revision_number_ = 1;
};

// ============================================================================
// Weld Symbol Annotation
// ============================================================================

class WeldSymbolAnnotation : public Annotation {
public:
    explicit WeldSymbolAnnotation(const std::string& id);

    void setWeldType(WeldSymbolType type) { weld_type_ = type; }
    WeldSymbolType getWeldType() const { return weld_type_; }

    void setSize(double size) { size_ = size; }
    double getSize() const { return size_; }

    void setLength(double length) { length_ = length; }
    double getLength() const { return length_; }

    void setArrowSide(bool arrow_side) { arrow_side_ = arrow_side; }
    bool getArrowSide() const { return arrow_side_; }

    void setOtherSide(bool other_side) { other_side_ = other_side; }
    bool hasOtherSide() const { return other_side_; }

private:
    WeldSymbolType weld_type_;
    double size_ = 0.0;
    double length_ = 0.0;
    bool arrow_side_ = false;
    bool other_side_ = false;
};

// ============================================================================
// Surface Finish Annotation
// ============================================================================

class SurfaceFinishAnnotation : public Annotation {
public:
    explicit SurfaceFinishAnnotation(const std::string& id);

    void setFinishType(SurfaceFinishType type) { finish_type_ = type; }
    SurfaceFinishType getFinishType() const { return finish_type_; }

    void setRoughnessValue(double value) { roughness_value_ = value; }
    double getRoughnessValue() const { return roughness_value_; }

    void setNote(const std::string& note) { note_ = note; }
    const std::string& getNote() const { return note_; }

private:
    SurfaceFinishType finish_type_;
    double roughness_value_ = 0.0;
    std::string note_;
};

// ============================================================================
// GD&T Symbol Annotation
// ============================================================================

class GDandTAnnotation : public Annotation {
public:
    explicit GDandTAnnotation(const std::string& id);

    void setToleranceType(GDandTType type) { tolerance_type_ = type; }
    GDandTType getToleranceType() const { return tolerance_type_; }

    void setToleranceValue(double value) { tolerance_value_ = value; }
    double getToleranceValue() const { return tolerance_value_; }

    void setDatumReference(const std::string& datum) { datum_reference_ = datum; }
    const std::string& getDatumReference() const { return datum_reference_; }

private:
    GDandTType tolerance_type_;
    double tolerance_value_ = 0.0;
    std::string datum_reference_;
};

// ============================================================================
// Reference Tag/Callout Annotation
// ============================================================================

class ReferenceTagAnnotation : public Annotation {
public:
    explicit ReferenceTagAnnotation(const std::string& id);

    void setLabel(const std::string& label) { label_ = label; }
    const std::string& getLabel() const { return label_; }

    void setReferencedEntity(const std::string& entity_id) { referenced_entity_id_ = entity_id; }
    const std::string& getReferencedEntity() const { return referenced_entity_id_; }

private:
    std::string label_;
    std::string referenced_entity_id_;
};

// ============================================================================
// Balloon Annotation
// ============================================================================

class BalloonAnnotation : public Annotation {
public:
    explicit BalloonAnnotation(const std::string& id);

    void setNumber(int number) { number_ = number; }
    int getNumber() const { return number_; }

    void setRadius(double radius) { radius_ = radius; }
    double getRadius() const { return radius_; }

    void setLeaderPoint(const Vec2& point) { leader_point_ = point; }
    const Vec2& getLeaderPoint() const { return leader_point_; }

private:
    int number_ = 1;
    double radius_ = 5.0;
    Vec2 leader_point_{0, 0};
};

// ============================================================================
// Auto-Dimensioning System
// ============================================================================

/**
 * @brief Auto-Dimensioning configuration
 */
struct AutoDimensioningConfig {
    int min_spacing = 5;            // Minimum spacing between dimensions (mm)
    int secondary_spacing = 8;      // Secondary dimension line spacing (mm)
    bool avoid_overlaps = true;
    bool align_dimensions = true;
    int precision = 1;
    std::string unit = "mm";
};

/**
 * @brief Dimension Manager for a view
 *
 * Manages all dimensions and annotations for a single drawing view
 */
class DimensionManager {
public:
    explicit DimensionManager(const std::string& view_id);

    // Dimension management
    std::shared_ptr<Dimension> addDimension(std::shared_ptr<Dimension> dim);
    std::shared_ptr<Dimension> getDimension(const std::string& id) const;
    void removeDimension(const std::string& id);
    const std::vector<std::shared_ptr<Dimension>>& getDimensions() const { return dimensions_; }

    // Annotation management
    std::shared_ptr<Annotation> addAnnotation(std::shared_ptr<Annotation> annotation);
    std::shared_ptr<Annotation> getAnnotation(const std::string& id) const;
    void removeAnnotation(const std::string& id);
    const std::vector<std::shared_ptr<Annotation>>& getAnnotations() const { return annotations_; }

    // Auto-dimensioning
    std::vector<std::shared_ptr<Dimension>> autoDimension(
        const AutoDimensioningConfig& config = AutoDimensioningConfig()
    );

    // Dimension search and filtering
    std::vector<std::shared_ptr<Dimension>> getDimensionsByType(DimensionType type) const;
    std::vector<std::shared_ptr<Dimension>> getDimensionsForEntity(const std::string& entity_id) const;

    // Geometry linking
    void linkDimensionToEntity(const std::string& dim_id, const std::string& entity_id);
    void unlinkDimensionFromEntity(const std::string& dim_id, const std::string& entity_id);

    // Collision detection and spacing
    bool checkDimensionOverlap(const std::shared_ptr<Dimension>& dim) const;
    void adjustDimensionPlacement(std::shared_ptr<Dimension>& dim);

    // Clear all
    void clear();
    size_t getTotalElementCount() const { return dimensions_.size() + annotations_.size(); }

private:
    std::string view_id_;
    std::vector<std::shared_ptr<Dimension>> dimensions_;
    std::vector<std::shared_ptr<Annotation>> annotations_;
    std::map<std::string, std::vector<std::string>> entity_dimension_map_;  // entity_id -> dimension_ids
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create a linear dimension with automatic value calculation
 */
std::shared_ptr<LinearDimension> createLinearDimension(
    const std::string& id,
    LinearDimensionSubType sub_type,
    const Vec2& p0,
    const Vec2& p1
);

/**
 * @brief Create a radial dimension (radius or diameter)
 */
std::shared_ptr<RadialDimension> createRadialDimension(
    const std::string& id,
    const Vec2& center,
    const Vec2& reference_point,
    bool is_diameter = false
);

/**
 * @brief Create an angular dimension
 */
std::shared_ptr<AngularDimension> createAngularDimension(
    const std::string& id,
    const Vec2& p1_start,
    const Vec2& p1_end,
    const Vec2& p2_start,
    const Vec2& p2_end
);

/**
 * @brief Calculate distance between two points
 */
double calculateDistance(const Vec2& p0, const Vec2& p1);

/**
 * @brief Calculate angle between two lines
 */
double calculateAngle(const Vec2& p1_start, const Vec2& p1_end,
                     const Vec2& p2_start, const Vec2& p2_end);

/**
 * @brief Format dimension value with unit and precision
 */
std::string formatDimensionValue(double value, int precision, const std::string& unit);

/**
 * @brief Convert dimension type to string
 */
std::string dimensionTypeToString(DimensionType type);

/**
 * @brief Convert annotation type to string
 */
std::string annotationTypeToString(AnnotationType type);

} // namespace BendCore::Drawing
