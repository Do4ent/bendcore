#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace bendcore::assembly {

struct ReferenceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Forward declarations
class Part;
class PartReference;
class ReferenceGraph;

/**
 * @brief Metadata about a part reference
 */
struct ReferenceMetadata {
    std::string creator;                                           // User or system that created the reference
    std::chrono::system_clock::time_point creation_time;          // When the reference was created
    std::chrono::system_clock::time_point last_modified;          // Last modification time
    std::string last_modifier;                                     // Who last modified this reference
    int version = 1;                                               // Reference version number
    std::map<std::string, std::string> custom_properties;         // Custom metadata
};

/**
 * @brief Represents a reference to a Part
 * Tracks relationships between parts in assemblies
 */
class PartReference {
public:
    /**
     * @brief Default constructor - creates invalid reference
     */
    PartReference() : reference_id_(""), referenced_part_id_(""), is_valid_(false) {
        metadata_.creation_time = std::chrono::system_clock::now();
        metadata_.last_modified = metadata_.creation_time;
    }

    /**
     * @brief Constructor
     * @param reference_id Unique identifier for this reference
     * @param referenced_part_id ID of the part being referenced
     * @param creator User or system creating this reference
     */
    explicit PartReference(const std::string& reference_id, const std::string& referenced_part_id,
                          const std::string& creator = "system");

    /**
     * @brief Get reference ID
     */
    [[nodiscard]] const std::string& getReferenceId() const noexcept { return reference_id_; }

    /**
     * @brief Get ID of referenced part
     */
    [[nodiscard]] const std::string& getReferencedPartId() const noexcept { return referenced_part_id_; }

    /**
     * @brief Get reference metadata
     */
    [[nodiscard]] const ReferenceMetadata& getMetadata() const noexcept { return metadata_; }

    /**
     * @brief Check if reference is valid
     */
    [[nodiscard]] bool isValid() const noexcept { return is_valid_; }

    /**
     * @brief Mark reference as invalid
     */
    void invalidate() noexcept { is_valid_ = false; }

    /**
     * @brief Update the referenced part
     * @param new_part_id ID of the new part to reference
     * @param modifier Who made this change
     */
    void updateReferencedPart(const std::string& new_part_id, const std::string& modifier = "system");

    /**
     * @brief Get custom property
     */
    [[nodiscard]] std::optional<std::string> getCustomProperty(const std::string& key) const;

    /**
     * @brief Set custom property
     */
    void setCustomProperty(const std::string& key, const std::string& value);

    /**
     * @brief Get creation time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getCreationTime() const noexcept {
        return metadata_.creation_time;
    }

    /**
     * @brief Get last modification time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getLastModifiedTime() const noexcept {
        return metadata_.last_modified;
    }

    /**
     * @brief Get version number
     */
    [[nodiscard]] int getVersion() const noexcept { return metadata_.version; }

private:
    std::string reference_id_;
    std::string referenced_part_id_;
    ReferenceMetadata metadata_;
    bool is_valid_ = true;
};

/**
 * @brief Represents a node in the reference dependency graph
 */
struct ReferenceNode {
    std::string part_id;
    std::vector<std::string> referenced_parts;  // Parts this one references
    std::vector<std::string> referencing_parts; // Parts that reference this one
    int nesting_depth = 0;
    int usage_count = 0;
};

/**
 * @brief Reference dependency graph for tracking part relationships
 */
class ReferenceGraph {
public:
    /**
     * @brief Constructor
     */
    ReferenceGraph() = default;

    /**
     * @brief Add a node to the graph
     */
    void addNode(const std::string& part_id);

    /**
     * @brief Add an edge (reference relationship) between two parts
     * @return true if edge was added, false if it would create a cycle
     */
    bool addEdge(const std::string& from_part_id, const std::string& to_part_id);

    /**
     * @brief Remove an edge
     */
    void removeEdge(const std::string& from_part_id, const std::string& to_part_id);

    /**
     * @brief Remove a node (all its edges)
     */
    void removeNode(const std::string& part_id);

    /**
     * @brief Get a node
     */
    [[nodiscard]] std::optional<ReferenceNode> getNode(const std::string& part_id) const;

    /**
     * @brief Get all nodes
     */
    [[nodiscard]] std::vector<ReferenceNode> getAllNodes() const;

    /**
     * @brief Get parts referenced by a part
     */
    [[nodiscard]] std::vector<std::string> getReferencedParts(const std::string& part_id) const;

    /**
     * @brief Get parts that reference a part
     */
    [[nodiscard]] std::vector<std::string> getReferencingParts(const std::string& part_id) const;

    /**
     * @brief Check if adding an edge would create a cycle
     */
    [[nodiscard]] bool wouldCreateCycle(const std::string& from_part_id, const std::string& to_part_id) const;

