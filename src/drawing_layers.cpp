// Реализация системы слоёв и аннотаций
#include "bendcore/drawing_layers.hpp"

#include <algorithm>
#include <sstream>
#include <random>

namespace bendcore::drawing {

// ==================== Layer ====================

bool DrawingLayerManager::Layer::operator==(const Layer& other) const {
    return id == other.id && name == other.name && visible == other.visible &&
           editable == other.editable && locked == other.locked &&
           element_ids == other.element_ids && parent_layer_id == other.parent_layer_id;
}

// ==================== Annotation ====================

bool DrawingLayerManager::Annotation::operator==(const Annotation& other) const {
    return id == other.id && type == other.type && text == other.text && x == other.x &&
           y == other.y && layer_id == other.layer_id && properties == other.properties;
}

// ==================== DrawingLayerManager ====================

DrawingLayerManager::DrawingLayerManager() {
    // Инициализировать стандартные слои
    Layer default_layer;
    default_layer.id = "layer_contours";
    default_layer.name = "Контуры";
    default_layer.visible = true;
    default_layer.editable = true;
    layers_["layer_contours"] = default_layer;

    Layer dims_layer;
    dims_layer.id = "layer_dimensions";
    dims_layer.name = "Размеры";
    dims_layer.visible = true;
    dims_layer.editable = true;
    layers_["layer_dimensions"] = dims_layer;

    Layer annotations_layer;
    annotations_layer.id = "layer_annotations";
    annotations_layer.name = "Аннотации";
    annotations_layer.visible = true;
    annotations_layer.editable = true;
    layers_["layer_annotations"] = annotations_layer;
}

// ===== Управление слоями =====

void DrawingLayerManager::add_layer(const Layer& layer) {
    if (!layer.id.empty()) {
        layers_[layer.id] = layer;
    }
}

void DrawingLayerManager::remove_layer(const std::string& layer_id) {
    layers_.erase(layer_id);

    // Удалить аннотации в этом слое
    std::vector<std::string> to_remove;
    for (const auto& [annot_id, annot] : annotations_) {
        if (annot.layer_id == layer_id) {
            to_remove.push_back(annot_id);
        }
    }
    for (const auto& id : to_remove) {
        annotations_.erase(id);
    }
}

std::optional<DrawingLayerManager::Layer> DrawingLayerManager::get_layer(
    const std::string& layer_id) const {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<DrawingLayerManager::Layer> DrawingLayerManager::get_all_layers() const {
    std::vector<Layer> result;
    for (const auto& [_, layer] : layers_) {
        result.push_back(layer);
    }
    return result;
}

void DrawingLayerManager::update_layer(const Layer& layer) {
    if (layers_.find(layer.id) != layers_.end()) {
        layers_[layer.id] = layer;
    }
}

void DrawingLayerManager::rename_layer(const std::string& layer_id, const std::string& new_name) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        it->second.name = new_name;
    }
}

void DrawingLayerManager::toggle_layer_visibility(const std::string& layer_id) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        it->second.visible = !it->second.visible;
    }
}

void DrawingLayerManager::set_layer_visibility(const std::string& layer_id, bool visible) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        it->second.visible = visible;
    }
}

void DrawingLayerManager::set_layer_locked(const std::string& layer_id, bool locked) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        it->second.locked = locked;
    }
}

void DrawingLayerManager::set_layer_editable(const std::string& layer_id, bool editable) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        it->second.editable = editable;
    }
}

// ===== Управление элементами в слоях =====

void DrawingLayerManager::add_element_to_layer(const std::string& layer_id,
                                               const std::string& element_id) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        auto& elem_ids = it->second.element_ids;
        if (std::find(elem_ids.begin(), elem_ids.end(), element_id) == elem_ids.end()) {
            elem_ids.push_back(element_id);
        }
    }
}

void DrawingLayerManager::remove_element_from_layer(const std::string& layer_id,
                                                    const std::string& element_id) {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        auto& elem_ids = it->second.element_ids;
        elem_ids.erase(std::remove(elem_ids.begin(), elem_ids.end(), element_id),
                      elem_ids.end());
    }
}

std::vector<std::string> DrawingLayerManager::get_elements_in_layer(
    const std::string& layer_id) const {
    auto it = layers_.find(layer_id);
    if (it != layers_.end()) {
        return it->second.element_ids;
    }
    return {};
}

std::optional<std::string> DrawingLayerManager::get_layer_for_element(
    const std::string& element_id) const {
    for (const auto& [layer_id, layer] : layers_) {
        if (std::find(layer.element_ids.begin(), layer.element_ids.end(), element_id) !=
            layer.element_ids.end()) {
            return layer_id;
        }
    }
    return std::nullopt;
}

// ===== Управление аннотациями =====

void DrawingLayerManager::add_annotation(const Annotation& annot) {
    if (!annot.id.empty()) {
        annotations_[annot.id] = annot;
    }
}

void DrawingLayerManager::remove_annotation(const std::string& annot_id) {
    annotations_.erase(annot_id);
}

std::optional<DrawingLayerManager::Annotation> DrawingLayerManager::get_annotation(
    const std::string& annot_id) const {
    auto it = annotations_.find(annot_id);
    if (it != annotations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<DrawingLayerManager::Annotation> DrawingLayerManager::get_annotations_for_layer(
    const std::string& layer_id) const {
    std::vector<Annotation> result;
    for (const auto& [_, annot] : annotations_) {
        if (annot.layer_id == layer_id) {
            result.push_back(annot);
        }
    }
    return result;
}

std::vector<DrawingLayerManager::Annotation> DrawingLayerManager::get_annotations_by_type(
    const std::string& type) const {
    std::vector<Annotation> result;
    for (const auto& [_, annot] : annotations_) {
        if (annot.type == type) {
            result.push_back(annot);
        }
    }
    return result;
}

void DrawingLayerManager::update_annotation(const Annotation& annot) {
    if (annotations_.find(annot.id) != annotations_.end()) {
        annotations_[annot.id] = annot;
    }
}

void DrawingLayerManager::move_annotation(const std::string& annot_id, double x, double y) {
    auto it = annotations_.find(annot_id);
    if (it != annotations_.end()) {
        it->second.x = x;
        it->second.y = y;
    }
}

// ===== Управление группами слоёв =====

void DrawingLayerManager::create_layer_group(const LayerGroup& group) {
    if (!group.id.empty()) {
        layer_groups_[group.id] = group;
    }
}

void DrawingLayerManager::remove_layer_group(const std::string& group_id) {
    layer_groups_.erase(group_id);
}

std::optional<DrawingLayerManager::LayerGroup> DrawingLayerManager::get_layer_group(
    const std::string& group_id) const {
    auto it = layer_groups_.find(group_id);
    if (it != layer_groups_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ===== Общие операции =====

void DrawingLayerManager::clear() {
    layers_.clear();
    annotations_.clear();
    layer_groups_.clear();
}

size_t DrawingLayerManager::layer_count() const {
    return layers_.size();
}

size_t DrawingLayerManager::annotation_count() const {
    return annotations_.size();
}

bool DrawingLayerManager::layer_exists(const std::string& layer_id) const {
    return layers_.find(layer_id) != layers_.end();
}

std::string DrawingLayerManager::generate_layer_id() {
    static int counter = 0;
    return "layer_" + std::to_string(++counter);
}

} // namespace bendcore::drawing
