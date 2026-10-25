#include "bendcore/project/project_format.hpp"
#include <ctime>

namespace BendCore::CadCore {

bool ProjectFormat::createProject(const ProjectMetadata& metadata) noexcept {
    metadata_ = metadata;
    metadata_.createdTime = std::time(nullptr);
    metadata_.modifiedTime = std::time(nullptr);
    return true;
}

void ProjectFormat::setMetadata(const ProjectMetadata& metadata) noexcept {
    metadata_ = metadata;
    metadata_.modifiedTime = std::time(nullptr);
}

void ProjectFormat::addResource(const std::string& resourceId, const std::string& filePath) noexcept {
    resources_[resourceId] = filePath;
}

std::string ProjectFormat::getResource(const std::string& resourceId) const noexcept {
    auto it = resources_.find(resourceId);
    return it != resources_.end() ? it->second : "";
}

bool ProjectFormat::validate() const noexcept {
    if (metadata_.name.empty()) return false;
    if (metadata_.version.empty()) return false;
    return true;
}

std::string ProjectFormat::getFormatVersion() noexcept {
    return FORMAT_VERSION;
}

} // namespace BendCore::CadCore