    /**
     * @brief Get dependency hierarchy (tree structure)
     * Returns vector of parts sorted by nesting depth (dependencies first)
     */
    [[nodiscard]] std::vector<std::string> getHierarchy(const std::string& root_part_id) const;

    /**
     * @brief Get statistics about the graph
     */
    struct GraphStats {
        int total_nodes = 0;
        int total_edges = 0;
        int max_depth = 0;
        double avg_usage = 0.0;
    };
    [[nodiscard]] GraphStats getStatistics() const;

    /**
     * @brief Clear the graph
     */
    void clear() noexcept;

    /**
     * @brief Check if graph is empty
     */
    [[nodiscard]] bool isEmpty() const noexcept { return nodes_.empty(); }

private:
    std::map<std::string, ReferenceNode> nodes_;

    /**
     * @brief Detect cycle using DFS
     */
    bool hasCycleDFS(const std::string& node, const std::string& target, std::set<std::string>& visited) const;

    /**
     * @brief Update nesting depth for all nodes
     */
    void updateNestingDepths();

    /**
     * @brief Calculate depth for a specific node
     */
    int calculateNestingDepth(const std::string& part_id, std::set<std::string>& visited) const;

    /**
     * @brief Update usage counts
     */
    void updateUsageCounts();
};

/**
 * @brief Reference validator
 */
class ReferenceValidator {
public:
    /**
     * @brief Validation result
     */
    struct ValidationResult {
        bool is_valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        int broken_references = 0;
        int circular_references = 0;
    };

    /**
     * @brief Validate reference integrity
     */
    [[nodiscard]] static ValidationResult validateReference(const PartReference& ref);

    /**
     * @brief Validate entire graph
     */
    [[nodiscard]] static ValidationResult validateGraph(const ReferenceGraph& graph);

    /**
     * @brief Check for circular references
     */
    [[nodiscard]] static bool hasCircularReferences(const ReferenceGraph& graph);

    /**
     * @brief Detect all circular reference chains
     */
    [[nodiscard]] static std::vector<std::vector<std::string>> findCircularChains(const ReferenceGraph& graph);

private:
    static bool dfsCircleDetect(const std::string& node, const std::string& start,
                               std::set<std::string>& visited, std::vector<std::string>& path,
                               const ReferenceGraph& graph);
};

/**
 * @brief Reference resolver
 */
class ReferenceResolver {
public:
    /**
     * @brief Resolve a part reference to actual part
     * @param reference The reference to resolve
     * @return Part ID if valid, empty string otherwise
     */
    [[nodiscard]] static std::string resolvePartReference(const PartReference& reference);

    /**
     * @brief Get all parts directly referenced by a part
     */
    [[nodiscard]] static std::vector<std::string> getReferencedParts(const std::string& part_id,
                                                                      const ReferenceGraph& graph);

    /**
     * @brief Get all parts that reference a part
     */
    [[nodiscard]] static std::vector<std::string> getReferencingParts(const std::string& part_id,
                                                                       const ReferenceGraph& graph);

    /**
     * @brief Get the complete dependency chain for a part
     */
    [[nodiscard]] static std::vector<std::string> getDependencyChain(const std::string& part_id,
                                                                      const ReferenceGraph& graph);
};

/**
 * @brief Reference registry and manager
 */
class ReferenceRegistry {
public:
    /**
     * @brief Constructor
     */
    ReferenceRegistry() = default;

    /**
     * @brief Create a new part reference
     */
    void createReference(const std::string& reference_id, const std::string& part_id,
                        const std::string& referenced_part_id, const std::string& creator = "system");

    /**
     * @brief Get a reference
     */
    [[nodiscard]] const PartReference& getReference(const std::string& reference_id) const;

    /**
     * @brief Get all references
     */
    [[nodiscard]] std::vector<PartReference> getAllReferences() const;

    /**
     * @brief Get references for a part
     */
    [[nodiscard]] std::vector<std::string> getReferencesForPart(const std::string& part_id) const;

    /**
     * @brief Update a reference
     */
    void updateReference(const std::string& reference_id, const std::string& new_part_id,
                        const std::string& modifier = "system");

    /**
     * @brief Remove a reference
     */
    void removeReference(const std::string& reference_id);

    /**
     * @brief Update part reference
     */
    void updatePartReference(const std::string& reference_id, const std::string& new_part_id,
                            const std::string& modifier = "system");

    /**
     * @brief Get reference graph
     */
    [[nodiscard]] const ReferenceGraph& getGraph() const noexcept { return graph_; }

    /**
     * @brief Validate all references
     */
    [[nodiscard]] ReferenceValidator::ValidationResult validateAll() const;

    /**
     * @brief Get statistics
     */
    [[nodiscard]] ReferenceGraph::GraphStats getStatistics() const { return graph_.getStatistics(); }

    /**
     * @brief Clear all references
     */
    void clear() noexcept;

private:
    std::map<std::string, PartReference> references_;
    ReferenceGraph graph_;
};

}  // namespace bendcore::assembly
