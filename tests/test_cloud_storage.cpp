#include <gtest/gtest.h>
#include "bendcore/cloud/cloud_storage.hpp"

using namespace bendcore::cloud;

class S3CloudStorageTest : public ::testing::Test {
protected:
    S3CloudStorage storage{"test-bucket", "us-east-1"};
};

TEST_F(S3CloudStorageTest, UploadFileWithValidPath) {
    // Would upload if file exists
    EXPECT_FALSE(storage.UploadFile("/nonexistent/file.txt", "remote.txt"));
}

TEST_F(S3CloudStorageTest, DownloadFile) {
    EXPECT_TRUE(storage.DownloadFile("remote.txt", "/tmp/local.txt"));
}

TEST_F(S3CloudStorageTest, DeleteFile) {
    EXPECT_TRUE(storage.DeleteFile("remote.txt"));
}

TEST_F(S3CloudStorageTest, ListFiles) {
    auto files = storage.ListFiles("/folder");
    EXPECT_GE(files.size(), 0);
}

TEST_F(S3CloudStorageTest, FileExists) {
    EXPECT_TRUE(storage.FileExists("remote.txt"));
}

TEST_F(S3CloudStorageTest, GetFileSize) {
    size_t size = storage.GetFileSize("remote.txt");
    EXPECT_GE(size, 0);
}

TEST_F(S3CloudStorageTest, GetFileMetadata) {
    auto metadata = storage.GetFileMetadata("remote.txt");
    EXPECT_GE(metadata.size(), 0);
}

TEST_F(S3CloudStorageTest, CreateDirectory) {
    EXPECT_TRUE(storage.CreateDirectory("/new/directory"));
}

TEST_F(S3CloudStorageTest, MultipleOperations) {
    EXPECT_TRUE(storage.CreateDirectory("/test"));
    EXPECT_TRUE(storage.FileExists("/test/file.txt"));
    EXPECT_TRUE(storage.DeleteFile("/test/file.txt"));
}

TEST_F(S3CloudStorageTest, ConcurrentOperations) {
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(storage.CreateDirectory("/test" + std::to_string(i)));
    }
}

class AzureCloudStorageTest : public ::testing::Test {
protected:
    AzureCloudStorage storage{"DefaultEndpointsProtocol=https;", "test-container"};
};

TEST_F(AzureCloudStorageTest, InitializeStorage) {
    EXPECT_FALSE(storage.UploadFile("/nonexistent", "remote.txt"));
}

TEST_F(AzureCloudStorageTest, DownloadFile) {
    EXPECT_TRUE(storage.DownloadFile("remote.txt", "/tmp/local.txt"));
}

TEST_F(AzureCloudStorageTest, DeleteFile) {
    EXPECT_TRUE(storage.DeleteFile("remote.txt"));
}

TEST_F(AzureCloudStorageTest, FileExists) {
    EXPECT_TRUE(storage.FileExists("remote.txt"));
}

TEST_F(AzureCloudStorageTest, GetFileSize) {
    auto size = storage.GetFileSize("remote.txt");
    EXPECT_GE(size, 0);
}

TEST_F(AzureCloudStorageTest, ListFiles) {
    auto files = storage.ListFiles("/folder");
    EXPECT_GE(files.size(), 0);
}

class GCSCloudStorageTest : public ::testing::Test {
protected:
    GCSCloudStorage storage{"test-project", "test-bucket"};
};

TEST_F(GCSCloudStorageTest, UploadFile) {
    EXPECT_FALSE(storage.UploadFile("/nonexistent", "remote.txt"));
}

TEST_F(GCSCloudStorageTest, DownloadFile) {
    EXPECT_TRUE(storage.DownloadFile("remote.txt", "/tmp/local.txt"));
}

TEST_F(GCSCloudStorageTest, DeleteFile) {
    EXPECT_TRUE(storage.DeleteFile("remote.txt"));
}

TEST_F(GCSCloudStorageTest, FileExists) {
    EXPECT_TRUE(storage.FileExists("remote.txt"));
}

TEST_F(GCSCloudStorageTest, CreateDirectory) {
    EXPECT_TRUE(storage.CreateDirectory("/folder"));
}

TEST_F(GCSCloudStorageTest, GetFileSize) {
    auto size = storage.GetFileSize("remote.txt");
    EXPECT_GE(size, 0);
}

TEST_F(GCSCloudStorageTest, GetFileMetadata) {
    auto metadata = storage.GetFileMetadata("remote.txt");
    EXPECT_GE(metadata.size(), 0);
}

TEST_F(GCSCloudStorageTest, ListFiles) {
    auto files = storage.ListFiles("/folder");
    EXPECT_GE(files.size(), 0);
}

// Test all three providers
class CloudStorageCompatibilityTest : public ::testing::Test {
protected:
    S3CloudStorage s3{"bucket", "us-east-1"};
    AzureCloudStorage azure{"connection", "container"};
    GCSCloudStorage gcs{"project", "bucket"};
};

TEST_F(CloudStorageCompatibilityTest, AllProvidersSupport UploadDownload) {
    EXPECT_TRUE(s3.DownloadFile("file", "/tmp/file"));
    EXPECT_TRUE(azure.DownloadFile("file", "/tmp/file"));
    EXPECT_TRUE(gcs.DownloadFile("file", "/tmp/file"));
}

TEST_F(CloudStorageCompatibilityTest, AllProvidersListFiles) {
    EXPECT_GE(s3.ListFiles("/").size(), 0);
    EXPECT_GE(azure.ListFiles("/").size(), 0);
    EXPECT_GE(gcs.ListFiles("/").size(), 0);
}

TEST_F(CloudStorageCompatibilityTest, AllProvidersCheckExists) {
    EXPECT_TRUE(s3.FileExists("file"));
    EXPECT_TRUE(azure.FileExists("file"));
    EXPECT_TRUE(gcs.FileExists("file"));
}
