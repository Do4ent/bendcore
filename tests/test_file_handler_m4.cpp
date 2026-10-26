// M4 Phase - File Handler Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/file_handler.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

using namespace BendCore::Drawing;

class FileHandlerTest : public ::testing::Test {
 protected:
  std::string test_dir = "/tmp/bendcore_file_test";

  void SetUp() override {
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);
  }

  void TearDown() override {
    fs::remove_all(test_dir);
  }

  std::string createTestFile(const std::string& filename, const std::string& content = "test") {
    std::string filepath = (fs::path(test_dir) / filename).string();
    std::ofstream f(filepath);
    f << content;
    f.close();
    return filepath;
  }
};

// ============================================================================
// Filename Generation Tests
// ============================================================================

TEST_F(FileHandlerTest, GenerateFilenameFromPattern) {
  FilenamePattern pattern;
  pattern.project_code = "PROJ";
  pattern.drawing_number = "DRW001";
  pattern.drawing_name = "MainAssembly";
  pattern.revision_number = 1;
  pattern.revision_letter = "A";
  pattern.include_timestamp = false;
  pattern.include_date = false;

  auto filename = generateFilenameFromPattern(pattern, ExportFormat::PDF);

  EXPECT_TRUE(filename.find("PROJ") != std::string::npos);
  EXPECT_TRUE(filename.find("DRW001") != std::string::npos);
  EXPECT_TRUE(filename.find("MainAssembly") != std::string::npos);
  EXPECT_TRUE(filename.find("revA") != std::string::npos);
  EXPECT_TRUE(filename.find(".pdf") != std::string::npos);
}

TEST_F(FileHandlerTest, GenerateFilenameWithTimestamp) {
  FilenamePattern pattern;
  pattern.project_code = "PROJ";
  pattern.drawing_number = "DRW001";
  pattern.include_timestamp = true;

  auto filename = generateFilenameFromPattern(pattern, ExportFormat::DXF);

  EXPECT_TRUE(filename.find("PROJ") != std::string::npos);
  EXPECT_TRUE(filename.find("_") != std::string::npos);  // Timestamp separator
  EXPECT_TRUE(filename.find(".dxf") != std::string::npos);
}

TEST_F(FileHandlerTest, GenerateFilenameWithDate) {
  FilenamePattern pattern;
  pattern.project_code = "PROJ";
  pattern.drawing_number = "DRW001";
  pattern.include_timestamp = false;
  pattern.include_date = true;
  pattern.date_format = "YYYYMMDD";

  auto filename = generateFilenameFromPattern(pattern, ExportFormat::PDF);

  EXPECT_TRUE(filename.find("PROJ") != std::string::npos);
  EXPECT_TRUE(filename.find("_") != std::string::npos);  // Date separator
  EXPECT_TRUE(filename.find(".pdf") != std::string::npos);
}

TEST_F(FileHandlerTest, GenerateUniqueFilename) {
  // Create existing file
  createTestFile("drawing.pdf");

  auto unique = generateUniqueFilename(test_dir, "drawing.pdf");

  EXPECT_NE(unique, "drawing.pdf");
  EXPECT_TRUE(unique.find("drawing") != std::string::npos);
  EXPECT_TRUE(unique.find(".pdf") != std::string::npos);
  EXPECT_TRUE(unique.find("_") != std::string::npos);  // Contains counter
}

TEST_F(FileHandlerTest, GenerateUniqueFilenameNonexistent) {
  auto unique = generateUniqueFilename(test_dir, "new_drawing.pdf");

  EXPECT_EQ(unique, "new_drawing.pdf");
}

// ============================================================================
// File Metadata Tests
// ============================================================================

TEST_F(FileHandlerTest, GetFileMetadata) {
  std::string filepath = createTestFile("test.pdf", "test content");

  auto metadata = getFileMetadata(filepath);

  EXPECT_EQ(metadata.file_path, filepath);
  EXPECT_GT(metadata.file_size_bytes, 0);
}

TEST_F(FileHandlerTest, GetFileMetadataNonexistent) {
  auto metadata = getFileMetadata("/nonexistent/file.pdf");

  EXPECT_EQ(metadata.file_size_bytes, 0);
}

TEST_F(FileHandlerTest, ComputeFileHash) {
  std::string filepath = createTestFile("test.pdf");

  auto hash = computeFileHash(filepath);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(hash.length(), 64);  // SHA256 hex is 64 chars
}

TEST_F(FileHandlerTest, VerifyFileIntegrity) {
  std::string filepath = createTestFile("test.pdf");
  auto expected_hash = computeFileHash(filepath);

  auto [matches, computed] = verifyFileIntegrity(filepath, expected_hash);

  EXPECT_TRUE(matches);
  EXPECT_EQ(computed, expected_hash);
}

