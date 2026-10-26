// M4 Phase - View Properties Implementation
#include "bendcore/drawing/view_properties.hpp"

#include <algorithm>
#include <stdexcept>

namespace BendCore::Drawing {

// ============================================================================
// ViewLayerManager Implementation
// ============================================================================

ViewLayerManager::ViewLayerManager() = default;

void ViewLayerManager::setLayerVisible(const std::string& layer_name, bool visible) {
    if (layers_.find(layer_name) != layers_.end()) {
        layers_[layer_name].visible = visible;
    }
}

bool ViewLayerManager::isLayerVisible(const std::string& layer_name) const {
    auto it = layers_.find(layer_name);
    if (it != layers_.end()) {
        return it->second.visible;
    }
    return true;  // Default: visible
}

void ViewLayerManager::setLayerPrintable(const std::string& layer_name, bool printable) {
    if (layers_.find(layer_name) != layers_.end()) {
        layers_[layer_name].printable = printable;
    }
}

bool ViewLayerManager::isLayerPrintable(const std::string& layer_name) const {
    auto it = layers_.find(layer_name);
    if (it != layers_.end()) {
        return it->second.printable;
    }
    return true;  // Default: printable
}

void ViewLayerManager::setLayerLocked(const std::string& layer_name, bool locked) {
    if (layers_.find(layer_name) != layers_.end()) {
        layers_[layer_name].locked = locked;
    }
}

bool ViewLayerManager::isLayerLocked(const std::string& layer_name) const {
    auto it = layers_.find(layer_name);
    if (it != layers_.end()) {
        return it->second.locked;
    }
    return false;  // Default: not locked
}

void ViewLayerManager::setLayerOpacity(const std::string& layer_name, double opacity) {
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (layers_.find(layer_name) != layers_.end()) {
        layers_[layer_name].opacity = opacity;
    }
}

double ViewLayerManager::getLayerOpacity(const std::string& layer_name) const {
    auto it = layers_.find(layer_name);
    if (it != layers_.end()) {
        return it->second.opacity;
    }
    return 1.0;  // Default: fully opaque
}

void ViewLayerManager::addLayer(const std::string& layer_name) {
    if (layers_.find(layer_name) == layers_.end()) {
        layers_[layer_name] = LayerVisibility{layer_name, true, true, false, 1.0};
    }
}

void ViewLayerManager::removeLayer(const std::string& layer_name) {
    layers_.erase(layer_name);
}

const std::map<std::string, LayerVisibility>& ViewLayerManager::getLayerMap() const {
    return layers_;
}

void ViewLayerManager::showAllLayers() {
    for (auto& [_, layer] : layers_) {
        layer.visible = true;
    }
}

void ViewLayerManager::hideAllLayers() {
    for (auto& [_, layer] : layers_) {
        layer.visible = false;
    }
}

void ViewLayerManager::showOnlyLayer(const std::string& layer_name) {
    for (auto& [name, layer] : layers_) {
        layer.visible = (name == layer_name);
    }
}

void ViewLayerManager::clear() {
    layers_.clear();
}

// ============================================================================
// ViewAppearance Implementation
// ============================================================================

ViewAppearance::ViewAppearance() {
    // Initialize default line styles
    line_styles_[LineStyle::Contour] = LineProperties{
        LineStyle::Contour, 0.7, "#000000", 0.0, 0.0
    };
    line_styles_[LineStyle::Hidden] = LineProperties{
        LineStyle::Hidden, 0.5, "#000000", 2.0, 1.0
    };
    line_styles_[LineStyle::Thin] = LineProperties{
        LineStyle::Thin, 0.35, "#000000", 0.0, 0.0
    };
    line_styles_[LineStyle::Center] = LineProperties{
        LineStyle::Center, 0.4, "#000000", 3.0, 1.5
    };
    line_styles_[LineStyle::Dimension] = LineProperties{
        LineStyle::Dimension, 0.35, "#000000", 0.0, 0.0
    };
    line_styles_[LineStyle::Cut] = LineProperties{
        LineStyle::Cut, 0.6, "#000000", 1.5, 1.0
    };
    line_styles_[LineStyle::BreakLine] = LineProperties{
        LineStyle::BreakLine, 0.5, "#000000", 2.0, 1.0
    };

    // Default text properties
    text_props_ = TextProperties{
        "Arial", 3.5, "#000000", "center", 1.2
    };

    // Default hatch properties
    hatch_props_ = HatchProperties{
        HatchingPattern::CrossHatch, 45.0, 2.0, "#000000", "#FFFFFF", 1.0
    };

    // Default frame properties
    frame_props_ = ViewFrameProperties{
        BorderStyle::Simple, 1.0, "#000000", 0.0, true, true, {0, -5}
    };
}

const LineProperties& ViewAppearance::getLineProperties(LineStyle style) const {
    auto it = line_styles_.find(style);
    if (it != line_styles_.end()) {
        return it->second;
    }
    // Return default contour style if not found
    return line_styles_.at(LineStyle::Contour);
}

void ViewAppearance::setLineProperties(LineStyle style, const LineProperties& props) {
    line_styles_[style] = props;
}

} // namespace BendCore::Drawing
