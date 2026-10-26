#pragma once

#include "drawing.hpp"
#include "../document/cad_document.hpp"
#include "../serialization/document_serializer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace BendCore::CadCore {

/**
 * @brief Result of drawing operations
 */
struct DrawingOperationResult {
    bool success = false;
    std::string error;
    EntityId created_entity_id;  // For operations that create entities

    DrawingOperationResult() = default;
    explicit DrawingOperationResult(bool ok, const std::string& err = "")
        : success(ok), error(err) {}
};

/**
 * @brief Drawing manager handling operations and serialization
 *
 * Manages:
 * - Drawing creation and deletion
 * - Sheet management (add, remove, modify)
 * - View management and linking
 * - Drawing serialization to JSON
 * - Drawing deserialization from JSON
 * - Assembly linkage
 * - Change notifications
 */
class DrawingManager {
public:
    /**
     * @brief Constructor
     */
    DrawingManager();

    /**
     * @brief Create new drawing from assembly/part
     */
    [[nodiscard]] DrawingOperationResult createDrawing(
        const std::string& name,
        const EntityId& source_id,
        const std::string& title = ""
    );

    /**
     * @brief Get drawing by ID
     */
    [[nodiscard]] std::shared_ptr<Drawing> getDrawing(const EntityId& drawing_id);

    /**
     * @brief Get drawing by ID (const)
     */
    [[nodiscard]] std::shared_ptr<const Drawing> getDrawing(const EntityId& drawing_id) const;

    /**
     * @brief Get all drawing IDs
     */
    [[nodiscard]] std::vector<EntityId> getAllDrawingIds() const;

    /**
     * @brief Delete drawing
     */
    [[nodiscard]] DrawingOperationResult deleteDrawing(const EntityId& drawing_id);

    /**
     * @brief Add sheet to drawing
     */
    [[nodiscard]] DrawingOperationResult addSheet(
        const EntityId& drawing_id,
        const std::string& name,
        SheetSize size = SheetSize::ISO_A4
    );

    /**
     * @brief Remove sheet from drawing
     */
    [[nodiscard]] DrawingOperationResult removeSheet(
        const EntityId& drawing_id,
        const EntityId& sheet_id
    );

    /**
     * @brief Add view to drawing
     */
    [[nodiscard]] DrawingOperationResult addView(
        const EntityId& drawing_id,
        const EntityId& source_geometry_id,
        const std::string& name
    );

    /**
     * @brief Remove view from drawing
     */
    [[nodiscard]] DrawingOperationResult removeView(
        const EntityId& drawing_id,
        const EntityId& view_id
    );

    /**
     * @brief Add view to sheet
     */
    [[nodiscard]] DrawingOperationResult addViewToSheet(
        const EntityId& drawing_id,
        const EntityId& sheet_id,
        const EntityId& view_id,
        const ViewProperties& props
    );

    /**
     * @brief Update view on sheet
     */
    [[nodiscard]] DrawingOperationResult updateViewOnSheet(
        const EntityId& drawing_id,
        const EntityId& sheet_id,
        const EntityId& view_id,
        const ViewProperties& props
    );

    /**
     * @brief Remove view from sheet
     */
    [[nodiscard]] DrawingOperationResult removeViewFromSheet(
        const EntityId& drawing_id,
        const EntityId& sheet_id,
        const EntityId& view_id
    );

    /**
     * @brief Serialize drawing to JSON
     */
    [[nodiscard]] std::string serializeToJson(const EntityId& drawing_id) const;

    /**
     * @brief Deserialize drawing from JSON
     */
    [[nodiscard]] DrawingOperationResult deserializeFromJson(const std::string& json_str);

    /**
     * @brief Save drawing to file
     */
    [[nodiscard]] DrawingOperationResult saveDrawingToFile(
        const EntityId& drawing_id,
        const std::string& filepath
    );

    /**
     * @brief Load drawing from file
     */
    [[nodiscard]] DrawingOperationResult loadDrawingFromFile(const std::string& filepath);

    /**
     * @brief Get drawing count
     */
    [[nodiscard]] size_t getDrawingCount() const noexcept { return drawings_.size(); }

    /**
     * @brief Clear all drawings
     */
    void clear() noexcept { drawings_.clear(); }

    /**
     * @brief Validate all drawings
     */
    [[nodiscard]] std::vector<std::string> validateAllDrawings() const;

    /**
     * @brief Create backup of drawing
     */
    [[nodiscard]] DrawingOperationResult backupDrawing(
        const EntityId& drawing_id,
        const std::string& backup_filepath
    );

private:
    std::map<EntityId, std::shared_ptr<Drawing>> drawings_;
    DocumentSerializer serializer_;

    /**
     * @brief Helper to convert Drawing to JSON
     */
    [[nodiscard]] std::string drawingToJson(const Drawing& drawing) const;

    /**
     * @brief Helper to parse JSON to Drawing
     */
    [[nodiscard]] std::optional<std::shared_ptr<Drawing>> jsonToDrawing(const std::string& json_str);
};

} // namespace BendCore::CadCore