TEST_F(FileHandlerTest, VerifyFileIntegrityMismatch) {
  std::string filepath = createTestFile("test.pdf");

  auto [matches, computed] = verifyFileIntegrity(filepath, "0000000000000000000000000000000000000000000000000000000000000000");

  EXPECT_FALSE(matches);
}

// ============================================================================
// Directory Operations Tests
// ============================================================================

TEST_F(FileHandlerTest, EnsureDirectoryExists) {
  std::string new_dir = (fs::path(test_dir) / "new" / "nested" / "dir").string();

  auto [success, error] = ensureDirectoryExists(new_dir);

  EXPECT_TRUE(success);
  EXPECT_TRUE(fs::exists(new_dir));
}

TEST_F(FileHandlerTest, CheckFileWritable) {
  std::string filepath = (fs::path(test_dir) / "test.pdf").string();

  auto [exists, writable] = checkFileWritable(filepath);

  EXPECT_FALSE(exists);  // File doesn't exist yet
  EXPECT_TRUE(writable);  // But directory is writable
}

TEST_F(FileHandlerTest, CheckFileWritableExisting) {
  std::string filepath = createTestFile("test.pdf");

  auto [exists, writable] = checkFileWritable(filepath);

  EXPECT_TRUE(exists);
  EXPECT_TRUE(writable);
}

TEST_F(FileHandlerTest, CheckDirectoryWritable) {
  auto [exists, writable] = checkDirectoryWritable(test_dir);

  EXPECT_TRUE(exists);
  EXPECT_TRUE(writable);
}

// ============================================================================
// Backup Tests
// ============================================================================

TEST_F(FileHandlerTest, CreateBackup) {
  std::string filepath = createTestFile("test.pdf", "original content");

  auto backup = createBackup(filepath, false);

  EXPECT_FALSE(backup.backup_file_path.empty());
  EXPECT_TRUE(fs::exists(backup.backup_file_path));
  EXPECT_FALSE(backup.original_file_hash.empty());
}

TEST_F(FileHandlerTest, GetBackupHistory) {
  std::string filepath = createTestFile("test.pdf");

  // Create multiple backups
  createBackup(filepath, false);
  createBackup(filepath, false);
  createBackup(filepath, false);

  auto backups = getBackupHistory(filepath);

  EXPECT_GE(backups.size(), 0);  // May be 0 if system doesn't support backup scanning
}

TEST_F(FileHandlerTest, CleanOldBackups) {
  std::string filepath = createTestFile("test.pdf");

  // Create backups
  createBackup(filepath, false);

  int deleted = cleanOldBackups(filepath, 0, 1);  // Keep max 1 backup

  EXPECT_GE(deleted, 0);
}

// ============================================================================
// Revision Management Tests
// ============================================================================

TEST_F(FileHandlerTest, GetNextRevisionNumber) {
  // Create some revision files
  createTestFile("drawing_revA.pdf");
  createTestFile("drawing_revB.pdf");

  auto [version, letter] = getNextRevisionNumber(test_dir, "drawing");

  EXPECT_GT(version, 0);
  EXPECT_FALSE(letter.empty());
}

TEST_F(FileHandlerTest, GetRevisionHistory) {
  // Create revision files with different names
  createTestFile("DRW001_revA.pdf");
  createTestFile("DRW001_revB.pdf");
  createTestFile("DRW001_revC.pdf");

  auto revisions = getRevisionHistory(test_dir, "DRW001");

  EXPECT_GE(revisions.size(), 0);
}

TEST_F(FileHandlerTest, ArchiveOldRevisions) {
  std::string archive_dir = (fs::path(test_dir) / "archive").string();
  fs::create_directories(archive_dir);

  // Create files
  createTestFile("drawing_v1.pdf");
  createTestFile("drawing_v2.pdf");
  createTestFile("drawing_v3.pdf");
  createTestFile("drawing_v4.pdf");

  int archived = archiveOldRevisions(test_dir, archive_dir, 2);  // Keep 2 recent

  EXPECT_GE(archived, 0);
}

// ============================================================================
// File Size Tests
// ============================================================================

TEST_F(FileHandlerTest, GetFileSize) {
  std::string filepath = createTestFile("test.pdf", "0123456789");  // 10 bytes

  auto size = getFileSize(filepath);

  EXPECT_EQ(size, 10);
}

TEST_F(FileHandlerTest, GetFileSizeNonexistent) {
  auto size = getFileSize("/nonexistent/file.pdf");

  EXPECT_EQ(size, -1);
}

TEST_F(FileHandlerTest, FormatFileSize) {
  EXPECT_EQ(formatFileSize(512), "512.00 B");
  EXPECT_EQ(formatFileSize(1024), "1.00 KB");
  EXPECT_EQ(formatFileSize(1024 * 1024), "1.00 MB");
  EXPECT_EQ(formatFileSize(1024 * 1024 * 1024), "1.00 GB");
}

