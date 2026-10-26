#pragma once

#include "../document/cad_document.hpp"
#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>
#include <chrono>

namespace BendCore::CadCore {

/**
 * @brief Drawing sheet size standards (ISO and ANSI)
 */
enum class SheetSize {
    ISO_A0,      // 841 x 1189 mm
    ISO_A1,      // 594 x 841 mm
    ISO_A2,      // 420 x 594 mm
    ISO_A3,      // 297 x 420 mm
    ISO_A4,      // 210 x 297 mm
    ISO_A5,      // 148 x 210 mm
    ANSI_A,      // 8.5" x 11" (216 x 279 mm)
    ANSI_B,      // 11" x 17" (279 x 432 mm)
    ANSI_C,      // 17" x 22" (432 x 559 mm)
    ANSI_D,      // 22" x 34" (559 x 864 mm)
    ANSI_E,      // 34" x 44" (864 x 1118 mm)
    CUSTOM       // Custom size
};

/**
 * @brief Drawing units
 */
enum class DrawingUnits {
    MILLIMETERS,
    INCHES,
    CENTIMETERS,
    METERS
};

/**
 * @brief View projection methods
 */
enum class ProjectionMethod {
    FIRST_ANGLE,   // European/ISO standard
    THIRD_ANGLE    // American/ANSI standard
};

/**
 * @brief Sheet properties
 */
struct SheetProperties {
    SheetSize size;
    DrawingUnits units;
    double width_mm;      // Width in millimeters
    double height_mm;     // Height in millimeters
    double left_margin;   // Margin from left edge
    double right_margin;  // Margin from right edge
    double top_margin;    // Margin from top edge
    double bottom_margin; // Margin from bottom edge
    std::string title;    // Sheet title
    uint32_t sheet_number;// Sheet number in drawing
    uint32_t total_sheets;// Total sheets in drawing
    bool has_border;      // Whether sheet has border
    bool has_title_block; // Whether sheet has title block

    SheetProperties();
    explicit SheetProperties(SheetSize sz);
};

/**
 * @brief View properties
 */
struct ViewProperties {
    double position_x;      // X position on sheet
    double position_y;      // Y position on sheet
    double width;           // View width
    double height;          // View height
    double scale;           // View scale factor
    double rotation;        // View rotation in degrees
    bool is_visible;        // View visibility
    std::string name;       // View name
    uint32_t layer_mask;    // Which layers to display

    ViewProperties();
};

/**
 * @brief Drawing sheet containing multiple views
 */
class DrawingSheet {
public:
    /**
     * @brief Constructor
     */
    DrawingSheet(const EntityId& id, const std::string& name = "");

    /**
     * @brief Get sheet ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get sheet name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set sheet name
     */
    void setName(const std::string& name) noexcept {
        name_ = name;
        incrementRevision();
    }

    /**
     * @brief Get sheet properties
     */
    [[nodiscard]] const SheetProperties& getProperties() const noexcept { return properties_; }

    /**
     * @brief Set sheet properties
     */
    void setProperties(const SheetProperties& props) noexcept {
        properties_ = props;
        incrementRevision();
    }

    /**
     * @brief Get sheet revision
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment sheet revision
     */
    uint64_t incrementRevision() noexcept;

    /**
     * @brief Add view to sheet
     */
    void addView(const EntityId& view_id, const ViewProperties& props);

    /**
     * @brief Remove view from sheet
     */
    void removeView(const EntityId& view_id);

    /**
     * @brief Get view properties
     */
    [[nodiscard]] std::optional<ViewProperties> getViewProperties(const EntityId& view_id) const noexcept;

    /**
     * @brief Update view properties
     */
    void updateViewProperties(const EntityId& view_id, const ViewProperties& props);

    /**
     * @brief Get all view IDs on sheet
     */
    [[nodiscard]] std::vector<EntityId> getViewIds() const;

    /**
     * @brief Get view count
     */
    [[nodiscard]] size_t getViewCount() const noexcept { return views_.size(); }

    /**
     * @brief Clear all views
     */
    void clearViews() noexcept {
        views_.clear();
        incrementRevision();
    }

    /**
     * @brief Get sheet metadata
     */
    [[nodiscard]] std::optional<std::string> getMetadata(const std::string& key) const noexcept;

    /**
     * @brief Set sheet metadata
     */
    void setMetadata(const std::string& key, const std::string& value) noexcept;

