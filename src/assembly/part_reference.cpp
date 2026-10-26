#include "bendcore/assembly/part_reference.hpp"

#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>

namespace bendcore::assembly {

// ========== PartReference ==========

PartReference::PartReference(const std::string& reference_id, const std::string& referenced_part_id,
                             const std::string& creator)
    : reference_id_(reference_id), referenced_part_id_(referenced_part_id) {
    if (reference_id.empty()) {
        throw ReferenceError("Reference ID cannot be empty");
    }
    if (referenced_part_id.empty()) {
        throw ReferenceError("Referenced part ID cannot be empty");
    }

    metadata_.creator = creator;
    metadata_.last_modifier = creator;
    metadata_.creation_time = std::chrono::system_clock::now();
    metadata_.last_modified = metadata_.creation_time;
    metadata_.version = 1;
}

void PartReference::updateReferencedPart(const std::string& new_part_id, const std::string& modifier) {
    if (new_part_id.empty()) {
        throw ReferenceError("New part ID cannot be empty");
    }
    referenced_part_id_ = new_part_id;
    metadata_.last_modifier = modifier;
    metadata_.last_modified = std::chrono::system_clock::now();
    metadata_.version++;
}

std::optional<std::string> PartReference::getCustomProperty(const std::string& key) const {
    const auto it = metadata_.custom_properties.find(key);
    if (it != metadata_.custom_properties.end()) {
        return it->second;
    }
    return std::nullopt;
}

void PartReference::setCustomProperty(const std::string& key, const std::string& value) {
    metadata_.custom_properties[key] = value;
    metadata_.last_modified = std::chrono::system_clock::now();
}

// ========== ReferenceGraph ==========

void ReferenceGraph::addNode(const std::string& part_id) {
    if (part_id.empty()) {
        throw ReferenceError("Part ID cannot be empty");
    }
    if (nodes_.find(part_id) == nodes_.end()) {
        ReferenceNode node;
        node.part_id = part_id;
        nodes_[part_id] = node;
    }
}

bool ReferenceGraph::addEdge(const std::string& from_part_id, const std::string& to_part_id) {
    if (from_part_id.empty() || to_part_id.empty()) {
        throw ReferenceError("Part IDs cannot be empty");
    }
    if (from_part_id == to_part_id) {
        throw ReferenceError("Self-referencing is not allowed");
    }

    // Check if nodes exist, create if needed
    if (nodes_.find(from_part_id) == nodes_.end()) {
        addNode(from_part_id);
    }
    if (nodes_.find(to_part_id) == nodes_.end()) {
        addNode(to_part_id);
    }

    // Check for cycles
    if (wouldCreateCycle(from_part_id, to_part_id)) {
        return false;
    }

    // Add edge
    auto& from_node = nodes_[from_part_id];
    auto& to_node = nodes_[to_part_id];

    if (std::find(from_node.referenced_parts.begin(), from_node.referenced_parts.end(), to_part_id) ==
        from_node.referenced_parts.end()) {
        from_node.referenced_parts.push_back(to_part_id);
    }

    if (std::find(to_node.referencing_parts.begin(), to_node.referencing_parts.end(), from_part_id) ==
        to_node.referencing_parts.end()) {
        to_node.referencing_parts.push_back(from_part_id);
    }

    updateNestingDepths();
    updateUsageCounts();

    return true;
}

void ReferenceGraph::removeEdge(const std::string& from_part_id, const std::string& to_part_id) {
    if (nodes_.find(from_part_id) == nodes_.end() || nodes_.find(to_part_id) == nodes_.end()) {
        return;
    }

    auto& from_node = nodes_[from_part_id];
    auto& to_node = nodes_[to_part_id];

    auto it = std::find(from_node.referenced_parts.begin(), from_node.referenced_parts.end(), to_part_id);
    if (it != from_node.referenced_parts.end()) {
        from_node.referenced_parts.erase(it);
    }

    it = std::find(to_node.referencing_parts.begin(), to_node.referencing_parts.end(), from_part_id);
    if (it != to_node.referencing_parts.end()) {
        to_node.referencing_parts.erase(it);
    }

    updateNestingDepths();
    updateUsageCounts();
}

void ReferenceGraph::removeNode(const std::string& part_id) {
    if (nodes_.find(part_id) == nodes_.end()) {
        return;
    }

    auto& node = nodes_[part_id];

    // Remove all edges pointing from this node
    for (const auto& referenced : node.referenced_parts) {
        removeEdge(part_id, referenced);
    }

    // Remove all edges pointing to this node
    for (const auto& referencing : node.referencing_parts) {
        removeEdge(referencing, part_id);
    }

    nodes_.erase(part_id);
    updateNestingDepths();
    updateUsageCounts();
}

std::optional<ReferenceNode> ReferenceGraph::getNode(const std::string& part_id) const {
    const auto it = nodes_.find(part_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ReferenceNode> ReferenceGraph::getAllNodes() const {
    std::vector<ReferenceNode> result;
    for (const auto& [part_id, node] : nodes_) {
        result.push_back(node);
    }
    return result;
}

std::vector<std::string> ReferenceGraph::getReferencedParts(const std::string& part_id) const {
    const auto it = nodes_.find(part_id);
    if (it != nodes_.end()) {
        return it->second.referenced_parts;
    }
    return {};
}

std::vector<std::string> ReferenceGraph::getReferencingParts(const std::string& part_id) const {
    const auto it = nodes_.find(part_id);
    if (it != nodes_.end()) {
        return it->second.referencing_parts;
    }
    return {};
}

bool ReferenceGraph::wouldCreateCycle(const std::string& from_part_id, const std::string& to_part_id) const {
    // Check if path exists from to_part_id to from_part_id
    // If it does, adding edge from_part_id -> to_part_id would create a cycle
    std::set<std::string> visited;
    return hasCycleDFS(to_part_id, from_part_id, visited);
}

std::vector<std::string> ReferenceGraph::getHierarchy(const std::string& root_part_id) const {
    if (nodes_.find(root_part_id) == nodes_.end()) {
        return {};
    }

    std::vector<std::string> hierarchy;
    std::set<std::string> visited;
    std::deque<std::string> queue;

    queue.push_back(root_part_id);
    visited.insert(root_part_id);

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop_front();
        hierarchy.push_back(current);

        const auto& node = nodes_.at(current);
        for (const auto& referenced : node.referenced_parts) {
            if (visited.find(referenced) == visited.end()) {
                visited.insert(referenced);
                queue.push_back(referenced);
            }
        }
    }

    return hierarchy;
}

ReferenceGraph::GraphStats ReferenceGraph::getStatistics() const {
    GraphStats stats;
    stats.total_nodes = static_cast<int>(nodes_.size());

    int total_edges = 0;
    int max_depth = 0;
    int total_usage = 0;

    for (const auto& [part_id, node] : nodes_) {
        total_edges += static_cast<int>(node.referenced_parts.size());
        max_depth = std::max(max_depth, node.nesting_depth);
        total_usage += node.usage_count;
    }

    stats.total_edges = total_edges;
    stats.max_depth = max_depth;
    stats.avg_usage = nodes_.empty() ? 0.0 : static_cast<double>(total_usage) / nodes_.size();

    return stats;
}

void ReferenceGraph::clear() noexcept {
    nodes_.clear();
}

bool ReferenceGraph::hasCycleDFS(const std::string& node, const std::string& target,
                                std::set<std::string>& visited) const {
    if (node == target) {
        return true;
    }

    if (visited.find(node) != visited.end()) {
        return false;
    }

    visited.insert(node);

    const auto it = nodes_.find(node);
    if (it != nodes_.end()) {
        for (const auto& referenced : it->second.referenced_parts) {
            if (hasCycleDFS(referenced, target, visited)) {
                return true;
            }
        }
    }

    return false;
}

void ReferenceGraph::updateNestingDepths() {
    for (auto& [part_id, node] : nodes_) {
        std::set<std::string> visited;
        node.nesting_depth = calculateNestingDepth(part_id, visited);
    }
}

int ReferenceGraph::calculateNestingDepth(const std::string& part_id, std::set<std::string>& visited) const {
    if (visited.find(part_id) != visited.end()) {
        return 0;  // Cycle or already visited
    }

    visited.insert(part_id);

    const auto it = nodes_.find(part_id);
    if (it == nodes_.end() || it->second.referenced_parts.empty()) {
        return 0;
    }

    int max_depth = 0;
    for (const auto& referenced : it->second.referenced_parts) {
        std::set<std::string> temp_visited;
        int depth = calculateNestingDepth(referenced, temp_visited);
        max_depth = std::max(max_depth, depth + 1);
    }

    return max_depth;
}

void ReferenceGraph::updateUsageCounts() {
    for (auto& [part_id, node] : nodes_) {
        node.usage_count = static_cast<int>(node.referencing_parts.size());
    }
}

// ========== ReferenceValidator ==========

ReferenceValidator::ValidationResult ReferenceValidator::validateReference(const PartReference& ref) {
    ValidationResult result;

    if (ref.getReferenceId().empty()) {
        result.is_valid = false;
        result.errors.push_back("Reference ID is empty");
    }

    if (ref.getReferencedPartId().empty()) {
        result.is_valid = false;
        result.errors.push_back("Referenced part ID is empty");
    }

    if (!ref.isValid()) {
        result.is_valid = false;
        result.errors.push_back("Reference marked as invalid");
        result.broken_references = 1;
    }

    return result;
}

ReferenceValidator::ValidationResult ReferenceValidator::validateGraph(const ReferenceGraph& graph) {
    ValidationResult result;

    // Check for circular references
    if (hasCircularReferences(graph)) {
        result.is_valid = false;
        const auto circles = findCircularChains(graph);
        result.circular_references = static_cast<int>(circles.size());
        for (const auto& circle : circles) {
            std::string chain;
            for (const auto& part : circle) {
                if (!chain.empty()) chain += " -> ";
                chain += part;
            }
            result.errors.push_back("Circular reference: " + chain);
        }
    }

    return result;
}

bool ReferenceValidator::hasCircularReferences(const ReferenceGraph& graph) {
    const auto circles = findCircularChains(graph);
    return !circles.empty();
}

std::vector<std::vector<std::string>> ReferenceValidator::findCircularChains(const ReferenceGraph& graph) {
    std::vector<std::vector<std::string>> circles;
    const auto nodes = graph.getAllNodes();

    for (const auto& node : nodes) {
        std::set<std::string> visited;
        std::vector<std::string> path;
        if (dfsCircleDetect(node.part_id, node.part_id, visited, path, graph)) {
            circles.push_back(path);
        }
    }

    return circles;
}

bool ReferenceValidator::dfsCircleDetect(const std::string& node, const std::string& start,
                                         std::set<std::string>& visited, std::vector<std::string>& path,
                                         const ReferenceGraph& graph) {
    visited.insert(node);
    path.push_back(node);

    const auto node_opt = graph.getNode(node);
    if (!node_opt) {
        path.pop_back();
        return false;
    }

    for (const auto& referenced : node_opt->referenced_parts) {
        if (referenced == start && path.size() > 1) {
            return true;
        }

        if (visited.find(referenced) == visited.end()) {
            if (dfsCircleDetect(referenced, start, visited, path, graph)) {
                return true;
            }
        }
    }

    path.pop_back();
    return false;
}

// ========== ReferenceResolver ==========

std::string ReferenceResolver::resolvePartReference(const PartReference& reference) {
    if (!reference.isValid()) {
        return "";
    }
    return reference.getReferencedPartId();
}

std::vector<std::string> ReferenceResolver::getReferencedParts(const std::string& part_id,
                                                               const ReferenceGraph& graph) {
    return graph.getReferencedParts(part_id);
}

std::vector<std::string> ReferenceResolver::getReferencingParts(const std::string& part_id,
                                                                const ReferenceGraph& graph) {
    return graph.getReferencingParts(part_id);
}

std::vector<std::string> ReferenceResolver::getDependencyChain(const std::string& part_id,
                                                               const ReferenceGraph& graph) {
    return graph.getHierarchy(part_id);
}

// ========== ReferenceRegistry ==========

void ReferenceRegistry::createReference(const std::string& reference_id, const std::string& part_id,
                                       const std::string& referenced_part_id, const std::string& creator) {
    if (references_.find(reference_id) != references_.end()) {
        throw ReferenceError("Reference already exists: " + reference_id);
    }

    PartReference ref(reference_id, referenced_part_id, creator);
    references_[reference_id] = ref;

    // Add to graph
    graph_.addNode(part_id);
    graph_.addNode(referenced_part_id);
    graph_.addEdge(part_id, referenced_part_id);
}

const PartReference& ReferenceRegistry::getReference(const std::string& reference_id) const {
    const auto it = references_.find(reference_id);
    if (it == references_.end()) {
        throw ReferenceError("Reference not found: " + reference_id);
    }
    return it->second;
}

std::vector<PartReference> ReferenceRegistry::getAllReferences() const {
    std::vector<PartReference> result;
    for (const auto& [ref_id, ref] : references_) {
        result.push_back(ref);
    }
    return result;
}

std::vector<std::string> ReferenceRegistry::getReferencesForPart(const std::string& part_id) const {
    std::vector<std::string> result;
    for (const auto& [ref_id, ref] : references_) {
        if (ref.getReferencedPartId() == part_id) {
            result.push_back(ref_id);
        }
    }
    return result;
}

void ReferenceRegistry::updateReference(const std::string& reference_id, const std::string& new_part_id,
                                       const std::string& modifier) {
    const auto it = references_.find(reference_id);
    if (it == references_.end()) {
        throw ReferenceError("Reference not found: " + reference_id);
    }

    const auto old_part_id = it->second.getReferencedPartId();
    it->second.updateReferencedPart(new_part_id, modifier);

    // Update graph
    // For now, we don't have the source part_id, so we can't update the graph edge
    // In a full implementation, we would need to track part_id -> reference_id mapping
}

void ReferenceRegistry::removeReference(const std::string& reference_id) {
    const auto it = references_.find(reference_id);
    if (it == references_.end()) {
        throw ReferenceError("Reference not found: " + reference_id);
    }

    references_.erase(it);
}

void ReferenceRegistry::updatePartReference(const std::string& reference_id, const std::string& new_part_id,
                                           const std::string& modifier) {
    updateReference(reference_id, new_part_id, modifier);
}

ReferenceValidator::ValidationResult ReferenceRegistry::validateAll() const {
    return ReferenceValidator::validateGraph(graph_);
}

void ReferenceRegistry::clear() noexcept {
    references_.clear();
    graph_.clear();
}

}  // namespace bendcore::assembly
