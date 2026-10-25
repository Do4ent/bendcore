#pragma once

#include <string>
#include <vector>

namespace bendcore::testing {

/**
 * CAD-CORE-024: LaunchPackage
 *
 * Distribution and documentation package for Phase F production launch.
 * Generates all necessary documentation for users and developers.
 *
 * Features:
 * - README.md (user guide)
 * - API.md (developer documentation)
 * - CHANGELOG.md (v6.0.0 release notes)
 * - LICENSE (production license)
 * - INSTALLATION.md (installation guide)
 * - BUILDING.md (build from source)
 * - TESTING.md (test suite documentation)
 * - PERFORMANCE.md (performance benchmarks)
 * - SECURITY.md (security guidelines)
 * - DEPLOYMENT.md (production deployment)
 */

struct DocumentationFile {
  std::string filename;
  std::string title;
  std::string content;
  std::string section;
};

class LaunchPackage {
public:
  LaunchPackage();

  // Generate documentation files
  struct PackageContents {
    std::vector<DocumentationFile> documentation_files;
    std::string version;
    std::string build_date;
    std::string git_hash;
    bool is_complete;
  };

  PackageContents generate_package() const;

  // Individual generators
  std::string generate_readme() const;
  std::string generate_api_documentation() const;
  std::string generate_changelog() const;
  std::string generate_license() const;
  std::string generate_installation_guide() const;
  std::string generate_building_guide() const;
  std::string generate_testing_guide() const;
  std::string generate_performance_guide() const;
  std::string generate_security_guide() const;
  std::string generate_deployment_guide() const;

  // Write package to disk
  bool write_package_to_disk(const std::string& output_directory) const;

  // Create distribution archive
  bool create_distribution_archive(const std::string& output_path) const;

  // Version information
  static constexpr const char* VERSION = "6.0.0";
  static constexpr const char* CODENAME = "Production Launch";

private:
  std::string get_build_info() const;
  std::string get_feature_list() const;
  std::string get_api_summary() const;
};

} // namespace bendcore::testing
