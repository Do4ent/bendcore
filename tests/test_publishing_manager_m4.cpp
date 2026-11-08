// M4 Phase - Publishing Manager Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/publishing_manager.hpp"
#include "../include/bendcore/drawing/file_handler.hpp"
#include <filesystem>
#include <chrono>
#include <ctime>
#include <fstream>

namespace fs = std::filesystem;

using namespace BendCore::Drawing;

class PublishingManagerTest : public ::testing::Test {
 protected:
  std::shared_ptr<DrawingSheet> createTestSheet() {
    auto sheet = std::make_shared<DrawingSheet>(1);
    auto view = std::make_shared<DrawingView>("view1", ViewType::Front);

    Edge2D edge;
    edge.p0 = {0, 0};
    edge.p1 = {100, 0};
    view->addEdge(edge);

    sheet->addView(view);

    TitleBlock title;
    title.fields["title"] = "Test Drawing";
    title.fields["part_number"] = "DRW-001";
    sheet->setTitleBlock(title);

    return sheet;
  }

  PublishingJob createTestPublishingJob() {
    PublishingJob job;
    job.job_id = "job_001";
    job.sheets.push_back(createTestSheet());
    job.export_options.format = ExportFormat::PDF;
    job.export_options.auto_filename = true;

    job.metadata.title = "Test Drawing";
    job.metadata.drawing_number = "DRW-001";
    job.metadata.revision_number = "1";
    job.metadata.author = "Test Author";
    job.metadata.description = "Test Description";

    return job;
  }
};

// ============================================================================
// Local Publishing Tests
// ============================================================================

// Export has no format writer until roadmap phase 4; publishing must surface
// that error rather than report published files.
TEST_F(PublishingManagerTest, PublishToLocalReportsExportError) {
  std::string test_dir = "/tmp/bendcore_test_publish";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  auto job = createTestPublishingJob();
  job.local_settings = LocalPublishSettings{test_dir};

  auto result = publishToLocal(job);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.sheets_published, 0);
  EXPECT_NE(result.error_message.find("not supported"), std::string::npos);

  fs::remove_all(test_dir);
}

TEST_F(PublishingManagerTest, PublishToLocalMissingDirectory) {
  auto job = createTestPublishingJob();
  // A directory below a regular file cannot be created, even as root.
  const auto blocker = fs::temp_directory_path() / "bendcore_publish_blocker";
  { std::ofstream(blocker.string()) << "x"; }
  job.local_settings = LocalPublishSettings{(blocker / "sub").string()};

  auto result = publishToLocal(job);
  fs::remove(blocker);

  // Should handle error gracefully
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

// ============================================================================
// Unsupported destinations: cloud storage and e-mail
// ============================================================================

TEST_F(PublishingManagerTest, PublishToMultipleReportsUnsupportedCloudAndEmail) {
  auto job = createTestPublishingJob();

  auto results = publishToMultiple(
      job, {PublishDestination::CloudStorage, PublishDestination::EmailRecipients});

  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].destination, PublishDestination::CloudStorage);
  EXPECT_EQ(results[1].destination, PublishDestination::EmailRecipients);
  for (const auto& r : results) {
    EXPECT_FALSE(r.success);
    EXPECT_EQ(r.job_id, "job_001");
    EXPECT_EQ(r.sheets_published, 0);
    EXPECT_TRUE(r.published_files.empty());
    EXPECT_NE(r.error_message.find("is not supported"), std::string::npos) << r.error_message;
  }
  EXPECT_NE(results[0].error_message.find("cloud"), std::string::npos);
  EXPECT_NE(results[1].error_message.find("e-mail"), std::string::npos);
}

TEST_F(PublishingManagerTest, ValidateRejectsCloudAndEmail) {
  auto job = createTestPublishingJob();
  for (auto dest : {PublishDestination::CloudStorage, PublishDestination::EmailRecipients}) {
    auto [valid, error] = validatePublishingDestination(dest, job);
    EXPECT_FALSE(valid);
    EXPECT_NE(error.find("is not supported"), std::string::npos) << error;
  }
}

// ============================================================================
// Versioning Tests
// ============================================================================

TEST_F(PublishingManagerTest, GenerateNextRevision) {
  std::string test_dir = "/tmp/bendcore_test_revision";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  auto revision = generateNextRevision(test_dir, "DRW-001");

  EXPECT_EQ(revision, "A");

  // Create a file with revision A
  std::ofstream f((fs::path(test_dir) / "DRW-001_revA.pdf").string());
  f.close();

  // Next revision should be B
  revision = generateNextRevision(test_dir, "DRW-001");
  EXPECT_EQ(revision, "B");

  // Clean up
  fs::remove_all(test_dir);
}

