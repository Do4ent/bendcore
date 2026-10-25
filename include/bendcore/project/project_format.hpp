#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <map>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Project metadata
 */
struct ProjectMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    uint64_t createdTime = 0;
    uint64_t modifiedTime = 0;
};

/**
 * @brief Project file format specification
 */
class ProjectFormat {
public:
    static constexpr const char* FORMAT_VERSION = "1.0.0";
    static constexpr const char* FILE_EXTENSION = ".cadcore";
    static constexpr const char* MANIFEST_FILE = "manifest.json";
    static constexpr const char* DOCUMENT_FILE = "document.dat";
    static constexpr const char* RESOURCES_DIR = "resources";

    ProjectFormat() noexcept = default;

    /**
     * @brief Create project with metadata
     */
    bool createProject(const ProjectMetadata& metadata) noexcept;

    /**
     * @brief Set metadata
     */
    void setMetadata(const ProjectMetadata& metadata) noexcept;

    /**
     * @brief Get metadata
     */
    const ProjectMetadata& getMetadata() const noexcept { return metadata_; }

    /**
     * @brief Add resource file mapping
     */
    void addResource(const std::string& resourceId, const std::string& filePath) noexcept;

    /**
     * @brief Get resource file path
     */
    std::string getResource(const std::string& resourceId) const noexcept;

    /**
     * @brief Get all resources
     */
    const std::map<std::string, std::string>& getResources() const noexcept { return resources_; }

    /**
     * @brief Validate project structure
     */
    bool validate() const noexcept;

    /**
     * @brief Get format version
     */
    static std::string getFormatVersion() noexcept;

private:
    ProjectMetadata metadata_;
    std::map<std::string, std::string> resources_;
};

} // namespace BendCore::CadCore
