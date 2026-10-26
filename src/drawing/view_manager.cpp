// M4 Phase - View Manager Implementation
#include "bendcore/drawing/view_manager.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace BendCore::Drawing {

// ============================================================================
// ViewMetadata Implementation
// ============================================================================

ViewMetadata::ViewMetadata(const std::string& view_id)
    : view_id_(view_id), view_name_(view_id) {
    updateModificationTime();
}

void ViewMetadata::updateModificationTime() {
    last_modified_ = std::chrono::system_clock::now().time_since_epoch().count();
}

// ============================================================================
// ViewManager Implementation
// ============================================================================

ViewManager::ViewManager() = default;

ViewManager::~ViewManager() = default;

void ViewManager::registerView(std::shared_ptr<DrawingView> view) {
    if (!view) {
        throw std::invalid_argument("Cannot register null view");
    }

    const std::string& view_id = view->getId();

    if (views_.find(view_id) != views_.end()) {
        throw std::invalid_argument("View with ID " + view_id + " already registered");
    }

    views_[view_id] = view;
    view_order_.push_back(view_id);
    ensureMetadataExists(view_id);
    ensureAppearanceExists(view_id);
}

void ViewManager::unregisterView(const std::string& view_id) {
    auto it = views_.find(view_id);
    if (it != views_.end()) {
        views_.erase(it);
    }

    metadata_.erase(view_id);
    appearances_.erase(view_id);
    change_callbacks_.erase(view_id);

    auto view_it = std::find(view_order_.begin(), view_order_.end(), view_id);
    if (view_it != view_order_.end()) {
        view_order_.erase(view_it);
    }

    // Remove dependencies
    for (auto& [_, metadata] : metadata_) {
        metadata.removeDependentView(view_id);
    }
}

std::shared_ptr<DrawingView> ViewManager::getView(const std::string& view_id) {
    auto it = views_.find(view_id);
    if (it != views_.end()) {
        return it->second;
    }
    return nullptr;
}

