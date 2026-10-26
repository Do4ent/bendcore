// M4 Phase - View Manager (GAP-017)
// Manages views, their relationships, dependencies, and synchronization
#pragma once

#include "drawing_extraction.hpp"
#include "view_properties.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <functional>

namespace BendCore::Drawing {

using Vec2 = std::array<double, 2>;

// ============================================================================
// View Metadata and Information
// ============================================================================

enum class DetailViewType {
    Circle,       // Circular detail callout
    Rectangle,    // Rectangular detail callout
    Cloud         // Cloud callout
};

struct DetailViewInfo {
    std::string parent_view_id;    // ID of parent view
    Vec2 callout_center;           // Center of detail callout on parent
    double callout_radius = 10.0;  // Radius/size of callout in mm
    DetailViewType callout_type = DetailViewType::Circle;
    Vec2 reference_arrow_offset{0, -10}; // Offset for reference arrow
};

struct SectionViewInfo {
    std::string parent_view_id;    // ID of parent view
    Vec2 section_line_start;       // Start of section line on parent
    Vec2 section_line_end;         // End of section line on parent
    std::string section_identifier = "A"; // e.g., "A", "B", "A-A"
};

struct ExplodedViewInfo {
    std::vector<std::string> component_ids;  // Components visible in exploded view
    double explosion_scale = 2.0;  // Scale of explosion
    std::string explosion_direction = "isometric"; // Direction of explosion
};

// ============================================================================
// View Metadata Container
// ============================================================================

class ViewMetadata {
public:
    ViewMetadata() = default;
    ViewMetadata(const std::string& view_id);

    // Identity
    const std::string& getViewId() const { return view_id_; }
    const std::string& getViewName() const { return view_name_; }
    void setViewName(const std::string& name) { view_name_ = name; }

    // Relationships
    std::string getParentViewId() const { return parent_view_id_; }
    void setParentViewId(const std::string& id) { parent_view_id_ = id; }

    const std::set<std::string>& getDependentViews() const { return dependent_views_; }
    void addDependentView(const std::string& view_id) { dependent_views_.insert(view_id); }
    void removeDependentView(const std::string& view_id) { dependent_views_.erase(view_id); }

    // Detail view information
    const std::optional<DetailViewInfo>& getDetailViewInfo() const { return detail_info_; }
    void setDetailViewInfo(const DetailViewInfo& info) { detail_info_ = info; }

    // Section view information
    const std::optional<SectionViewInfo>& getSectionViewInfo() const { return section_info_; }
    void setSectionViewInfo(const SectionViewInfo& info) { section_info_ = info; }

    // Exploded view information
    const std::optional<ExplodedViewInfo>& getExplodedViewInfo() const { return exploded_info_; }
    void setExplodedViewInfo(const ExplodedViewInfo& info) { exploded_info_ = info; }

    // Visibility state
    bool isVisible() const { return is_visible_; }
    void setVisible(bool visible) { is_visible_ = visible; }

    // Layer visibility for this view
    const std::map<std::string, bool>& getVisibleLayers() const { return visible_layers_; }
    void setLayerVisible(const std::string& layer, bool visible) { visible_layers_[layer] = visible; }

    // Last modification time
    long long getLastModifiedTime() const { return last_modified_; }
    void updateModificationTime();

    // Revision/version tracking
    int getRevision() const { return revision_; }
    void incrementRevision() { revision_++; }

private:
    std::string view_id_;
    std::string view_name_;
    std::string parent_view_id_;
    std::set<std::string> dependent_views_;
    std::optional<DetailViewInfo> detail_info_;
    std::optional<SectionViewInfo> section_info_;
    std::optional<ExplodedViewInfo> exploded_info_;
    bool is_visible_ = true;
    std::map<std::string, bool> visible_layers_;
    long long last_modified_ = 0;
    int revision_ = 0;
};

// ============================================================================
// View Change Notifications
// ============================================================================

enum class ViewChangeType {
    Geometry,       // Geometric elements changed
    Properties,     // Properties changed
    Position,       // Position on sheet changed
    Scale,          // Scale changed
    Visibility,     // Visibility changed
    Appearance,     // Appearance/styling changed
    All             // All aspects changed
};

struct ViewChangeEvent {
    std::string view_id;
    ViewChangeType change_type;
    long long timestamp = 0;
};

// ============================================================================
// View Manager
// ============================================================================

class ViewManager {
public:
    ViewManager();
    ~ViewManager();

