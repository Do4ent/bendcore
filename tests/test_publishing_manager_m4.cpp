// M4 Phase - Publishing Manager Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/publishing_manager.hpp"
#include "../include/bendcore/drawing/file_handler.hpp"
#include <filesystem>
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
// Cloud Publishing Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishToCloudMissingCredentials) {
  auto job = createTestPublishingJob();

  CloudPublishSettings cloud_settings;
  cloud_settings.provider = CloudProvider::AmazonS3;
  cloud_settings.bucket_name = "";  // Missing
  cloud_settings.api_key = "";      // Missing

  job.cloud_settings = cloud_settings;

  auto result = publishToCloud(job);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

// ============================================================================
// Email Publishing Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishViaEmailNoRecipients) {
  auto job = createTestPublishingJob();

  EmailPublishSettings email_settings;
  // No recipients

  job.email_settings = email_settings;

  auto result = publishViaEmail(job);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
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

  auto number = generateDrawingNumber(scheme, test_dir);

  EXPECT_TRUE(number.find("DRW") != std::string::npos);
  EXPECT_TRUE(number.find("-") != std::string::npos);
  EXPECT_TRUE(number.find("1000") != std::string::npos);

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

  auto number = generateDrawingNumber(scheme, test_dir);

  EXPECT_TRUE(number.find("DRW") != std::string::npos);
  EXPECT_TRUE(number.find("-") != std::string::npos);

  // Clean up
  fs::remove_all(test_dir);
}

// ============================================================================
// Metadata Embedding Tests
// ============================================================================

TEST_F(PublishingManagerTest, EmbedMetadata) {
  std::string test_file = "/tmp/test_metadata.pdf";
  FILE* f = fopen(test_file.c_str(), "w");
  if (f) {
    fprintf(f, "test");
    fclose(f);
  }

  PublishingMetadata metadata;
  metadata.title = "Test";
  metadata.drawing_number = "DRW-001";
  metadata.author = "Test Author";

  auto [success, error] = embedMetadata(test_file, metadata, ExportFormat::PDF);

  EXPECT_TRUE(success);

  // Clean up
  std::remove(test_file.c_str());
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

TEST_F(PublishingManagerTest, ValidatePublishingDestinationCloud) {
  auto job = createTestPublishingJob();

  CloudPublishSettings cloud_settings;
  cloud_settings.bucket_name = "test-bucket";
  cloud_settings.api_key = "test-key";

  job.cloud_settings = cloud_settings;

  auto [valid, error] = validatePublishingDestination(
    PublishDestination::CloudStorage, job);

  EXPECT_TRUE(valid);
}

TEST_F(PublishingManagerTest, ValidatePublishingDestinationEmail) {
  auto job = createTestPublishingJob();

  EmailPublishSettings email_settings;
  email_settings.recipients = {"test@example.com"};

  job.email_settings = email_settings;

  auto [valid, error] = validatePublishingDestination(
    PublishDestination::EmailRecipients, job);

  EXPECT_TRUE(valid);
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
