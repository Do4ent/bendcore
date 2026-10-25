#pragma once

#include <string>
#include <vector>
#include <map>

namespace bendcore::deployment {

// Installer Generation (CAD-CORE-042)
class Installers {
 public:
  enum class InstallerType {
    WINDOWS_MSI,     // Microsoft Installer (.msi)
    WINDOWS_EXE,     // Executable installer
    MACOS_DMG,       // Disk Image
    MACOS_PKG,       // Package installer
    LINUX_DEB,       // Debian package
    LINUX_RPM,       // Red Hat package
    LINUX_APPIMAGE,  // AppImage format
    PORTABLE_ZIP     // Portable ZIP archive
  };

  struct InstallerConfig {
    std::string app_name = "BendCore";
    std::string version = "9.0.0";
    std::string author = "BendCore Team";
    std::string license = "Commercial";
    std::string homepage = "https://bendcore.com";
    std::string install_dir = "C:\\Program Files\\BendCore";  // Windows default
    bool create_desktop_shortcut = true;
    bool create_start_menu = true;
    bool associate_file_types = true;
    std::vector<std::string> file_types = {".bendcore", ".bnd"};
  };

  struct InstallerResult {
    bool success = true;
    std::string installer_path;
    std::string file_hash;  // SHA256
    long file_size_bytes = 0;
    double compression_ratio = 0.0;
    std::string error_message;
  };

  Installers();

  // Generate complete installer suite for all platforms
  std::vector<InstallerResult> generateInstallerSuite(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir
  );

  // Generate single installer
  InstallerResult generateInstaller(
    const std::string& binary_path,
    InstallerType type,
    const InstallerConfig& config,
    const std::string& output_dir
  );

  // Windows MSI/EXE generation
  InstallerResult generateWindowsInstaller(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir,
    bool use_msi = true
  );

  // macOS DMG/PKG generation
  InstallerResult generateMacOSInstaller(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir,
    bool use_dmg = true
  );

  // Linux DEB/RPM generation
  InstallerResult generateLinuxInstaller(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir,
    bool use_deb = true
  );

  // Generate portable ZIP
  InstallerResult generatePortableZip(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir
  );

  // Generate AppImage for Linux
  InstallerResult generateAppImage(
    const std::string& binary_path,
    const InstallerConfig& config,
    const std::string& output_dir
  );

  // Sign installer (for macOS/Windows code signing)
  bool signInstaller(
    const std::string& installer_path,
    const std::string& certificate_path,
    const std::string& certificate_password = ""
  );

  // Notarize macOS installer (Apple requirements)
  bool notarizeMacOS(
    const std::string& installer_path,
    const std::string& apple_id,
    const std::string& apple_password
  );

  // Create checksum file
  std::string createChecksumFile(
    const std::vector<InstallerResult>& results,
    const std::string& output_path
  );

  // Verify installer integrity
  bool verifyInstaller(const InstallerResult& result);

  // Generate installer metadata
  std::string generateMetadata(
    const std::vector<InstallerResult>& results,
    const InstallerConfig& config
  );

 private:
  // Platform-specific helpers
  std::string getInstallerExtension(InstallerType type);
  std::string getInstallerName(
    const InstallerConfig& config,
    InstallerType type
  );

  // File operations
  bool copyBinary(
    const std::string& src,
    const std::string& dest
  );

  bool createInstallerPackage(
    const std::string& source_dir,
    const std::string& output_path,
    InstallerType type
  );

  // Execute installer creation commands
  std::pair<bool, std::string> executeInstallerCommand(
    const std::string& command
  );

  InstallerConfig current_config_;
};

}  // namespace bendcore::deployment