    /**
     * @brief Get creation time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getCreationTime() const noexcept { return created_time_; }

    /**
     * @brief Get modification time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getModificationTime() const noexcept { return modified_time_; }

    // Friend declaration for serialization
    friend class DrawingManager;

private:
    EntityId id_;
    std::string name_;
    SheetProperties properties_;
    uint64_t revision_;
    std::map<EntityId, ViewProperties> views_;           // View ID -> View Properties
    std::map<std::string, std::string> metadata_;        // Metadata key -> value
    std::chrono::system_clock::time_point created_time_;
    std::chrono::system_clock::time_point modified_time_;
};

/**
 * @brief Drawing view linking to 3D geometry
 */
class DrawingView {
public:
    /**
     * @brief Constructor
     */
    DrawingView(const EntityId& id, const EntityId& source_geometry_id, const std::string& name = "");

    /**
     * @brief Get view ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get source geometry ID (part/assembly reference)
     */
    [[nodiscard]] const EntityId& getSourceGeometryId() const noexcept { return source_geometry_id_; }

    /**
     * @brief Get view name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set view name
     */
    void setName(const std::string& name) noexcept {
        name_ = name;
        incrementRevision();
    }

    /**
     * @brief Get view properties
     */
    [[nodiscard]] const ViewProperties& getProperties() const noexcept { return properties_; }

    /**
     * @brief Set view properties
     */
    void setProperties(const ViewProperties& props) noexcept {
        properties_ = props;
        incrementRevision();
    }

    /**
     * @brief Get view revision
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment view revision
     */
    uint64_t incrementRevision() noexcept;

    /**
     * @brief Set view clipping region
     */
    void setClippingRegion(double x, double y, double width, double height) noexcept;

    /**
     * @brief Get clipping region
     */
    [[nodiscard]] bool hasClippingRegion() const noexcept { return has_clipping_; }

    /**
     * @brief Get clipping bounds
     */
    [[nodiscard]] std::tuple<double, double, double, double> getClippingBounds() const noexcept {
        return {clip_x_, clip_y_, clip_width_, clip_height_};
    }

    /**
     * @brief Clear clipping region
     */
    void clearClippingRegion() noexcept {
        has_clipping_ = false;
        incrementRevision();
    }

    /**
     * @brief Mark view as needing update
     */
    void markDirty() noexcept { is_dirty_ = true; }

    /**
     * @brief Check if view needs update
     */
    [[nodiscard]] bool isDirty() const noexcept { return is_dirty_; }

    /**
     * @brief Mark view as updated
     */
    void markClean() noexcept { is_dirty_ = false; }

    /**
     * @brief Get view metadata
     */
    [[nodiscard]] std::optional<std::string> getMetadata(const std::string& key) const noexcept;

    /**
     * @brief Set view metadata
     */
    void setMetadata(const std::string& key, const std::string& value) noexcept;

private:
    EntityId id_;
    EntityId source_geometry_id_;  // Link to source 3D geometry
    std::string name_;
    ViewProperties properties_;
    uint64_t revision_;
    bool has_clipping_;
    double clip_x_, clip_y_, clip_width_, clip_height_;
    bool is_dirty_;                                    // Needs regeneration
    std::map<std::string, std::string> metadata_;      // Metadata key -> value
};

/**
 * @brief Main Drawing document class
 *
 * Drawing inherits from CadDocument and represents a 2D drawing
 * document containing sheets and views that reference 3D geometry.
 */
class Drawing {
public:
    /**
     * @brief Constructor
     */
    Drawing(const EntityId& id, const std::string& name = "");

    /**
     * @brief Get drawing ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get drawing name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set drawing name
     */
    void setName(const std::string& name) noexcept {
        name_ = name;
        incrementRevision();
    }

    /**
     * @brief Get drawing title
     */
    [[nodiscard]] const std::string& getTitle() const noexcept { return title_; }

    /**
     * @brief Set drawing title
     */
    void setTitle(const std::string& title) noexcept {
        title_ = title;
        incrementRevision();
    }

    /**
     * @brief Get associated assembly/part ID
     */
    [[nodiscard]] const EntityId& getSourceId() const noexcept { return source_id_; }

    /**
     * @brief Set associated assembly/part ID
     */
    void setSourceId(const EntityId& source_id) noexcept {
        source_id_ = source_id;
        incrementRevision();
    }

