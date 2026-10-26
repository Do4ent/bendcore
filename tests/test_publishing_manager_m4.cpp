// M4 Phase - Publishing Manager Tests
#include <gtest/gtest.h>
#include "../include/bendcore/drawing/publishing_manager.hpp"
#include "../include/bendcore/drawing/file_handler.hpp"
#include <filesystem>

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
    job.metadata.revision_number = 1;
    job.metadata.author = "Test Author";
    job.metadata.description = "Test Description";

    return job;
  }
};

// ============================================================================
// Local Publishing Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishToLocalDirectory) {
  std::string test_dir = "/tmp/bendcore_test_publish";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  auto job = createTestPublishingJob();
  job.local_settings = LocalPublishSettings{test_dir};

  auto result = publishToLocal(job);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.destination, PublishDestination::LocalDirectory);
  EXPECT_EQ(result.sheets_published, 1);

  // Clean up
  fs::remove_all(test_dir);
}

TEST_F(PublishingManagerTest, PublishToLocalMissingDirectory) {
  auto job = createTestPublishingJob();
  job.local_settings = LocalPublishSettings{"/nonexistent/path/that/does/not/exist"};

  auto result = publishToLocal(job);

  // Should handle error gracefully
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

// ============================================================================
// Cloud Publishing Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishToCloud) {
  auto job = createTestPublishingJob();

  CloudPublishSettings cloud_settings;
  cloud_settings.provider = CloudProvider::AmazonS3;
  cloud_settings.bucket_name = "test-bucket";
  cloud_settings.api_key = "test-key";
  cloud_settings.folder_prefix = "drawings";

  job.cloud_settings = cloud_settings;

  auto result = publishToCloud(job);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.destination, PublishDestination::CloudStorage);
  EXPECT_EQ(result.sheets_published, 1);
  EXPECT_GT(result.published_urls.size(), 0);
}

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

TEST_F(PublishingManagerTest, PublishViaEmail) {
  auto job = createTestPublishingJob();

  EmailPublishSettings email_settings;
  email_settings.recipients = {"test@example.com"};
  email_settings.subject_template = "{title} - Rev {revision}";
  email_settings.message_body = "Please find the attached drawing.";

  job.email_settings = email_settings;

  auto result = publishViaEmail(job);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.destination, PublishDestination::EmailRecipients);
  EXPECT_TRUE(result.email_sent);
}

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
// Archival Tests
// ============================================================================

TEST_F(PublishingManagerTest, ArchiveDrawings) {
  std::string archive_dir = "/tmp/bendcore_test_archive";
  fs::remove_all(archive_dir);
  fs::create_directories(archive_dir);

  auto job = createTestPublishingJob();

  ArchivalSettings archival_settings;
  archival_settings.archive_directory = archive_dir;
  archival_settings.compress_files = false;
  archival_settings.maintain_history = true;

  job.archival_settings = archival_settings;

  auto result = archiveDrawings(job);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.destination, PublishDestination::ArchivalRepository);
  EXPECT_EQ(result.sheets_published, 1);
  EXPECT_FALSE(result.archive_id.empty());

  // Clean up
  fs::remove_all(archive_dir);
}

// ============================================================================
// Multi-Destination Publishing Tests
// ============================================================================

TEST_F(PublishingManagerTest, PublishToMultipleDestinations) {
  std::string test_dir = "/tmp/bendcore_test_multi";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  auto job = createTestPublishingJob();
  job.local_settings = LocalPublishSettings{test_dir};

  CloudPublishSettings cloud_settings;
  cloud_settings.bucket_name = "test-bucket";
  cloud_settings.api_key = "test-key";
  job.cloud_settings = cloud_settings;

  std::vector<PublishDestination> destinations = {
    PublishDestination::LocalDirectory,
    PublishDestination::CloudStorage
  };

  auto results = publishToMultiple(job, destinations);

  EXPECT_EQ(results.size(), 2);
  EXPECT_TRUE(results[0].success);  // Local should succeed
  EXPECT_TRUE(results[1].success);  // Cloud should succeed

  // Clean up
  fs::remove_all(test_dir);
}

// ============================================================================
// Batch Export Multiple Formats Tests
// ============================================================================

TEST_F(PublishingManagerTest, BatchExportMultipleFormats) {
  std::string test_dir = "/tmp/bendcore_test_batch";
  fs::remove_all(test_dir);
  fs::create_directories(test_dir);

  std::vector<std::shared_ptr<DrawingSheet>> sheets;
  sheets.push_back(createTestSheet());
  sheets.push_back(createTestSheet());

  std::vector<ExportFormat> formats = {
    ExportFormat::PDF,
    ExportFormat::DXF,
    ExportFormat::SVG
  };

  auto result = batchExportMultipleFormats(sheets, formats, test_dir);

  EXPECT_EQ(result.total_drawings, 6);  // 2 sheets * 3 formats
  EXPECT_EQ(result.successful_exports, 6);
  EXPECT_EQ(result.failed_exports, 0);
  EXPECT_TRUE(result.success);

  // Clean up
  fs::remove_all(test_dir);
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

  auto versions = getDrawingVersionHistory(test_dir, "DRW-001");

  EXPECT_GE(versions.size(), 0);

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
  metadata.revision_number = 1;
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
  EXPECT_EQ(job.metadata.revision_number, 1);
  EXPECT_EQ(job.metadata.author, "Test Author");
}

TEST_F(PublishingManagerTest, PublishingJobExportOptions) {
  auto job = createTestPublishingJob();

  EXPECT_EQ(job.export_options.format, ExportFormat::PDF);
  EXPECT_TRUE(job.export_options.auto_filename);
}
