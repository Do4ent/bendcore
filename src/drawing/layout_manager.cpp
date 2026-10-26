// M4 Phase - Layout Manager Implementation
#include "bendcore/drawing/layout_manager.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numbers>

namespace BendCore::Drawing {

// ============================================================================
// LayoutManager Implementation
// ============================================================================

LayoutManager::LayoutManager(const SheetProperties& sheet_props)
    : sheet_props_(sheet_props) {
    config_ = getPresetConfig(LayoutTemplate::FreeLayout);
}

const LayoutConfig& LayoutManager::getConfig() const {
    return config_;
}

void LayoutManager::setConfig(const LayoutConfig& config) {
    config_ = config;
}

LayoutSolution LayoutManager::arrangeViewsByTemplate(
    const std::vector<std::shared_ptr<DrawingView>>& views,
    LayoutTemplate template_type) {

    if (views.empty()) {
        return LayoutSolution{{}, false, "No views provided"};
    }

    switch (template_type) {
        case LayoutTemplate::SingleView:
            return layoutSingleView(views);
        case LayoutTemplate::TwoView:
            return layoutTwoView(views);
        case LayoutTemplate::ThreeView:
            return layoutThreeView(views);
        case LayoutTemplate::FourView:
            return layoutThreeView(views);  // Fallback to 3-view
        case LayoutTemplate::SixView:
            return layoutSixView(views);
        case LayoutTemplate::Isometric:
            return layoutIsometric(views);
        case LayoutTemplate::SectionDetail:
            return layoutSectionDetail(views);
        case LayoutTemplate::ExplodedAssembly:
            return layoutIsometric(views);  // Fallback to isometric
        default:
            return LayoutSolution{{}, false, "Unknown template type"};
    }
}

void LayoutManager::addConstraint(const LayoutConstraint& constraint) {
    constraints_.push_back(constraint);
}

void LayoutManager::removeConstraint(const std::string& view_id, ConstraintType type) {
    auto it = std::remove_if(constraints_.begin(), constraints_.end(),
        [view_id, type](const LayoutConstraint& c) {
            return c.view_id == view_id && c.type == type;
        });
    constraints_.erase(it, constraints_.end());
}

void LayoutManager::clearConstraints() {
    constraints_.clear();
}

LayoutSolution LayoutManager::solveConstraints(
    const std::vector<std::shared_ptr<DrawingView>>& views) {
    // Simple constraint solver
    LayoutSolution solution{{}, true, ""};

    std::map<std::string, ViewLayout> layouts;

    // Initialize layouts from views
    for (const auto& view : views) {
        if (!view) continue;

        auto bbox = view->calculateBoundingBox();
        layouts[view->getId()] = ViewLayout{
            view->getId(),
            view->getOrigin(),
            bbox.width() * view->getScale(),
            bbox.height() * view->getScale(),
            view->getScale()
        };
    }

    // Apply constraints (simplified - just apply basic ones)
    for (const auto& constraint : constraints_) {
        auto it = layouts.find(constraint.view_id);
        if (it == layouts.end()) continue;

        switch (constraint.type) {
            case ConstraintType::FixedPosition:
                it->second.position = {constraint.value, constraint.value};
                break;
            case ConstraintType::FixedSize:
                it->second.width = it->second.height = constraint.value;
                break;
            case ConstraintType::HorizontalSpacing: {
                if (!constraint.related_views.empty()) {
                    auto it2 = layouts.find(constraint.related_views[0]);
                    if (it2 != layouts.end()) {
                        it->second.position[0] = it2->second.position[0] + it2->second.width +
                                                 constraint.value;
                    }
                }
                break;
            }
            case ConstraintType::VerticalSpacing: {
                if (!constraint.related_views.empty()) {
                    auto it2 = layouts.find(constraint.related_views[0]);
                    if (it2 != layouts.end()) {
                        it->second.position[1] = it2->second.position[1] + it2->second.height +
                                                 constraint.value;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    // Convert to solution
    for (const auto& [_, layout] : layouts) {
        solution.view_layouts.push_back(layout);
    }

    solution.is_valid = validateLayout(solution);
    return solution;
}

LayoutSolution LayoutManager::arrangeViewsManually(
    const std::vector<std::shared_ptr<DrawingView>>& views,
    const std::map<std::string, Vec2>& positions) {

    LayoutSolution solution{{}, true, ""};

    for (const auto& view : views) {
        if (!view) continue;

        auto it = positions.find(view->getId());
        if (it == positions.end()) continue;

        auto bbox = view->calculateBoundingBox();
        solution.view_layouts.push_back(ViewLayout{
            view->getId(),
            it->second,
            bbox.width() * view->getScale(),
            bbox.height() * view->getScale(),
            view->getScale()
        });
    }

    solution.is_valid = validateLayout(solution);
    return solution;
}

LayoutSolution LayoutManager::arrangeViewsInGrid(
    const std::vector<std::shared_ptr<DrawingView>>& views,
    int columns,
    int rows) {

    if (columns <= 0 || rows <= 0) {
        return LayoutSolution{{}, false, "Invalid grid dimensions"};
    }

    LayoutSolution solution{{}, true, ""};
    int view_index = 0;

    double cell_width = (getWorkspaceWidth()) / columns;
    double cell_height = (getWorkspaceHeight()) / rows;

    for (int row = 0; row < rows && view_index < views.size(); row++) {
        for (int col = 0; col < columns && view_index < views.size(); col++) {
            auto view = views[view_index++];
            if (!view) continue;

            auto bbox = view->calculateBoundingBox();

            // Calculate scale to fit in cell
            double scale = std::min(
                (cell_width - 2 * config_.margin_left) / (bbox.width() + 0.1),
                (cell_height - 2 * config_.margin_top) / (bbox.height() + 0.1)
            );
            scale = std::min(scale, config_.max_view_size / std::max(bbox.width(), bbox.height()));

            Vec2 position{
                config_.margin_left + col * cell_width,
                config_.margin_top + row * cell_height
            };

            solution.view_layouts.push_back(ViewLayout{
                view->getId(),
                position,
                bbox.width() * scale,
                bbox.height() * scale,
                scale
            });
        }
    }

    solution.is_valid = validateLayout(solution);
    return solution;
}

LayoutSolution LayoutManager::arrangeViewsInLine(
    const std::vector<std::shared_ptr<DrawingView>>& views,
    bool horizontal) {

    if (views.empty()) {
        return LayoutSolution{{}, false, "No views provided"};
    }

    LayoutSolution solution{{}, true, ""};

    double current_pos_x = config_.margin_left;
    double current_pos_y = config_.margin_top;
    double max_height = 0;

    for (const auto& view : views) {
        if (!view) continue;

        auto bbox = view->calculateBoundingBox();
        double scale = calculateOptimalViewScale(*view);

        Vec2 position{current_pos_x, current_pos_y};

        if (horizontal) {
            current_pos_x += bbox.width() * scale + config_.horizontal_spacing;
            max_height = std::max(max_height, bbox.height() * scale);
        } else {
            current_pos_y += bbox.height() * scale + config_.vertical_spacing;
        }

        solution.view_layouts.push_back(ViewLayout{
            view->getId(),
            position,
            bbox.width() * scale,
            bbox.height() * scale,
            scale
        });
    }

    // Only validate if we have layouts
    if (!solution.view_layouts.empty()) {
        solution.is_valid = validateLayout(solution);
    }
    return solution;
}

void LayoutManager::alignViews(const std::vector<std::string>& view_ids,
                               ConstraintType alignment) {
    switch (alignment) {
        case ConstraintType::AlignLeft:
        case ConstraintType::AlignRight:
        case ConstraintType::AlignTop:
        case ConstraintType::AlignBottom:
        case ConstraintType::AlignCenterX:
        case ConstraintType::AlignCenterY:
            for (const auto& view_id : view_ids) {
                addConstraint(LayoutConstraint{view_id, alignment, 0.0, {}, 100});
            }
            break;
        default:
            break;
    }
}

void LayoutManager::distributeViews(const std::vector<std::string>& view_ids,
                                     bool horizontally) {
    if (view_ids.size() < 3) return;

    LayoutConstraint constraint{
        view_ids[0],
        horizontally ? ConstraintType::Distribute : ConstraintType::Distribute,
        0.0,
        view_ids,
        100
    };
    addConstraint(constraint);
}

void LayoutManager::spaceViewsEvenly(const std::vector<std::string>& view_ids,
                                      bool horizontally) {
    if (view_ids.size() < 2) return;

    for (size_t i = 1; i < view_ids.size(); i++) {
        double spacing = horizontally ? config_.horizontal_spacing : config_.vertical_spacing;
        addConstraint(LayoutConstraint{
            view_ids[i],
            horizontally ? ConstraintType::HorizontalSpacing : ConstraintType::VerticalSpacing,
            spacing,
            {view_ids[i - 1]},
            100
        });
    }
}

double LayoutManager::calculateOptimalViewScale(const DrawingView& view) const {
    auto bbox = view.calculateBoundingBox();
    if (bbox.width() == 0.0 || bbox.height() == 0.0) {
        return 1.0;
    }

    double available_width = getWorkspaceWidth();
    double available_height = getWorkspaceHeight();

    double scale_x = available_width / (bbox.width() + 0.1);
    double scale_y = available_height / (bbox.height() + 0.1);

    double scale = std::min(scale_x, scale_y);
    scale = std::clamp(scale, 0.1, 10.0);

    return scale;
}

void LayoutManager::resizeViewToFit(DrawingView& view, double available_width,
                                    double available_height) {
    auto bbox = view.calculateBoundingBox();
    if (bbox.width() == 0.0 || bbox.height() == 0.0) return;

    double scale_x = available_width / bbox.width();
    double scale_y = available_height / bbox.height();
    double scale = std::min(scale_x, scale_y);

    view.setScale(scale);
}

void LayoutManager::uniformScaleViews(std::vector<std::shared_ptr<DrawingView>>& views) {
    if (views.empty()) return;

    // Find the view that would have the smallest scale
    double min_scale = 10.0;
    for (const auto& view : views) {
        if (!view) continue;
        double scale = calculateOptimalViewScale(*view);
        min_scale = std::min(min_scale, scale);
    }

    // Apply the minimum scale to all views
    for (auto& view : views) {
        if (view) {
            view->setScale(min_scale);
        }
    }
}

Vec2 LayoutManager::calculateNextPosition(size_t view_index, size_t total_views) const {
    double x = config_.margin_left;
    double y = config_.margin_top;

    // Simple linear layout
    if (total_views > 0) {
        double row = view_index / 3;  // 3 views per row by default
        double col = view_index % 3;

        x += col * (getWorkspaceWidth() / 3);
        y += row * (getWorkspaceHeight() / 2);
    }

    return Vec2{x, y};
}

Vec2 LayoutManager::getAvailableSpace(const DrawingView& view) const {
    double available_width = getWorkspaceWidth();
    double available_height = getWorkspaceHeight();
    return Vec2{available_width, available_height};
}

bool LayoutManager::checkOverlap(const ViewLayout& layout1, const ViewLayout& layout2) const {
    // Check if two rectangles overlap
    double l1_right = layout1.position[0] + layout1.width;
    double l1_bottom = layout1.position[1] + layout1.height;
    double l2_right = layout2.position[0] + layout2.width;
    double l2_bottom = layout2.position[1] + layout2.height;

    return !(l1_right < layout2.position[0] ||
             l2_right < layout1.position[0] ||
             l1_bottom < layout2.position[1] ||
             l2_bottom < layout1.position[1]);
}

bool LayoutManager::checkFitsOnSheet(const ViewLayout& layout) const {
    return layout.position[0] >= 0 &&
           layout.position[1] >= 0 &&
           layout.position[0] + layout.width <= sheet_props_.width &&
           layout.position[1] + layout.height <= sheet_props_.height;
}

std::vector<std::pair<size_t, size_t>> LayoutManager::findOverlappingViews(
    const std::vector<ViewLayout>& layouts) const {

    std::vector<std::pair<size_t, size_t>> overlaps;

    for (size_t i = 0; i < layouts.size(); i++) {
        for (size_t j = i + 1; j < layouts.size(); j++) {
            if (checkOverlap(layouts[i], layouts[j])) {
                overlaps.push_back({i, j});
            }
        }
    }

    return overlaps;
}

bool LayoutManager::validateLayout(const LayoutSolution& solution) const {
    if (solution.view_layouts.empty()) {
        return false;
    }

    // Check for overlaps if not allowed
    if (!config_.allow_overlap) {
        auto overlaps = findOverlappingViews(solution.view_layouts);
        if (!overlaps.empty()) {
            return false;
        }
    }

    // Check if all views fit on sheet
    for (const auto& layout : solution.view_layouts) {
        if (!checkFitsOnSheet(layout)) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> LayoutManager::validateAllConstraints(
    const std::vector<std::shared_ptr<DrawingView>>& views) const {
    std::vector<std::string> errors;

    for (const auto& constraint : constraints_) {
        // Check if all related views exist
        bool found = false;
        for (const auto& view : views) {
            if (view && view->getId() == constraint.view_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            errors.push_back("View " + constraint.view_id + " in constraint not found");
        }
    }

    return errors;
}

std::vector<LayoutManager::AvailableRectangle> LayoutManager::findAvailableRectangles(
    const std::vector<ViewLayout>& existing_layouts) const {
    std::vector<AvailableRectangle> available;

    // Simple approach: find space in corners and edges
    double right_edge = 0;
    double bottom_edge = 0;

    for (const auto& layout : existing_layouts) {
        right_edge = std::max(right_edge, layout.position[0] + layout.width);
        bottom_edge = std::max(bottom_edge, layout.position[1] + layout.height);
    }

    // Right space
    if (right_edge < sheet_props_.width) {
        available.push_back(AvailableRectangle{
            {right_edge + config_.horizontal_spacing, config_.margin_top},
            sheet_props_.width - right_edge - config_.margin_right,
            sheet_props_.height - config_.margin_top - config_.margin_bottom
        });
    }

    // Bottom space
    if (bottom_edge < sheet_props_.height) {
        available.push_back(AvailableRectangle{
            {config_.margin_left, bottom_edge + config_.vertical_spacing},
            sheet_props_.width - config_.margin_left - config_.margin_right,
            sheet_props_.height - bottom_edge - config_.margin_bottom
        });
    }

    return available;
}

void LayoutManager::applyLayout(
    std::vector<std::shared_ptr<DrawingView>>& views,
    const LayoutSolution& solution) {

    std::map<std::string, const ViewLayout*> layout_map;
    for (const auto& layout : solution.view_layouts) {
        layout_map[layout.view_id] = &layout;
    }

    for (auto& view : views) {
        if (!view) continue;

        auto it = layout_map.find(view->getId());
        if (it != layout_map.end()) {
            view->setOrigin(it->second->position);
            view->setScale(it->second->scale);
        }
    }
}

double LayoutManager::getWorkspaceWidth() const {
    return sheet_props_.width - config_.margin_left - config_.margin_right;
}

double LayoutManager::getWorkspaceHeight() const {
    return sheet_props_.height - config_.margin_top - config_.margin_bottom;
}

bool LayoutManager::isViewPositionValid(const ViewLayout& layout) const {
    return layout.position[0] >= config_.margin_left &&
           layout.position[1] >= config_.margin_top &&
           layout.position[0] + layout.width <= sheet_props_.width - config_.margin_right &&
           layout.position[1] + layout.height <= sheet_props_.height - config_.margin_bottom;
}

std::vector<ViewLayout> LayoutManager::sortByPosition(const std::vector<ViewLayout>& layouts) const {
    auto sorted = layouts;
    std::sort(sorted.begin(), sorted.end(), [](const ViewLayout& a, const ViewLayout& b) {
        if (a.position[1] != b.position[1]) {
            return a.position[1] < b.position[1];
        }
        return a.position[0] < b.position[0];
    });
    return sorted;
}

// ============================================================================
// Layout Template Implementations
// ============================================================================

LayoutSolution LayoutManager::layoutSingleView(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    if (views.empty()) {
        return LayoutSolution{{}, false, "No views provided"};
    }

    LayoutSolution solution{{}, true, ""};
    auto view = views[0];
    if (!view) {
        return LayoutSolution{{}, false, "Invalid view"};
    }

    auto bbox = view->calculateBoundingBox();
    double scale = calculateOptimalViewScale(*view);

    // Center the view
    double x = (sheet_props_.width - bbox.width() * scale) / 2;
    double y = (sheet_props_.height - bbox.height() * scale) / 2;

    solution.view_layouts.push_back(ViewLayout{
        view->getId(),
        {x, y},
        bbox.width() * scale,
        bbox.height() * scale,
        scale
    });

    return solution;
}

LayoutSolution LayoutManager::layoutTwoView(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    if (views.size() < 2) {
        return layoutSingleView(views);
    }

    LayoutSolution solution{{}, true, ""};

    double half_width = (getWorkspaceWidth() - config_.horizontal_spacing) / 2;
    int view_idx = 0;

    for (int i = 0; i < 2 && view_idx < views.size(); i++) {
        auto view = views[view_idx++];
        if (!view) continue;

        auto bbox = view->calculateBoundingBox();
        double scale = std::min(
            half_width / (bbox.width() + 0.1),
            getWorkspaceHeight() / (bbox.height() + 0.1)
        );

        Vec2 position{
            config_.margin_left + i * (half_width + config_.horizontal_spacing),
            config_.margin_top
        };

        solution.view_layouts.push_back(ViewLayout{
            view->getId(),
            position,
            bbox.width() * scale,
            bbox.height() * scale,
            scale
        });
    }

    solution.is_valid = validateLayout(solution);
    return solution;
}

LayoutSolution LayoutManager::layoutThreeView(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    LayoutSolution solution{{}, true, ""};

    double third_width = (getWorkspaceWidth() - 2 * config_.horizontal_spacing) / 3;
    double third_height = (getWorkspaceHeight() - config_.vertical_spacing) / 2;

    int view_idx = 0;
    for (int row = 0; row < 2 && view_idx < views.size(); row++) {
        for (int col = 0; col < 3 && view_idx < views.size(); col++) {
            if (row == 1 && col >= 1) break;  // Only one view in second row

            auto view = views[view_idx++];
            if (!view) continue;

            auto bbox = view->calculateBoundingBox();
            double scale = std::min(
                third_width / (bbox.width() + 0.1),
                third_height / (bbox.height() + 0.1)
            );

            Vec2 position{
                config_.margin_left + col * (third_width + config_.horizontal_spacing),
                config_.margin_top + row * (third_height + config_.vertical_spacing)
            };

            solution.view_layouts.push_back(ViewLayout{
                view->getId(),
                position,
                bbox.width() * scale,
                bbox.height() * scale,
                scale
            });
        }
    }

    solution.is_valid = validateLayout(solution);
    return solution;
}

LayoutSolution LayoutManager::layoutSixView(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    return arrangeViewsInGrid(views, 3, 2);
}

LayoutSolution LayoutManager::layoutIsometric(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    if (views.empty()) {
        return LayoutSolution{{}, false, "No views provided"};
    }

    LayoutSolution solution{{}, true, ""};
    auto view = views[0];
    if (!view) {
        return LayoutSolution{{}, false, "Invalid view"};
    }

    auto bbox = view->calculateBoundingBox();
    double scale = calculateOptimalViewScale(*view);

    double x = (sheet_props_.width - bbox.width() * scale) / 2;
    double y = (sheet_props_.height - bbox.height() * scale) / 2;

    solution.view_layouts.push_back(ViewLayout{
        view->getId(),
        {x, y},
        bbox.width() * scale,
        bbox.height() * scale,
        scale
    });

    return solution;
}

LayoutSolution LayoutManager::layoutSectionDetail(
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    // Similar to 3-view, but optimized for section + detail
    return layoutThreeView(views);
}

LayoutConfig LayoutManager::getPresetConfig(LayoutTemplate template_type) {
    LayoutConfig config;
    config.template_type = template_type;

    switch (template_type) {
        case LayoutTemplate::SingleView:
            config.center_views = true;
            config.auto_scale = true;
            break;

        case LayoutTemplate::TwoView:
            config.horizontal_spacing = 15.0;
            config.auto_scale = true;
            break;

        case LayoutTemplate::ThreeView:
            config.horizontal_spacing = 10.0;
            config.vertical_spacing = 10.0;
            config.auto_scale = true;
            break;

        case LayoutTemplate::SixView:
            config.horizontal_spacing = 5.0;
            config.vertical_spacing = 5.0;
            config.auto_scale = true;
            break;

        case LayoutTemplate::Isometric:
            config.center_views = true;
            config.auto_scale = true;
            break;

        case LayoutTemplate::CustomGrid:
            config.align_to_grid = true;
            config.grid_size = 5.0;
            break;

        case LayoutTemplate::FreeLayout:
            config.allow_overlap = false;
            config.snap_to_edges = false;
            break;

        default:
            break;
    }

    return config;
}

// ============================================================================
// LayoutHistory Implementation
// ============================================================================

void LayoutHistory::recordLayout(const std::string& layout_name, const LayoutSolution& solution) {
    layouts_[layout_name] = solution;
}

const LayoutSolution& LayoutHistory::getLayout(const std::string& layout_name) const {
    auto it = layouts_.find(layout_name);
    if (it != layouts_.end()) {
        return it->second;
    }
    throw std::invalid_argument("Layout not found: " + layout_name);
}

std::vector<std::string> LayoutHistory::getLayoutNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : layouts_) {
        names.push_back(name);
    }
    return names;
}

void LayoutHistory::clearHistory() {
    layouts_.clear();
}

} // namespace BendCore::Drawing
