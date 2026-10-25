#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::deployment {

// Production Build Pipeline (CAD-CORE-041)
class ProductionBuild {
 public:
  enum class BuildType {
    DEBUG,
    RELEASE,
    RELEASE_WITH_DEBUG_INFO,
    DISTRIBUTION  // Optimized for distribution
  };

  enum class Platform {
    WINDOWS,
    LINUX,
    MACOS,
    WEB  // Emscripten
  };

  struct BuildConfig {
    BuildType build_type = BuildType::DISTRIBUTION;
    Platform target_platform = Platform::LINUX;
    bool enable_optimizations = true;
    bool enable_lto = true;  // Link Time Optimization
    bool strip_symbols = false;
    bool enable_pgo = false;  // Profile-Guided Optimization
    int optimization_level = 3;  // 0-3
    bool enable_ubsan = false;  // Undefined Behavior Sanitizer
    bool enable_asan = false;  // Address Sanitizer
  };

  struct BuildResult {
    bool success = true;
    std::string build_log;
    std::string error_message;
    double build_time_seconds = 0.0;
    long binary_size_bytes = 0;
    int warnings = 0;
    int errors = 0;
    std::vector<std::string> compiled_files;
    std::string output_path;
  };

  struct PerformanceMetrics {
    double startup_time_ms = 0.0;
    double memory_usage_mb = 0.0;
    double cpu_usage_percent = 0.0;
    int frame_rate_fps = 0;
    std::vector<std::string> bottlenecks;
  };

  ProductionBuild();

  // Execute production build
  BuildResult executeBuild(
    const BuildConfig& config,
    const std::string& source_dir,
    const std::string& build_dir
  );

  // Clean build artifacts
  bool cleanBuild(const std::string& build_dir);

  // Validate build output
  bool validateBuild(const BuildResult& result);

  // Generate build report
  std::string generateBuildReport(const BuildResult& result);

  // Measure performance after build
  PerformanceMetrics measurePerformance(const std::string& binary_path);

  // Run CMake configuration
  bool runCMake(
    const std::string& source_dir,
    const std::string& build_dir,
    const BuildConfig& config
  );

  // Run compilation
  BuildResult runCompilation(
    const std::string& build_dir,
    int num_jobs = 4
  );

  // Strip debug symbols
  bool stripDebugSymbols(const std::string& binary_path);

  // Create checksums for verification
  std::string createChecksum(const std::string& file_path);

  // Verify build integrity
  bool verifyBuildIntegrity(const std::vector<std::string>& files);

 private:
  // Helper functions
  std::string getPlatformString(Platform platform);
  std::string getBuildTypeString(BuildType build_type);

  // Execute shell command and capture output
  std::pair<int, std::string> executeCommand(
    const std::string& command,
    const std::string& working_dir = ""
  );

  // Parse compiler output for warnings/errors
  void parseCompilerOutput(
    const std::string& output,
    BuildResult& result
  );

  BuildConfig current_config_;
  PerformanceMetrics last_metrics_;
};

}  // namespace bendcore::deployment