const std::shared_ptr<DrawingView> ViewManager::getView(const std::string& view_id) const {
    auto it = views_.find(view_id);
    if (it != views_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<DrawingView>> ViewManager::getAllViews() {
    std::vector<std::shared_ptr<DrawingView>> result;
    for (const auto& view_id : view_order_) {
        auto it = views_.find(view_id);
        if (it != views_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

const std::vector<std::shared_ptr<DrawingView>>& ViewManager::getAllViews() const {
    // Note: This const version cannot maintain view_order properly
    // Return through a static temporary (not ideal for thread-safety)
    static std::vector<std::shared_ptr<DrawingView>> temp;
    temp.clear();
    for (const auto& view_id : view_order_) {
        auto it = views_.find(view_id);
        if (it != views_.end()) {
            temp.push_back(it->second);
        }
    }
    return temp;
}

bool ViewManager::hasView(const std::string& view_id) const {
    return views_.find(view_id) != views_.end();
}

ViewMetadata& ViewManager::getMetadata(const std::string& view_id) {
    if (!hasView(view_id)) {
        throw std::invalid_argument("View with ID " + view_id + " not found");
    }
    ensureMetadataExists(view_id);
    return metadata_[view_id];
}

const ViewMetadata& ViewManager::getMetadata(const std::string& view_id) const {
    auto it = metadata_.find(view_id);
    if (it != metadata_.end()) {
        return it->second;
    }
    throw std::invalid_argument("Metadata for view " + view_id + " not found");
}

ViewAppearance& ViewManager::getAppearance(const std::string& view_id) {
    if (!hasView(view_id)) {
        throw std::invalid_argument("View with ID " + view_id + " not found");
    }
    ensureAppearanceExists(view_id);
    return appearances_[view_id];
}

const ViewAppearance& ViewManager::getAppearance(const std::string& view_id) const {
    auto it = appearances_.find(view_id);
    if (it != appearances_.end()) {
        return it->second;
    }
    throw std::invalid_argument("Appearance for view " + view_id + " not found");
}

void ViewManager::setViewPosition(const std::string& view_id, const Vec2& position) {
    auto view = getView(view_id);
    if (!view) {
        throw std::invalid_argument("View not found: " + view_id);
    }

    view->setOrigin(position);
    getMetadata(view_id).updateModificationTime();

    if (!in_batch_update_) {
        ViewChangeEvent event{view_id, ViewChangeType::Position, 0};
        notifyChange(event);
    }
}

Vec2 ViewManager::getViewPosition(const std::string& view_id) const {
    auto view = getView(view_id);
    if (!view) {
        throw std::invalid_argument("View not found: " + view_id);
    }
    return view->getOrigin();
}

void ViewManager::setViewScale(const std::string& view_id, double scale) {
    if (scale <= 0.0) {
        throw std::invalid_argument("Scale must be positive");
    }

    auto view = getView(view_id);
    if (!view) {
        throw std::invalid_argument("View not found: " + view_id);
    }

    view->setScale(scale);
    getMetadata(view_id).updateModificationTime();

    if (!in_batch_update_) {
        ViewChangeEvent event{view_id, ViewChangeType::Scale, 0};
        notifyChange(event);
    }
}

double ViewManager::getViewScale(const std::string& view_id) const {
    auto view = getView(view_id);
    if (!view) {
        throw std::invalid_argument("View not found: " + view_id);
    }
    return view->getScale();
}

void ViewManager::setViewDependency(const std::string& parent_id, const std::string& child_id) {
    if (!hasView(parent_id)) {
        throw std::invalid_argument("Parent view not found: " + parent_id);
    }
    if (!hasView(child_id)) {
        throw std::invalid_argument("Child view not found: " + child_id);
    }

    auto& child_metadata = getMetadata(child_id);
    child_metadata.setParentViewId(parent_id);

    auto& parent_metadata = getMetadata(parent_id);
    parent_metadata.addDependentView(child_id);
}

void ViewManager::removeDependency(const std::string& parent_id, const std::string& child_id) {
    if (hasView(parent_id)) {
        getMetadata(parent_id).removeDependentView(child_id);
    }
    if (hasView(child_id)) {
        getMetadata(child_id).setParentViewId("");
    }
}

std::vector<std::string> ViewManager::getDependentViews(const std::string& view_id) const {
    if (!hasView(view_id)) {
        throw std::invalid_argument("View not found: " + view_id);
    }

    const auto& dependent_set = getMetadata(view_id).getDependentViews();
    return std::vector<std::string>(dependent_set.begin(), dependent_set.end());
}

std::string ViewManager::getParentView(const std::string& view_id) const {
    if (!hasView(view_id)) {
        throw std::invalid_argument("View not found: " + view_id);
    }
    return getMetadata(view_id).getParentViewId();
}

void ViewManager::createDetailView(const std::string& parent_id, const std::string& detail_id,
                                   const DetailViewInfo& info) {
    if (!hasView(parent_id)) {
        throw std::invalid_argument("Parent view not found: " + parent_id);
    }

    setViewDependency(parent_id, detail_id);
    getMetadata(detail_id).setDetailViewInfo(info);
}

void ViewManager::deleteDetailView(const std::string& detail_id) {
    if (!hasView(detail_id)) {
        return;
    }

    const auto& metadata = getMetadata(detail_id);
    if (metadata.getDetailViewInfo()) {
        std::string parent_id = metadata.getParentViewId();
        removeDependency(parent_id, detail_id);
    }
}

void ViewManager::createSectionView(const std::string& parent_id, const std::string& section_id,
                                    const SectionViewInfo& info) {
    if (!hasView(parent_id)) {
        throw std::invalid_argument("Parent view not found: " + parent_id);
    }

    setViewDependency(parent_id, section_id);
    getMetadata(section_id).setSectionViewInfo(info);
}

void ViewManager::deleteSectionView(const std::string& section_id) {
    if (!hasView(section_id)) {
        return;
    }

    const auto& metadata = getMetadata(section_id);
    if (metadata.getSectionViewInfo()) {
        std::string parent_id = metadata.getParentViewId();
        removeDependency(parent_id, section_id);
    }
}

void ViewManager::createExplodedView(const std::string& exploded_id, const ExplodedViewInfo& info) {
    getMetadata(exploded_id).setExplodedViewInfo(info);
}

void ViewManager::deleteExplodedView(const std::string& exploded_id) {
    if (hasView(exploded_id)) {
        // Just remove the metadata association
        getMetadata(exploded_id).setExplodedViewInfo({});
    }
}

void ViewManager::syncView(const std::string& view_id) {
    if (!hasView(view_id)) {
        return;
    }

    getMetadata(view_id).incrementRevision();
    ViewChangeEvent event{view_id, ViewChangeType::All, 0};
    notifyChange(event);
}

void ViewManager::syncAllViews() {
    beginBatchUpdate();
    for (const auto& [view_id, _] : views_) {
        getMetadata(view_id).incrementRevision();
        pending_changes_.push_back(ViewChangeEvent{view_id, ViewChangeType::All, 0});
    }
    endBatchUpdate();
}

void ViewManager::syncDependentViews(const std::string& parent_id) {
    auto dependent_ids = getDependentViews(parent_id);
    beginBatchUpdate();
    for (const auto& child_id : dependent_ids) {
        getMetadata(child_id).incrementRevision();
        pending_changes_.push_back(ViewChangeEvent{child_id, ViewChangeType::All, 0});
    }
    endBatchUpdate();
}

void ViewManager::beginBatchUpdate() {
    in_batch_update_ = true;
    pending_changes_.clear();
}

void ViewManager::endBatchUpdate() {
    in_batch_update_ = false;

    // Send all pending changes
    for (const auto& event : pending_changes_) {
        notifyChange(event);
    }
    pending_changes_.clear();
}

void ViewManager::subscribeToChanges(const std::string& view_id, ChangeCallback callback) {
    change_callbacks_[view_id] = callback;
}

void ViewManager::unsubscribeFromChanges(const std::string& view_id) {
    change_callbacks_.erase(view_id);
}

void ViewManager::notifyChange(const ViewChangeEvent& event) {
    auto it = change_callbacks_.find(event.view_id);
    if (it != change_callbacks_.end()) {
        it->second(event);
    }
}

void ViewManager::moveViewToFront(const std::string& view_id) {
    auto it = std::find(view_order_.begin(), view_order_.end(), view_id);
    if (it != view_order_.end()) {
        view_order_.erase(it);
        view_order_.push_back(view_id);
    }
}

void ViewManager::moveViewToBack(const std::string& view_id) {
    auto it = std::find(view_order_.begin(), view_order_.end(), view_id);
    if (it != view_order_.end()) {
        view_order_.erase(it);
        view_order_.insert(view_order_.begin(), view_id);
    }
}

void ViewManager::reorderViews(const std::vector<std::string>& view_ids) {
    view_order_ = view_ids;
}

void ViewManager::setViewVisibility(const std::string& view_id, bool visible) {
    if (!hasView(view_id)) {
        throw std::invalid_argument("View not found: " + view_id);
    }

    getMetadata(view_id).setVisible(visible);
    ViewChangeEvent event{view_id, ViewChangeType::Visibility, 0};
    notifyChange(event);
}

bool ViewManager::getViewVisibility(const std::string& view_id) const {
    if (!hasView(view_id)) {
        return false;
    }
    return getMetadata(view_id).isVisible();
}

void ViewManager::showAllViews() {
    beginBatchUpdate();
    for (auto& [view_id, _] : metadata_) {
        _.setVisible(true);
        pending_changes_.push_back(ViewChangeEvent{view_id, ViewChangeType::Visibility, 0});
    }
    endBatchUpdate();
}

void ViewManager::hideAllViews() {
    beginBatchUpdate();
    for (auto& [view_id, _] : metadata_) {
        _.setVisible(false);
        pending_changes_.push_back(ViewChangeEvent{view_id, ViewChangeType::Visibility, 0});
    }
    endBatchUpdate();
}

ViewManager::AlignmentInfo ViewManager::calculateViewAlignment(
    const std::vector<std::string>& view_ids) const {
    AlignmentInfo info{};

    if (view_ids.empty()) {
        return info;
    }

    bool first = true;

    for (const auto& view_id : view_ids) {
        auto view = getView(view_id);
        if (!view) continue;

        auto bbox = view->calculateBoundingBox();
        auto origin = view->getOrigin();
        double scale = view->getScale();

        double x = origin[0];
        double y = origin[1];
        double x2 = x + bbox.width() * scale;
        double y2 = y + bbox.height() * scale;

        if (first) {
            info.left = x;
            info.right = x2;
            info.top = y;
            info.bottom = y2;
            first = false;
        } else {
            info.left = std::min(info.left, x);
            info.right = std::max(info.right, x2);
            info.top = std::min(info.top, y);
            info.bottom = std::max(info.bottom, y2);
        }
    }

    info.centerX = (info.left + info.right) / 2.0;
    info.centerY = (info.top + info.bottom) / 2.0;

    return info;
}

bool ViewManager::isViewValid(const std::string& view_id) const {
    auto view = getView(view_id);
    if (!view) return false;

    // Check for basic validity
    if (view->getScale() <= 0.0) return false;

    return true;
}

std::vector<std::string> ViewManager::getInvalidViews() const {
    std::vector<std::string> invalid;

    for (const auto& [view_id, _] : views_) {
        if (!isViewValid(view_id)) {
            invalid.push_back(view_id);
        }
    }

    return invalid;
}

void ViewManager::ensureMetadataExists(const std::string& view_id) {
    if (metadata_.find(view_id) == metadata_.end()) {
        metadata_.emplace(view_id, ViewMetadata(view_id));
    }
}

void ViewManager::ensureAppearanceExists(const std::string& view_id) {
    if (appearances_.find(view_id) == appearances_.end()) {
        appearances_.emplace(view_id, ViewAppearance());
    }
}

} // namespace BendCore::Drawing
