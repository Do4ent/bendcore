#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Assembly drawing configuration
 */
struct AssemblyDrawing {
    EntityId drawing_id;
    std::string drawing_title;
    std::string drawing_number;
    std::string revision;
    std::string sheet_size;  // A3, A2, A1, A0
    std::vector<std::string> view_types;  // Front, Top, Right, Bottom, Isometric, etc.
    bool include_dimensions = true;
    bool include_annotations = true;
};

/**
 * @brief Training material specification
 */
struct TrainingMaterial {
    EntityId material_id;
    std::string title;
    std::string description;
    std::string content_type;  // PDF, Video, HTML, etc.
    std::vector<std::string> topics;
    std::string target_audience;
    uint32_t estimated_duration_minutes = 0;
};

/**
 * @brief Documentation package
 */
struct DocumentationPackage {
    std::string package_name;
    std::vector<AssemblyDrawing> assembly_drawings;
    std::vector<std::string> parts_lists;
    std::vector<std::string> work_instructions;
    std::vector<std::string> exploded_views;
    std::vector<TrainingMaterial> training_materials;
    std::string package_description;
    std::string created_date;
    std::string created_by;
};

/**
 * @brief Assembly Documentation Generator - Creates documentation
 *
 * Provides:
 * - Assembly drawings
 * - Parts lists generation
 * - Work instructions
 * - Exploded views
 * - Training materials
 */
class AssemblyDocumentationGenerator {
public:
    /**
     * @brief Constructor
     * @param assembly_id Assembly ID
     * @param assembly_name Assembly name
     */
    explicit AssemblyDocumentationGenerator(const EntityId& assembly_id,
                                            const std::string& assembly_name = "") noexcept
        : assembly_id_(assembly_id), assembly_name_(assembly_name) {}

    /**
     * @brief Create assembly drawing
     * @param drawing Assembly drawing configuration
     * @return Drawing ID
     */
    EntityId createAssemblyDrawing(const AssemblyDrawing& drawing);

    /**
     * @brief Get assembly drawing
     * @param drawing_id Drawing ID
     * @return Drawing info, or empty optional if not found
     */
    [[nodiscard]] std::optional<AssemblyDrawing> getAssemblyDrawing(
        const EntityId& drawing_id) const;

    /**
     * @brief Get all assembly drawings
     */
    [[nodiscard]] const std::vector<AssemblyDrawing>& getAllDrawings() const noexcept {
        return assembly_drawings_;
    }

    /**
     * @brief Generate parts list
     * @return Parts list as formatted text
     */
    [[nodiscard]] std::string generatePartsList() const;

    /**
     * @brief Generate work instructions document
     * @return Work instructions as formatted text
     */
    [[nodiscard]] std::string generateWorkInstructions() const;

    /**
     * @brief Generate exploded view description
     * @return Exploded view description text
     */
    [[nodiscard]] std::string generateExplodedViewDescription() const;

    /**
     * @brief Add training material
     * @param material Training material specification
     * @return Material ID
     */
    EntityId addTrainingMaterial(const TrainingMaterial& material);

    /**
     * @brief Get training material
     * @param material_id Material ID
     * @return Training material, or empty optional if not found
     */
    [[nodiscard]] std::optional<TrainingMaterial> getTrainingMaterial(
        const EntityId& material_id) const;

    /**
     * @brief Get all training materials
     */
    [[nodiscard]] const std::vector<TrainingMaterial>& getAllTrainingMaterials() const noexcept {
        return training_materials_;
    }

    /**
     * @brief Generate complete documentation package
     * @param created_by Author name
     * @return Documentation package
     */
    [[nodiscard]] DocumentationPackage generateDocumentationPackage(
        const std::string& created_by = "");

    /**
     * @brief Export documentation as HTML
     * @return HTML content
     */
    [[nodiscard]] std::string exportAsHTML() const;

    /**
     * @brief Export documentation as text
     * @return Text content
     */
    [[nodiscard]] std::string exportAsText() const;

    /**
     * @brief Create drawing title block
     * @param drawing_number Drawing number
     * @param revision Revision letter
     * @param company_name Company name
     * @return Title block text
     */
    [[nodiscard]] static std::string createTitleBlock(const std::string& drawing_number,
                                                       const std::string& revision,
                                                       const std::string& company_name);

    /**
     * @brief Generate drawing revision history
     * @return Revision history text
     */
    [[nodiscard]] std::string generateRevisionHistory() const;

    /**
     * @brief Get documentation drawing count
     */
    [[nodiscard]] size_t getDrawingCount() const noexcept { return assembly_drawings_.size(); }

    /**
     * @brief Set assembly revision for documentation
     */
    void setAssemblyRevision(uint32_t revision) noexcept { assembly_revision_ = revision; }

    /**
     * @brief Get assembly revision
     */
    [[nodiscard]] uint32_t getAssemblyRevision() const noexcept { return assembly_revision_; }

    /**
     * @brief Add custom documentation note
     */
    void addDocumentationNote(const std::string& note);

    /**
     * @brief Get all documentation notes
     */
    [[nodiscard]] const std::vector<std::string>& getDocumentationNotes() const noexcept {
        return documentation_notes_;
    }

private:
    EntityId assembly_id_;
    std::string assembly_name_;
    std::vector<AssemblyDrawing> assembly_drawings_;
    std::vector<TrainingMaterial> training_materials_;
    std::vector<std::string> documentation_notes_;
    uint32_t assembly_revision_ = 0;
};

} // namespace BendCore::CadCore
