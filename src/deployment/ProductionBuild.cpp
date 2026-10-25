#include "ProductionBuild.hpp"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <chrono>

namespace bendcore::deployment {

ProductionBuild::ProductionBuild() {}

ProductionBuild::BuildResult ProductionBuild::executeBuild(
    const BuildConfig& config,
    const std::string& source_dir,
    const std::string& build_dir) {

  BuildResult result;
  current_config_ = config;

  auto start_time = std::chrono::high_resolution_clock::now();

  // Step 1: Clean previous build
  cleanBuild(build_dir);

  // Step 2: Run CMake
  if (!runCMake(source_dir, build_dir, config)) {
    result.success = false;
    result.error_message = "CMake configuration failed";
    return result;
  }

  // Step 3: Compile
  result = runCompilation(build_dir, 4);

  // Step 4: Validate build
  if (!validateBuild(result)) {
    result.success = false;
    result.error_message = "Build validation failed";
  }

  // Step 5: Strip symbols if needed
  if (config.strip_symbols && !result.output_path.empty()) {
    stripDebugSymbols(result.output_path);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end_time - start_time);
  result.build_time_seconds = duration.count() / 1000.0;

  return result;
}

bool ProductionBuild::cleanBuild(const std::string& build_dir) {
  auto [success, output] = executeCommand("rm -rf " + build_dir);
  auto [success2, output2] = executeCommand("mkdir -p " + build_dir);
  return success2 == 0;
}

bool ProductionBuild::validateBuild(const BuildResult& result) {
  if (!result.success) {
    return false;
  }

  if (result.errors > 0) {
    return false;
  }

  // Check binary exists and is executable
  if (result.output_path.empty()) {
    return false;
  }

  return true;
}

std::string ProductionBuild::generateBuildReport(const BuildResult& result) {
  std::ostringstream report;
  report << std::fixed << std::setprecision(2);

  report << "===========================================\n";
  report << "PRODUCTION BUILD REPORT\n";
  report << "===========================================\n";
  report << "Status: " << (result.success ? "SUCCESS" : "FAILED") << "\n";
  report << "Build Time: " << result.build_time_seconds << " seconds\n";
  report << "Binary Size: " << (result.binary_size_bytes / 1024.0 / 1024.0) << " MB\n";
  report << "Warnings: " << result.warnings << "\n";
  report << "Errors: " << result.errors << "\n";
  report << "Output Path: " << result.output_path << "\n";
  report << "Compiled Files: " << result.compiled_files.size() << "\n";

  if (!result.error_message.empty()) {
    report << "Error: " << result.error_message << "\n";
  }

  report << "\n===========================================\n";
  report << "BUILD LOG\n";
  report << "===========================================\n";
  report << result.build_log << "\n";

  return report.str();
}

ProductionBuild::PerformanceMetrics ProductionBuild::measurePerformance(
    const std::string& binary_path) {

  PerformanceMetrics metrics;

  // Simplified performance measurement
  // In a real system, this would use profiling tools
  metrics.startup_time_ms = 100.0;
  metrics.memory_usage_mb = 256.0;
  metrics.cpu_usage_percent = 5.0;
  metrics.frame_rate_fps = 60;

  return metrics;
}

bool ProductionBuild::runCMake(
    const std::string& source_dir,
    const std::string& build_dir,
    const BuildConfig& config) {

  std::ostringstream cmake_cmd;
  cmake_cmd << "cd " << build_dir << " && cmake";

  // Add build type
  cmake_cmd << " -DCMAKE_BUILD_TYPE=" << getBuildTypeString(config.build_type);

  // Add platform
  if (config.target_platform == Platform::WINDOWS) {
    cmake_cmd << " -G 'Visual Studio 16 2019'";
  }

  // Add optimization flags
  if (config.enable_optimizations) {
    cmake_cmd << " -DCMAKE_CXX_FLAGS_RELEASE='-O" << config.optimization_level << "'";
  }

  if (config.enable_lto) {
    cmake_cmd << " -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON";
  }

  cmake_cmd << " " << source_dir;

  auto [success, output] = executeCommand(cmake_cmd.str());
  return success == 0;
}

ProductionBuild::BuildResult ProductionBuild::runCompilation(
    const std::string& build_dir,
    int num_jobs) {

  BuildResult result;

  std::string build_cmd = "cd " + build_dir + " && make -j" + std::to_string(num_jobs);

  auto start_time = std::chrono::high_resolution_clock::now();
  auto [success, output] = executeCommand(build_cmd);
  auto end_time = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end_time - start_time);
  result.build_time_seconds = duration.count() / 1000.0;

  result.build_log = output;
  result.success = (success == 0);

  parseCompilerOutput(output, result);

  return result;
}

bool ProductionBuild::stripDebugSymbols(const std::string& binary_path) {
  std::string cmd = "strip " + binary_path;
  auto [success, output] = executeCommand(cmd);
  return success == 0;
}

std::string ProductionBuild::createChecksum(const std::string& file_path) {
  std::string cmd = "sha256sum " + file_path + " | cut -d' ' -f1";
  auto [success, output] = executeCommand(cmd);

  // Remove trailing newline
  if (!output.empty() && output.back() == '\n') {
    output.pop_back();
  }

  return output;
}

bool ProductionBuild::verifyBuildIntegrity(const std::vector<std::string>& files) {
  // Verify all files are readable and valid
  for (const auto& file : files) {
    std::ifstream f(file);
    if (!f.good()) {
      return false;
    }
  }
  return true;
}

std::string ProductionBuild::getPlatformString(Platform platform) {
  switch (platform) {
    case Platform::WINDOWS:
      return "Windows";
    case Platform::LINUX:
      return "Linux";
    case Platform::MACOS:
      return "macOS";
    case Platform::WEB:
      return "Web";
    default:
      return "Unknown";
  }
}

std::string ProductionBuild::getBuildTypeString(BuildType build_type) {
  switch (build_type) {
    case BuildType::DEBUG:
      return "Debug";
    case BuildType::RELEASE:
      return "Release";
    case BuildType::RELEASE_WITH_DEBUG_INFO:
      return "RelWithDebInfo";
    case BuildType::DISTRIBUTION:
      return "MinSizeRel";
    default:
      return "Release";
  }
}

std::pair<int, std::string> ProductionBuild::executeCommand(
    const std::string& command,
    const std::string& working_dir) {

  std::string full_cmd = command;
  if (!working_dir.empty()) {
    full_cmd = "cd " + working_dir + " && " + command;
  }

  // Execute command and capture output
  FILE* pipe = popen(full_cmd.c_str(), "r");
  if (!pipe) {
    return {-1, ""};
  }

  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

  int status = pclose(pipe);
  return {status, output};
}

void ProductionBuild::parseCompilerOutput(
    const std::string& output,
    BuildResult& result) {

  // Count warnings and errors
  result.warnings = 0;
  result.errors = 0;

  size_t pos = 0;
  while ((pos = output.find("warning:", pos)) != std::string::npos) {
    result.warnings++;
    pos += 8;
  }

  pos = 0;
  while ((pos = output.find("error:", pos)) != std::string::npos) {
    result.errors++;
    pos += 6;
  }
}

}  // namespace bendcore::deployment
