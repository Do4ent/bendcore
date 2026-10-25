#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace BendCore::CadCore {

/**
 * @brief Material properties for rendering
 */
class Material {
public:
    /**
     * @brief Material type
     */
    enum class MaterialType {
        STEEL,
        ALUMINUM,
        COPPER,
        PLASTIC,
        CUSTOM,
    };

    /**
     * @brief Constructor
     */
    Material(MaterialType type = MaterialType::STEEL)
        : type_(type),
          diffuse_(0.5, 0.5, 0.5),
          specular_(1.0, 1.0, 1.0),
          shininess_(32.0) {}

    /**
     * @brief Set diffuse color
     */
    void setDiffuseColor(const Vector3D& color) noexcept { diffuse_ = color; }

    /**
     * @brief Get diffuse color
     */
    [[nodiscard]] const Vector3D& getDiffuseColor() const noexcept { return diffuse_; }

    /**
     * @brief Set specular properties
     */
    void setSpecular(const Vector3D& color, double shininess) noexcept {
        specular_ = color;
        shininess_ = shininess;
    }

private:
    MaterialType type_;
    Vector3D diffuse_;
    Vector3D specular_;
    double shininess_;
};

/**
 * @brief Rendering view modes
 */
class ViewMode {
public:
    /**
     * @brief Display mode
     */
    enum class DisplayMode {
        WIREFRAME,      // Edges only
        SHADED,         // Flat shading
        SMOOTH,         // Smooth shading
        REALISTIC,      // Physically-based rendering
        HIDDEN_LINE,    // Hidden lines removed
        SECTION,        // Section view (cut plane)
    };

    /**
     * @brief Constructor
     */
    explicit ViewMode(DisplayMode mode = DisplayMode::SHADED)
        : mode_(mode), show_edges_(false), show_vertices_(false) {}

    /**
     * @brief Set display mode
     */
    void setDisplayMode(DisplayMode mode) noexcept { mode_ = mode; }

    /**
     * @brief Get display mode
     */
    [[nodiscard]] DisplayMode getDisplayMode() const noexcept { return mode_; }

    /**
     * @brief Toggle edges
     */
    void setShowEdges(bool show) noexcept { show_edges_ = show; }

    /**
     * @brief Are edges shown
     */
    [[nodiscard]] bool areEdgesShown() const noexcept { return show_edges_; }

private:
    DisplayMode mode_;
    bool show_edges_;
    bool show_vertices_;
};

/**
 * @brief Lighting configuration
 */
class LightingConfig {
public:
    /**
     * @brief Light type
     */
    enum class LightType {
        AMBIENT,        // Ambient light
        DIRECTIONAL,    // Sun-like light
        POINT,          // Point light source
        SPOTLIGHT,      // Focused light
    };

    /**
     * @brief Constructor
     */
    LightingConfig() : ambient_strength_(0.3), light_count_(0) {}

    /**
     * @brief Add light source
     */
    void addLight(LightType type, const Vector3D& position, const Vector3D& color) noexcept;

    /**
     * @brief Set ambient strength
     */
    void setAmbientStrength(double strength) noexcept {
        ambient_strength_ = std::max(0.0, std::min(1.0, strength));
    }

    /**
     * @brief Get number of lights
     */
    [[nodiscard]] int getLightCount() const noexcept { return light_count_; }

private:
    double ambient_strength_;
    int light_count_;
};

/**
 * @brief Camera and viewport configuration
 */
class CameraConfig {
public:
    /**
     * @brief Constructor
     */
    CameraConfig() : position_(0, 0, 100), target_(0, 0, 0), fov_(45.0) {}

    /**
     * @brief Set camera position
     */
    void setPosition(const Vector3D& pos) noexcept { position_ = pos; }

    /**
     * @brief Get camera position
     */
    [[nodiscard]] const Vector3D& getPosition() const noexcept { return position_; }

    /**
     * @brief Set look-at target
     */
    void setTarget(const Vector3D& target) noexcept { target_ = target; }

    /**
     * @brief Get field of view
     */
    [[nodiscard]] double getFOV() const noexcept { return fov_; }

    /**
     * @brief Set field of view
     */
    void setFOV(double fov) noexcept { fov_ = std::max(10.0, std::min(120.0, fov)); }

private:
    Vector3D position_;
    Vector3D target_;
    double fov_;
};

/**
 * @brief Rendering viewport
 */
class RenderViewport {
public:
    /**
     * @brief Constructor
     */
    RenderViewport(int width, int height)
        : width_(width), height_(height),
          view_mode_(ViewMode::DisplayMode::SHADED) {}

    /**
     * @brief Set viewport size
     */
    void setSize(int width, int height) noexcept {
        width_ = width;
        height_ = height;
    }

    /**
     * @brief Get viewport width
     */
    [[nodiscard]] int getWidth() const noexcept { return width_; }

    /**
     * @brief Get viewport height
     */
    [[nodiscard]] int getHeight() const noexcept { return height_; }

    /**
     * @brief Set view mode
     */
    void setViewMode(const ViewMode& mode) noexcept { view_mode_ = mode; }

    /**
     * @brief Get view mode
     */
    [[nodiscard]] const ViewMode& getViewMode() const noexcept { return view_mode_; }

    /**
     * @brief Get camera configuration
     */
    [[nodiscard]] CameraConfig& getCameraConfig() noexcept { return camera_; }

    /**
     * @brief Get lighting configuration
     */
    [[nodiscard]] LightingConfig& getLightingConfig() noexcept { return lighting_; }

    /**
     * @brief Request frame render
     */
    [[nodiscard]] bool renderFrame() noexcept;

private:
    int width_;
    int height_;
    ViewMode view_mode_;
    CameraConfig camera_;
    LightingConfig lighting_;
};

/**
 * @brief Visualization engine coordinator
 */
class VisualizationEngine {
public:
    /**
     * @brief Constructor
     */
    VisualizationEngine(const EntityId& assembly_id)
        : assembly_id_(assembly_id), viewport_(1024, 768) {}

    /**
     * @brief Get or create viewport
     */
    [[nodiscard]] RenderViewport& getViewport() noexcept {
        return viewport_;
    }

    /**
     * @brief Set material for entity
     */
    void setMaterial(const EntityId& entity_id, const Material& material) noexcept {
        materials_[entity_id] = material;
    }

    /**
     * @brief Get material for entity
     */
    [[nodiscard]] bool getMaterial(const EntityId& entity_id, Material& material) const noexcept;

    /**
     * @brief Set visibility of entity
     */
    void setVisibility(const EntityId& entity_id, bool visible) noexcept {
        visibility_[entity_id] = visible;
    }

    /**
     * @brief Is entity visible
     */
    [[nodiscard]] bool isVisible(const EntityId& entity_id) const noexcept;

    /**
     * @brief Create section view (cut plane)
     */
    [[nodiscard]] bool createSectionView(const Vector3D& plane_normal) noexcept;

    /**
     * @brief Get material count
     */
    [[nodiscard]] size_t getMaterialCount() const noexcept { return materials_.size(); }

    /**
     * @brief Render scene
     */
    [[nodiscard]] bool renderScene() noexcept;

private:
    EntityId assembly_id_;
    RenderViewport viewport_;
    std::map<EntityId, Material> materials_;
    std::map<EntityId, bool> visibility_;
};

} // namespace BendCore::CadCore