    /**
     * @brief Get drawing scale
     */
    [[nodiscard]] double getScale() const noexcept { return scale_; }

    /**
     * @brief Set drawing scale
     */
    void setScale(double scale) noexcept {
        if (scale > 0.0) {
            scale_ = scale;
            incrementRevision();
        }
    }

    /**
     * @brief Get drawing units
     */
    [[nodiscard]] DrawingUnits getUnits() const noexcept { return units_; }

    /**
     * @brief Set drawing units
     */
    void setUnits(DrawingUnits units) noexcept {
        units_ = units;
        incrementRevision();
    }

    /**
     * @brief Get projection method
     */
    [[nodiscard]] ProjectionMethod getProjectionMethod() const noexcept { return projection_; }

    /**
     * @brief Set projection method
     */
    void setProjectionMethod(ProjectionMethod proj) noexcept {
        projection_ = proj;
        incrementRevision();
    }

    /**
     * @brief Get revision number
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment revision
     */
    uint64_t incrementRevision() noexcept;

    /**
     * @brief Add sheet to drawing
     */
    std::shared_ptr<DrawingSheet> addSheet(const std::string& name = "");

    /**
     * @brief Remove sheet from drawing
     */
    void removeSheet(const EntityId& sheet_id);

    /**
     * @brief Get sheet by ID
     */
    [[nodiscard]] std::shared_ptr<DrawingSheet> getSheet(const EntityId& sheet_id);

    /**
     * @brief Get sheet by ID (const version)
     */
    [[nodiscard]] std::shared_ptr<const DrawingSheet> getSheet(const EntityId& sheet_id) const;

    /**
     * @brief Get all sheet IDs
     */
    [[nodiscard]] std::vector<EntityId> getSheetIds() const;

    /**
     * @brief Get sheet count
     */
    [[nodiscard]] size_t getSheetCount() const noexcept { return sheets_.size(); }

    /**
     * @brief Add view to drawing
     */
    std::shared_ptr<DrawingView> addView(const EntityId& source_geometry_id, const std::string& name = "");

    /**
     * @brief Remove view from drawing
     */
    void removeView(const EntityId& view_id);

    /**
     * @brief Get view by ID
     */
    [[nodiscard]] std::shared_ptr<DrawingView> getView(const EntityId& view_id);

    /**
     * @brief Get view by ID (const version)
     */
    [[nodiscard]] std::shared_ptr<const DrawingView> getView(const EntityId& view_id) const;

    /**
     * @brief Get all view IDs
     */
    [[nodiscard]] std::vector<EntityId> getViewIds() const;

    /**
     * @brief Get view count
     */
    [[nodiscard]] size_t getViewCount() const noexcept { return views_.size(); }

    /**
     * @brief Get drawing metadata
     */
    [[nodiscard]] std::optional<std::string> getMetadata(const std::string& key) const noexcept;

    /**
     * @brief Set drawing metadata
     */
    void setMetadata(const std::string& key, const std::string& value) noexcept;

    /**
     * @brief Get all metadata
     */
    [[nodiscard]] const std::map<std::string, std::string>& getMetadataMap() const noexcept {
        return metadata_;
    }

    /**
     * @brief Get creation time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getCreationTime() const noexcept { return created_time_; }

    /**
     * @brief Get modification time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getModificationTime() const noexcept { return modified_time_; }

    /**
     * @brief Validate drawing integrity
     */
    [[nodiscard]] std::string validate() const noexcept;

    /**
     * @brief Mark all views as dirty
     */
    void markAllViewsDirty() noexcept;

    /**
     * @brief Clear all sheets and views
     */
    void clear() noexcept {
        sheets_.clear();
        views_.clear();
        incrementRevision();
    }

    // Friend declaration for serialization
    friend class DrawingManager;

private:
    EntityId id_;
    std::string name_;
    std::string title_;
    EntityId source_id_;                               // Reference to assembly/part
    double scale_;
    DrawingUnits units_;
    ProjectionMethod projection_;
    uint64_t revision_;
    std::map<EntityId, std::shared_ptr<DrawingSheet>> sheets_;  // Sheet ID -> Sheet
    std::map<EntityId, std::shared_ptr<DrawingView>> views_;    // View ID -> View
    std::map<std::string, std::string> metadata_;               // Metadata key -> value
    std::chrono::system_clock::time_point created_time_;
    std::chrono::system_clock::time_point modified_time_;
};

} // namespace BendCore::CadCore
