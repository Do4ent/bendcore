#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::dependencies {

/**
 * @brief Dependency node
 */
class DependencyNode {
public:
    DependencyNode(const std::string& id) : id_(id) {}

    const std::string& getId() const { return id_; }

    void addDependency(const std::string& dependency_id) {
        dependencies_.push_back(dependency_id);
    }

    const std::vector<std::string>& getDependencies() const {
        return dependencies_;
    }

private:
    std::string id_;
    std::vector<std::string> dependencies_;
};

/**
 * @brief Dependency graph
 */
class DependencyGraph {
public:
    void addNode(const std::string& id) {
        nodes_[id] = std::make_shared<DependencyNode>(id);
    }

    void addDependency(const std::string& from, const std::string& to) {
        if (nodes_.find(from) != nodes_.end()) {
            nodes_[from]->addDependency(to);
        }
    }

    std::shared_ptr<DependencyNode> getNode(const std::string& id) {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool hasCycle() const { return false; }

    std::vector<std::string> getTopologicalOrder() const {
        std::vector<std::string> result;
        for (const auto& node : nodes_) {
            result.push_back(node.first);
        }
        return result;
    }

private:
    std::map<std::string, std::shared_ptr<DependencyNode>> nodes_;
};

} // namespace bendcore::dependencies
