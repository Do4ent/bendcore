#include "bendcore/drawing/drawing.hpp"
#include <chrono>
#include <stdexcept>

namespace BendCore::CadCore {

// ============ SheetProperties ============

SheetProperties::SheetProperties()
    : size(SheetSize::ISO_A4)
    , units(DrawingUnits::MILLIMETERS)
    , width_mm(210.0)
    , height_mm(297.0)
    , left_margin(20.0)
    , right_margin(10.0)
    , top_margin(10.0)
    , bottom_margin(10.0)
    , sheet_number(1)
    , total_sheets(1)
    , has_border(true)
    , has_title_block(true)
{}

SheetProperties::SheetProperties(SheetSize sz)
    : size(sz)
    , units(DrawingUnits::MILLIMETERS)
    , sheet_number(1)
    , total_sheets(1)
    , has_border(true)
    , has_title_block(true)
{
    // Set dimensions based on size
    switch (sz) {
        case SheetSize::ISO_A0:
            width_mm = 841.0; height_mm = 1189.0; break;
        case SheetSize::ISO_A1:
            width_mm = 594.0; height_mm = 841.0; break;
        case SheetSize::ISO_A2:
            width_mm = 420.0; height_mm = 594.0; break;
        case SheetSize::ISO_A3:
            width_mm = 297.0; height_mm = 420.0; break;
        case SheetSize::ISO_A4:
            width_mm = 210.0; height_mm = 297.0; break;
        case SheetSize::ISO_A5:
            width_mm = 148.0; height_mm = 210.0; break;
        case SheetSize::ANSI_A:
            width_mm = 216.0; height_mm = 279.0; break;
        case SheetSize::ANSI_B:
            width_mm = 279.0; height_mm = 432.0; break;
        case SheetSize::ANSI_C:
            width_mm = 432.0; height_mm = 559.0; break;
        case SheetSize::ANSI_D:
            width_mm = 559.0; height_mm = 864.0; break;
        case SheetSize::ANSI_E:
            width_mm = 864.0; height_mm = 1118.0; break;
        case SheetSize::CUSTOM:
            width_mm = 0.0; height_mm = 0.0; break;
    }

    left_margin = 20.0;
    right_margin = 10.0;
    top_margin = 10.0;
    bottom_margin = 10.0;
}

// ============ ViewProperties ============

ViewProperties::ViewProperties()
    : position_x(0.0)
    , position_y(0.0)
    , width(100.0)
    , height(100.0)
    , scale(1.0)
    , rotation(0.0)
    , is_visible(true)
    , layer_mask(0xFFFFFFFF)
{}

// ============ DrawingSheet ============

DrawingSheet::DrawingSheet(const EntityId& id, const std::string& name)
    : id_(id)
    , name_(name)
    , properties_(SheetSize::ISO_A4)
    , revision_(0)
    , created_time_(std::chrono::system_clock::now())
    , modified_time_(std::chrono::system_clock::now())
{}

uint64_t DrawingSheet::incrementRevision() noexcept {
    modified_time_ = std::chrono::system_clock::now();
    return ++revision_;
}

void DrawingSheet::addView(const EntityId& view_id, const ViewProperties& props) {
    if (views_.count(view_id)) {
        throw std::invalid_argument("View already exists on sheet");
    }
    views_[view_id] = props;
    incrementRevision();
}

void DrawingSheet::removeView(const EntityId& view_id) {
    if (!views_.count(view_id)) {
        throw std::invalid_argument("View not found on sheet");
    }
    views_.erase(view_id);
    incrementRevision();
}

std::optional<ViewProperties> DrawingSheet::getViewProperties(const EntityId& view_id) const noexcept {
    auto it = views_.find(view_id);
    if (it != views_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DrawingSheet::updateViewProperties(const EntityId& view_id, const ViewProperties& props) {
    auto it = views_.find(view_id);
    if (it == views_.end()) {
        throw std::invalid_argument("View not found on sheet");
    }
    it->second = props;
    incrementRevision();
}

std::vector<EntityId> DrawingSheet::getViewIds() const {
    std::vector<EntityId> ids;
    for (const auto& [view_id, _] : views_) {
        ids.push_back(view_id);
    }
    return ids;
}

std::optional<std::string> DrawingSheet::getMetadata(const std::string& key) const noexcept {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DrawingSheet::setMetadata(const std::string& key, const std::string& value) noexcept {
    metadata_[key] = value;
    incrementRevision();
}

// ============ DrawingView ============

DrawingView::DrawingView(const EntityId& id, const EntityId& source_geometry_id, const std::string& name)
    : id_(id)
    , source_geometry_id_(source_geometry_id)
    , name_(name)
    , revision_(0)
    , has_clipping_(false)
    , clip_x_(0.0)
    , clip_y_(0.0)
    , clip_width_(0.0)
    , clip_height_(0.0)
    , is_dirty_(true)
{}

uint64_t DrawingView::incrementRevision() noexcept {
    return ++revision_;
}

void DrawingView::setClippingRegion(double x, double y, double width, double height) noexcept {
    clip_x_ = x;
    clip_y_ = y;
    clip_width_ = width;
    clip_height_ = height;
    has_clipping_ = true;
    incrementRevision();
}

std::optional<std::string> DrawingView::getMetadata(const std::string& key) const noexcept {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DrawingView::setMetadata(const std::string& key, const std::string& value) noexcept {
    metadata_[key] = value;
    incrementRevision();
}

// ============ Drawing ============

Drawing::Drawing(const EntityId& id, const std::string& name)
    : id_(id)
    , name_(name)
    , title_("")
    , scale_(1.0)
    , units_(DrawingUnits::MILLIMETERS)
    , projection_(ProjectionMethod::THIRD_ANGLE)
    , revision_(0)
    , created_time_(std::chrono::system_clock::now())
    , modified_time_(std::chrono::system_clock::now())
{}

uint64_t Drawing::incrementRevision() noexcept {
    modified_time_ = std::chrono::system_clock::now();
    return ++revision_;
}

std::shared_ptr<DrawingSheet> Drawing::addSheet(const std::string& name) {
    auto sheet_id = EntityId::generate();
    auto sheet = std::make_shared<DrawingSheet>(sheet_id, name.empty() ? "Sheet" + std::to_string(sheets_.size() + 1) : name);
    sheets_[sheet_id] = sheet;
    incrementRevision();
    return sheet;
}

void Drawing::removeSheet(const EntityId& sheet_id) {
    if (!sheets_.count(sheet_id)) {
        throw std::invalid_argument("Sheet not found in drawing");
    }
    sheets_.erase(sheet_id);
    incrementRevision();
}

std::shared_ptr<DrawingSheet> Drawing::getSheet(const EntityId& sheet_id) {
    auto it = sheets_.find(sheet_id);
    if (it == sheets_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<const DrawingSheet> Drawing::getSheet(const EntityId& sheet_id) const {
    auto it = sheets_.find(sheet_id);
    if (it == sheets_.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<EntityId> Drawing::getSheetIds() const {
    std::vector<EntityId> ids;
    for (const auto& [sheet_id, _] : sheets_) {
        ids.push_back(sheet_id);
    }
    return ids;
}

std::shared_ptr<DrawingView> Drawing::addView(const EntityId& source_geometry_id, const std::string& name) {
    auto view_id = EntityId::generate();
    auto view = std::make_shared<DrawingView>(view_id, source_geometry_id, name.empty() ? "View" + std::to_string(views_.size() + 1) : name);
    views_[view_id] = view;
    incrementRevision();
    return view;
}

void Drawing::removeView(const EntityId& view_id) {
    if (!views_.count(view_id)) {
        throw std::invalid_argument("View not found in drawing");
    }
    views_.erase(view_id);
    incrementRevision();
}

std::shared_ptr<DrawingView> Drawing::getView(const EntityId& view_id) {
    auto it = views_.find(view_id);
    if (it == views_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<const DrawingView> Drawing::getView(const EntityId& view_id) const {
    auto it = views_.find(view_id);
    if (it == views_.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<EntityId> Drawing::getViewIds() const {
    std::vector<EntityId> ids;
    for (const auto& [view_id, _] : views_) {
        ids.push_back(view_id);
    }
    return ids;
}

std::optional<std::string> Drawing::getMetadata(const std::string& key) const noexcept {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Drawing::setMetadata(const std::string& key, const std::string& value) noexcept {
    metadata_[key] = value;
    incrementRevision();
}

std::string Drawing::validate() const noexcept {
    // Check if source ID is valid
    if (source_id_.isNull()) {
        return "Invalid source geometry ID";
    }

    // Check if there's at least one sheet
    if (sheets_.empty()) {
        return "Drawing has no sheets";
    }

    // Check sheet validity
    for (const auto& [sheet_id, sheet] : sheets_) {
        if (!sheet) {
            return "Invalid sheet in drawing";
        }

        // Check view validity
        for (const auto& view_id : sheet->getViewIds()) {
            if (!views_.count(view_id)) {
                return "Sheet contains invalid view reference";
            }
        }
    }

    return "";  // Empty string means valid
}

void Drawing::markAllViewsDirty() noexcept {
    for (auto& [_, view] : views_) {
        if (view) {
            view->markDirty();
        }
    }
}

} // namespace BendCore::CadCore
