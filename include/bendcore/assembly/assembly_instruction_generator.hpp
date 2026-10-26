#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Tool specification for assembly
 */
struct ToolSpecification {
    EntityId tool_id;
    std::string tool_name;
    std::string tool_type;
    std::string description;
    std::string part_number;
    double estimated_cost = 0.0;
};

/**
 * @brief Fastener specification
 */
struct FastenerSpecification {
    EntityId fastener_id;
    std::string fastener_type;  // Bolt, screw, rivet, etc.
    std::string size;            // M6, #8-32, etc.
    std::string material;
    uint32_t quantity = 1;
    double torque_nm = 0.0;      // Torque in Newton-meters
    std::string thread_type;     // Coarse, fine, etc.
    std::string finish;
    std::string notes;
};

/**
 * @brief Assembly instruction step
 */
struct InstructionStep {
    uint32_t step_number = 0;
    std::string title;
    std::string description;
    std::vector<EntityId> components;
    std::vector<ToolSpecification> required_tools;
    std::vector<FastenerSpecification> required_fasteners;
    std::vector<std::string> warnings;
    std::vector<std::string> tips;
    double estimated_time_seconds = 0.0;
    std::string illustration_reference;  // Reference to illustration/drawing
};

/**
 * @brief Assembly work instruction document
 */
struct WorkInstruction {
    std::string title;
    std::string assembly_name;
    uint32_t revision = 0;
    std::string created_date;
    std::string created_by;
    std::vector<InstructionStep> steps;
    std::string safety_notes;
    std::string quality_requirements;
    std::vector<ToolSpecification> tool_list;
    std::vector<FastenerSpecification> fastener_list;
};

/**
 * @brief Assembly Instruction Generator - Creates assembly documentation
 *
 * Provides:
 * - Step-by-step assembly guides
 * - Tool and fastener specifications
 * - Illustrated instructions
 * - Work instruction generation
 * - Torque specification tracking
 */
class AssemblyInstructionGenerator {
public:
    /**
     * @brief Constructor
     * @param assembly_id Assembly ID
     * @param assembly_name Assembly name
     */
    explicit AssemblyInstructionGenerator(const EntityId& assembly_id,
                                          const std::string& assembly_name = "") noexcept
        : assembly_id_(assembly_id), assembly_name_(assembly_name) {}

    /**
     * @brief Add an instruction step
     * @param step Instruction step
     * @return Step number (1-based), or 0 if failed
     */
    uint32_t addInstructionStep(const InstructionStep& step);

    /**
     * @brief Remove an instruction step
     * @param step_number Step number to remove
     * @return true if removed, false if not found
     */
    bool removeInstructionStep(uint32_t step_number) noexcept;

    /**
     * @brief Get instruction step
     * @param step_number Step number
     * @return Step info, or empty optional if not found
     */
    [[nodiscard]] std::optional<InstructionStep> getInstructionStep(
        uint32_t step_number) const noexcept;

    /**
     * @brief Get all instruction steps
     */
    [[nodiscard]] const std::vector<InstructionStep>& getAllSteps() const noexcept {
        return steps_;
    }

    /**
     * @brief Update instruction step
     * @param step_number Step number to update
     * @param step Updated step
     * @return true if updated, false if not found
     */
    bool updateInstructionStep(uint32_t step_number, const InstructionStep& step) noexcept;

    /**
     * @brief Add a tool specification
     * @param tool Tool specification
     * @return Tool ID
     */
    EntityId addTool(const ToolSpecification& tool);

    /**
     * @brief Add a fastener specification
     * @param fastener Fastener specification
     * @return Fastener ID
     */
    EntityId addFastener(const FastenerSpecification& fastener);

    /**
     * @brief Get tool specification
     * @param tool_id Tool ID
     * @return Tool spec, or empty optional if not found
     */
    [[nodiscard]] std::optional<ToolSpecification> getTool(const EntityId& tool_id) const noexcept;

    /**
     * @brief Get fastener specification
     * @param fastener_id Fastener ID
     * @return Fastener spec, or empty optional if not found
     */
    [[nodiscard]] std::optional<FastenerSpecification> getFastener(
        const EntityId& fastener_id) const noexcept;

    /**
     * @brief Get all tools
     */
    [[nodiscard]] const std::vector<ToolSpecification>& getAllTools() const noexcept {
        return tools_;
    }

    /**
     * @brief Get all fasteners
     */
    [[nodiscard]] const std::vector<FastenerSpecification>& getAllFasteners() const noexcept {
        return fasteners_;
    }

    /**
     * @brief Set torque specification for a fastener
     * @param fastener_id Fastener ID
     * @param torque_nm Torque in Newton-meters
     * @return true if set, false if fastener not found
     */
    bool setFastenerTorque(const EntityId& fastener_id, double torque_nm) noexcept;

    /**
     * @brief Get fastener torque specification
     * @param fastener_id Fastener ID
     * @return Torque in Newton-meters, or -1 if not found
     */
    [[nodiscard]] double getFastenerTorque(const EntityId& fastener_id) const noexcept;

    /**
     * @brief Generate complete work instruction
     * @param created_by Author name
     * @return Work instruction document
     */
    [[nodiscard]] WorkInstruction generateWorkInstruction(const std::string& created_by = "");

    /**
     * @brief Export instructions as text document
     * @return Formatted text document
     */
    [[nodiscard]] std::string exportAsText() const;

    /**
     * @brief Get total estimated assembly time
     */
    [[nodiscard]] double getTotalEstimatedTime() const noexcept { return total_time_estimate_; }

    /**
     * @brief Get number of steps
     */
    [[nodiscard]] uint32_t getStepCount() const noexcept { return static_cast<uint32_t>(steps_.size()); }

    /**
     * @brief Set assembly revision
     */
    void setRevision(uint32_t revision) noexcept { revision_ = revision; }

    /**
     * @brief Get assembly revision
     */
    [[nodiscard]] uint32_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Add safety note
     */
    void addSafetyNote(const std::string& note);

    /**
     * @brief Get all safety notes
     */
    [[nodiscard]] const std::vector<std::string>& getSafetyNotes() const noexcept {
        return safety_notes_;
    }

private:
    EntityId assembly_id_;
    std::string assembly_name_;
    std::vector<InstructionStep> steps_;
    std::vector<ToolSpecification> tools_;
    std::vector<FastenerSpecification> fasteners_;
    std::vector<std::string> safety_notes_;
    uint32_t revision_ = 0;
    double total_time_estimate_ = 0.0;
};

} // namespace BendCore::CadCore