    // View registration and management
    void registerView(std::shared_ptr<DrawingView> view);
    void unregisterView(const std::string& view_id);

    std::shared_ptr<DrawingView> getView(const std::string& view_id);
    const std::shared_ptr<DrawingView> getView(const std::string& view_id) const;

    std::vector<std::shared_ptr<DrawingView>> getAllViews();
    const std::vector<std::shared_ptr<DrawingView>>& getAllViews() const;

    size_t getViewCount() const { return views_.size(); }
    bool hasView(const std::string& view_id) const;

    // View metadata
    ViewMetadata& getMetadata(const std::string& view_id);
    const ViewMetadata& getMetadata(const std::string& view_id) const;

    // View appearance
    ViewAppearance& getAppearance(const std::string& view_id);
    const ViewAppearance& getAppearance(const std::string& view_id) const;

    // Positioning and scale
    void setViewPosition(const std::string& view_id, const Vec2& position);
    Vec2 getViewPosition(const std::string& view_id) const;

    void setViewScale(const std::string& view_id, double scale);
    double getViewScale(const std::string& view_id) const;

    // Relationships
    void setViewDependency(const std::string& parent_id, const std::string& child_id);
    void removeDependency(const std::string& parent_id, const std::string& child_id);

    std::vector<std::string> getDependentViews(const std::string& view_id) const;
    std::string getParentView(const std::string& view_id) const;

    // Detail views
    void createDetailView(const std::string& parent_id, const std::string& detail_id,
                         const DetailViewInfo& info);
    void deleteDetailView(const std::string& detail_id);

    // Section views
    void createSectionView(const std::string& parent_id, const std::string& section_id,
                          const SectionViewInfo& info);
    void deleteSectionView(const std::string& section_id);

    // Exploded views
    void createExplodedView(const std::string& exploded_id, const ExplodedViewInfo& info);
    void deleteExplodedView(const std::string& exploded_id);

    // Synchronization
    void syncView(const std::string& view_id);
    void syncAllViews();
    void syncDependentViews(const std::string& parent_id);

    // Batch updates
    void beginBatchUpdate();
    void endBatchUpdate();
    bool isInBatchUpdate() const { return in_batch_update_; }

    // Change notifications
    using ChangeCallback = std::function<void(const ViewChangeEvent&)>;
    void subscribeToChanges(const std::string& view_id, ChangeCallback callback);
    void unsubscribeFromChanges(const std::string& view_id);

    void notifyChange(const ViewChangeEvent& event);

    // View ordering
    void moveViewToFront(const std::string& view_id);
    void moveViewToBack(const std::string& view_id);
    void reorderViews(const std::vector<std::string>& view_ids);

    // Visibility control
    void setViewVisibility(const std::string& view_id, bool visible);
    bool getViewVisibility(const std::string& view_id) const;

    void showAllViews();
    void hideAllViews();

    // Alignment helpers
    struct AlignmentInfo {
        double left = 0;
        double right = 0;
        double top = 0;
        double bottom = 0;
        double centerX = 0;
        double centerY = 0;
    };

    AlignmentInfo calculateViewAlignment(const std::vector<std::string>& view_ids) const;

    // View validation
    bool isViewValid(const std::string& view_id) const;
    std::vector<std::string> getInvalidViews() const;

private:
    std::map<std::string, std::shared_ptr<DrawingView>> views_;
    std::map<std::string, ViewMetadata> metadata_;
    std::map<std::string, ViewAppearance> appearances_;
    std::map<std::string, ChangeCallback> change_callbacks_;
    std::vector<std::string> view_order_;  // For Z-order management

    bool in_batch_update_ = false;
    std::vector<ViewChangeEvent> pending_changes_;

    void ensureMetadataExists(const std::string& view_id);
    void ensureAppearanceExists(const std::string& view_id);
};

} // namespace BendCore::Drawing
