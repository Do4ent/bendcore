// M4 Phase - Layout Manager (GAP-017)
// Layout management, view arrangement, and positioning algorithms
#pragma once

#include "drawing_extraction.hpp"
#include "view_manager.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <optional>

namespace BendCore::Drawing {

using Vec2 = std::array<double, 2>;

// ============================================================================
// Layout Templates and Presets
// ============================================================================

enum class LayoutTemplate {
    SingleView,      // One view, centered
    TwoView,         // Two views (Front, Top)
    ThreeView,       // Standard 3-view (Front, Top, Right)
    FourView,        // Four views
    SixView,         // Six orthographic views
    Isometric,       // Isometric view
    SectionDetail,   // Section view with detail
    ExplodedAssembly, // Exploded assembly layout
    CustomGrid,      // Custom grid layout
    FreeLayout       // Free positioning without constraints
};

// ============================================================================
// Layout Configuration
// ============================================================================

struct LayoutConfig {
    LayoutTemplate template_type = LayoutTemplate::FreeLayout;

    // Spacing and margins
    double margin_left = 15.0;      // Left margin in mm
    double margin_right = 15.0;     // Right margin in mm
    double margin_top = 15.0;       // Top margin in mm
    double margin_bottom = 15.0;    // Bottom margin in mm

    double horizontal_spacing = 10.0; // Spacing between views horizontally
    double vertical_spacing = 10.0;   // Spacing between views vertically

    // Alignment
    bool align_to_grid = false;
    double grid_size = 5.0;         // Grid size in mm
    bool center_views = false;      // Center views on sheet

    // View sizing
    double min_view_size = 10.0;    // Minimum view size in mm
    double max_view_size = 200.0;   // Maximum view size in mm
    bool auto_scale = true;         // Auto-scale views to fit
    bool uniform_scale = false;     // Force all views to same scale

    // Layout behavior
    bool allow_overlap = false;
    bool maintain_aspect_ratio = true;
    bool snap_to_edges = false;
};

// ============================================================================
// Layout Constraint Types
// ============================================================================

enum class ConstraintType {
    FixedPosition,      // View at fixed position
    FixedSize,          // View at fixed size
    AlignLeft,          // Align left edges
    AlignRight,         // Align right edges
    AlignTop,           // Align top edges
    AlignBottom,        // Align bottom edges
    AlignCenterX,       // Align horizontal centers
    AlignCenterY,       // Align vertical centers
    EqualWidth,         // All views equal width
    EqualHeight,        // All views equal height
    HorizontalSpacing,  // Fixed spacing horizontally
    VerticalSpacing,    // Fixed spacing vertically
    Distribute,         // Distribute evenly
    RelativePosition    // Position relative to another view
};

struct LayoutConstraint {
    std::string view_id;
    ConstraintType type;
    double value = 0.0;           // Constraint value
    std::vector<std::string> related_views;  // For relative constraints
    int priority = 0;             // Higher = more important
};

// ============================================================================
// Layout Solution
// ============================================================================

struct ViewLayout {
    std::string view_id;
    Vec2 position;                // Position on sheet (mm)
    double width;                 // View width (mm)
    double height;                // View height (mm)
    double scale;                 // View scale
};

struct LayoutSolution {
    std::vector<ViewLayout> view_layouts;
    bool is_valid = true;
    std::string error_message;
};

// ============================================================================
// Layout Manager
// ============================================================================

class LayoutManager {
public:
    LayoutManager(const SheetProperties& sheet_props);

    // Configuration
    const LayoutConfig& getConfig() const;
    void setConfig(const LayoutConfig& config);

    const SheetProperties& getSheetProperties() const { return sheet_props_; }
    void setSheetProperties(const SheetProperties& props) { sheet_props_ = props; }

    // Template-based layout
    LayoutSolution arrangeViewsByTemplate(
        const std::vector<std::shared_ptr<DrawingView>>& views,
        LayoutTemplate template_type
    );

    // Constraint-based layout
    void addConstraint(const LayoutConstraint& constraint);
    void removeConstraint(const std::string& view_id, ConstraintType type);
    void clearConstraints();

    LayoutSolution solveConstraints(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    // Manual layout
    LayoutSolution arrangeViewsManually(
        const std::vector<std::shared_ptr<DrawingView>>& views,
        const std::map<std::string, Vec2>& positions
    );

    // Grid-based layout
    LayoutSolution arrangeViewsInGrid(
        const std::vector<std::shared_ptr<DrawingView>>& views,
        int columns,
        int rows
    );

    // Line-based layout (vertical or horizontal)
    LayoutSolution arrangeViewsInLine(
        const std::vector<std::shared_ptr<DrawingView>>& views,
        bool horizontal = true
    );

    // Alignment operations
    void alignViews(const std::vector<std::string>& view_ids, ConstraintType alignment);
    void distributeViews(const std::vector<std::string>& view_ids, bool horizontally = true);
    void spaceViewsEvenly(const std::vector<std::string>& view_ids, bool horizontally = true);

    // View sizing
    double calculateOptimalViewScale(const DrawingView& view) const;
    void resizeViewToFit(DrawingView& view, double available_width, double available_height);
    void uniformScaleViews(std::vector<std::shared_ptr<DrawingView>>& views);

    // Position calculation
    Vec2 calculateNextPosition(size_t view_index, size_t total_views) const;
    Vec2 getAvailableSpace(const DrawingView& view) const;

    // Collision detection
    bool checkOverlap(const ViewLayout& layout1, const ViewLayout& layout2) const;
    bool checkFitsOnSheet(const ViewLayout& layout) const;
    std::vector<std::pair<size_t, size_t>> findOverlappingViews(
        const std::vector<ViewLayout>& layouts
    ) const;

    // Layout validation
    bool validateLayout(const LayoutSolution& solution) const;
    std::vector<std::string> validateAllConstraints(
        const std::vector<std::shared_ptr<DrawingView>>& views
    ) const;

    // Utility functions
    struct AvailableRectangle {
        Vec2 position;
        double width;
        double height;
    };

    std::vector<AvailableRectangle> findAvailableRectangles(
        const std::vector<ViewLayout>& existing_layouts
    ) const;

    // Apply layout solution
    void applyLayout(
        std::vector<std::shared_ptr<DrawingView>>& views,
        const LayoutSolution& solution
    );

private:
    LayoutConfig config_;
    SheetProperties sheet_props_;
    std::vector<LayoutConstraint> constraints_;

    // Layout algorithms
    LayoutSolution layoutSingleView(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    LayoutSolution layoutTwoView(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    LayoutSolution layoutThreeView(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    LayoutSolution layoutSixView(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    LayoutSolution layoutIsometric(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    LayoutSolution layoutSectionDetail(
        const std::vector<std::shared_ptr<DrawingView>>& views
    );

    // Helper methods
    double getWorkspaceWidth() const;
    double getWorkspaceHeight() const;

    bool isViewPositionValid(const ViewLayout& layout) const;
    std::vector<ViewLayout> sortByPosition(const std::vector<ViewLayout>& layouts) const;

public:
    // Preset template generators
    static LayoutConfig getPresetConfig(LayoutTemplate template_type);
};

// ============================================================================
// Layout History for Undo/Redo
// ============================================================================

class LayoutHistory {
public:
    void recordLayout(const std::string& layout_name, const LayoutSolution& solution);
    const LayoutSolution& getLayout(const std::string& layout_name) const;
    std::vector<std::string> getLayoutNames() const;
    void clearHistory();

private:
    std::map<std::string, LayoutSolution> layouts_;
};

} // namespace BendCore::Drawing