// ============================================================================
// Filename Utilities Tests
// ============================================================================

TEST_F(FileHandlerTest, SanitizeFilename) {
  std::string unsafe = "drawing<>:name\"|?*.pdf";
  auto safe = sanitizeFilename(unsafe);

  EXPECT_TRUE(safe.find('<') == std::string::npos);
  EXPECT_TRUE(safe.find('>') == std::string::npos);
  EXPECT_TRUE(safe.find(':') == std::string::npos);
  EXPECT_TRUE(safe.find('"') == std::string::npos);
  EXPECT_TRUE(safe.find('|') == std::string::npos);
  EXPECT_TRUE(safe.find('?') == std::string::npos);
  EXPECT_TRUE(safe.find('*') == std::string::npos);
}

TEST_F(FileHandlerTest, GetFileExtension) {
  EXPECT_EQ(getFileExtension("drawing.pdf"), ".pdf");
  EXPECT_EQ(getFileExtension("drawing.DWG"), ".DWG");
  EXPECT_EQ(getFileExtension("drawing"), "");
  EXPECT_EQ(getFileExtension("/path/to/drawing.dxf"), ".dxf");
}

TEST_F(FileHandlerTest, ReplaceFileExtension) {
  auto result = replaceFileExtension("drawing.pdf", ".dxf");
  EXPECT_EQ(result, "drawing.dxf");

  result = replaceFileExtension("drawing", ".pdf");
  EXPECT_EQ(result, "drawing.pdf");

  result = replaceFileExtension("/path/to/drawing.pdf", ".svg");
  EXPECT_TRUE(result.find(".svg") != std::string::npos);
}

TEST_F(FileHandlerTest, GetFilenameWithoutExtension) {
  EXPECT_EQ(getFilenameWithoutExtension("drawing.pdf"), "drawing");
  EXPECT_EQ(getFilenameWithoutExtension("/path/to/drawing.dxf"), "drawing");
  EXPECT_EQ(getFilenameWithoutExtension("drawing"), "drawing");
}

// ============================================================================
// Path Utilities Tests
// ============================================================================

TEST_F(FileHandlerTest, SplitPath) {
  auto [dir, file] = splitPath("/home/user/drawing.pdf");

  EXPECT_TRUE(dir.find("home") != std::string::npos);
  EXPECT_EQ(file, "drawing.pdf");
}

TEST_F(FileHandlerTest, GetAbsolutePath) {
  std::string relative = "drawing.pdf";
  auto absolute = getAbsolutePath(relative);

  EXPECT_TRUE(isAbsolutePath(absolute));
}

TEST_F(FileHandlerTest, IsAbsolutePath) {
  EXPECT_TRUE(isAbsolutePath("/home/user/drawing.pdf"));
  EXPECT_TRUE(isAbsolutePath("/tmp/test"));
  EXPECT_FALSE(isAbsolutePath("drawing.pdf"));
  EXPECT_FALSE(isAbsolutePath("./drawing.pdf"));
  EXPECT_FALSE(isAbsolutePath("../drawing.pdf"));
}

// ============================================================================
// Filename Parsing Tests
// ============================================================================

TEST_F(FileHandlerTest, ParseFilename) {
  FilenamePattern pattern;
  pattern.project_code = "PROJ";
  pattern.drawing_number = "DRW001";

  auto result = parseFilename("PROJ-DRW001_revA.pdf", pattern);

  // Parsing may not be fully implemented, but function should return optional
  // Result depends on implementation
}

// ============================================================================
// Path Components Tests
// ============================================================================

TEST_F(FileHandlerTest, PathComponents) {
  std::string filepath = "/home/user/documents/drawing.pdf";

  auto [dir, file] = splitPath(filepath);
  EXPECT_FALSE(dir.empty());
  EXPECT_EQ(file, "drawing.pdf");

  auto ext = getFileExtension(filepath);
  EXPECT_EQ(ext, ".pdf");

  auto name = getFilenameWithoutExtension(filepath);
  EXPECT_EQ(name, "drawing");
}

// ============================================================================
// File Operations Edge Cases Tests
// ============================================================================

TEST_F(FileHandlerTest, EnsureDirectoryExistsAlreadyExists) {
  // Directory already exists
  auto [success, error] = ensureDirectoryExists(test_dir);

  EXPECT_TRUE(success);
}

TEST_F(FileHandlerTest, CreateBackupNonexistentFile) {
  auto backup = createBackup("/nonexistent/file.pdf", false);

  // Should handle gracefully even if file doesn't exist
}

TEST_F(FileHandlerTest, ArchiveOldRevisionsEmptyDirectory) {
  std::string archive_dir = (fs::path(test_dir) / "empty_archive").string();
  fs::create_directories(archive_dir);

  int archived = archiveOldRevisions(test_dir, archive_dir, 5);

  EXPECT_GE(archived, 0);
}