TEST_F(PublishingManagerTest, GetDrawingVersionHistory) {
  std::string test_dir = "/tmp/bendcore_test_history";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  // Create some version files
  std::ofstream f1((fs::path(test_dir) / "DRW-001_revA.pdf").string());
  f1.close();
  std::ofstream f2((fs::path(test_dir) / "DRW-001_revB.pdf").string());
  f2.close();

  // Unrelated drawing and non-revision files are ignored
  std::ofstream((fs::path(test_dir) / "DRW-002_revC.pdf").string()).close();
  std::ofstream((fs::path(test_dir) / "DRW-001_notes.txt").string()).close();

  auto versions = getDrawingVersionHistory(test_dir, "DRW-001");

  ASSERT_EQ(versions.size(), 2u);
  EXPECT_EQ(versions[0].revision_letter, "A");
  EXPECT_EQ(versions[1].revision_letter, "B");
  EXPECT_EQ(versions[1].version_number, 2);
  EXPECT_EQ(generateNextRevision(test_dir, "DRW-001"), "C");

  // Z rolls over to AA, AZ to BA
  std::ofstream((fs::path(test_dir) / "DRW-001_revZ.pdf").string()).close();
  EXPECT_EQ(generateNextRevision(test_dir, "DRW-001"), "AA");
  std::ofstream((fs::path(test_dir) / "DRW-001_revAZ.pdf").string()).close();
  EXPECT_EQ(generateNextRevision(test_dir, "DRW-001"), "BA");

  // Clean up
  fs::remove_all(test_dir);
}

// ============================================================================
// Document Numbering Tests
// ============================================================================

TEST_F(PublishingManagerTest, GenerateDrawingNumber) {
  std::string test_dir = "/tmp/bendcore_test_numbering";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  NumberingScheme scheme;
  scheme.prefix = "DRW";
  scheme.separator = "-";
  scheme.sequential_start = 1000;
  scheme.include_date = false;

  // Empty archive: the sequence starts at sequential_start.
  EXPECT_EQ(generateDrawingNumber(scheme, test_dir), "DRW-1000");

  // Existing numbers (with any suffix) advance the sequence; others are ignored.
  std::ofstream((fs::path(test_dir) / "DRW-1000_revA.pdf").string()).close();
  std::ofstream((fs::path(test_dir) / "DRW-1007.dxf").string()).close();
  std::ofstream((fs::path(test_dir) / "DRW-12.pdf").string()).close();
  std::ofstream((fs::path(test_dir) / "ASM-5000.pdf").string()).close();
  std::ofstream((fs::path(test_dir) / "DRW-notes.txt").string()).close();
  EXPECT_EQ(generateDrawingNumber(scheme, test_dir), "DRW-1008");

  // Missing archive directory: nothing to scan.
  EXPECT_EQ(generateDrawingNumber(scheme, test_dir + "/missing"), "DRW-1000");

  // Clean up
  fs::remove_all(test_dir);
}

TEST_F(PublishingManagerTest, GenerateDrawingNumberWithDate) {
  std::string test_dir = "/tmp/bendcore_test_numbering_date";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  NumberingScheme scheme;
  scheme.prefix = "DRW";
  scheme.separator = "-";
  scheme.sequential_start = 1000;
  scheme.include_date = true;
  scheme.date_format = "YYYYMMDD";

  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  localtime_r(&now, &tm);
  char expected[32];
  std::strftime(expected, sizeof expected, "DRW-1000-%Y%m%d", &tm);
  EXPECT_EQ(generateDrawingNumber(scheme, test_dir), expected);

  scheme.date_format = "YY.MM";
  std::strftime(expected, sizeof expected, "DRW-1000-%y.%m", &tm);
  EXPECT_EQ(generateDrawingNumber(scheme, test_dir), expected);

  // Clean up
  fs::remove_all(test_dir);
}

// ============================================================================
// Document Digest Tests
// ============================================================================

TEST_F(PublishingManagerTest, CreateDocumentDigest) {
  auto sheet = createTestSheet();

  PublishingMetadata metadata;
  metadata.title = "Test Drawing";
  metadata.drawing_number = "DRW-001";
  metadata.revision_number = "1";
  metadata.author = "Test Author";

  auto digest = createDocumentDigest(*sheet, metadata);

  EXPECT_TRUE(digest.find("Drawing:") != std::string::npos);
  EXPECT_TRUE(digest.find("Test Drawing") != std::string::npos);
  EXPECT_TRUE(digest.find("DRW-001") != std::string::npos);
  EXPECT_TRUE(digest.find("Test Author") != std::string::npos);
}

// ============================================================================
// Publishing Destination Validation Tests
// ============================================================================

TEST_F(PublishingManagerTest, ValidatePublishingDestinationLocal) {
  std::string test_dir = "/tmp/bendcore_test_validate";
  fs::create_directories(test_dir);

  auto job = createTestPublishingJob();
  job.local_settings = LocalPublishSettings{test_dir};

  auto [valid, error] = validatePublishingDestination(
    PublishDestination::LocalDirectory, job);

  EXPECT_TRUE(valid);

  // Clean up
  fs::remove_all(test_dir);
}

// ============================================================================
// Publishing Job Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishingJobMetadata) {
  auto job = createTestPublishingJob();

  EXPECT_EQ(job.metadata.title, "Test Drawing");
  EXPECT_EQ(job.metadata.drawing_number, "DRW-001");
  EXPECT_EQ(job.metadata.revision_number, "1");
  EXPECT_EQ(job.metadata.author, "Test Author");
}

TEST_F(PublishingManagerTest, PublishingJobExportOptions) {
  auto job = createTestPublishingJob();

  EXPECT_EQ(job.export_options.format, ExportFormat::PDF);
  EXPECT_TRUE(job.export_options.auto_filename);
}
