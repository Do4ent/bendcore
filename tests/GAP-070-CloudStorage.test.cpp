#include <cassert>
#include <cstdio>
#include <string>
#include "bendcore/cloud/cloud_storage.hpp"

using namespace bendcore::cloud;

class MockCloudStorage : public ICloudStorage {
public:
  bool connect(const std::map<std::string, std::string>& creds) override { return true; }
  bool disconnect() override { return true; }
  bool is_connected() const override { return true; }
  UploadResult upload_file(const std::string& local, const std::string& cloud) override {
    return {true, cloud, 1000, ""};
  }
  bool download_file(const std::string& cloud, const std::string& local) override { return true; }
  bool delete_file(const std::string& cloud) override { return true; }
  std::optional<FileMetadata> get_file_metadata(const std::string& cloud) const override {
    return FileMetadata{cloud, cloud, 1000, "text/plain", 0, "abc123"};
  }
  std::vector<FileMetadata> list_files(const std::string& dir) const override { return {}; }
  bool create_directory(const std::string& path) override { return true; }
};

void test_register_provider() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  bool result = mgr.register_provider(StorageProvider::AWS_S3, storage);
  assert(result);
  printf("✓ test_register_provider\n");
}

void test_set_active_provider() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  bool result = mgr.set_active_provider(StorageProvider::AWS_S3);
  assert(result);
  printf("✓ test_set_active_provider\n");
}

void test_get_active_provider() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  auto provider = mgr.get_active_provider();
  assert(provider != nullptr);
  printf("✓ test_get_active_provider\n");
}

void test_connect_to_provider() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  bool result = mgr.connect_to_provider({});
  assert(result);
  printf("✓ test_connect_to_provider\n");
}

void test_disconnect_provider() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  bool result = mgr.disconnect_provider();
  assert(result);
  printf("✓ test_disconnect_provider\n");
}

void test_upload_file() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  auto result = mgr.upload_file("/local/file.txt", "/cloud/file.txt");
  assert(result.success);
  printf("✓ test_upload_file\n");
}

void test_download_file() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  bool result = mgr.download_file("/cloud/file.txt", "/local/file.txt");
  assert(result);
  printf("✓ test_download_file\n");
}

void test_delete_file() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  bool result = mgr.delete_file("/cloud/file.txt");
  assert(result);
  printf("✓ test_delete_file\n");
}

void test_get_file_metadata() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  auto metadata = mgr.get_file_metadata("/cloud/file.txt");
  assert(metadata.has_value());
  assert(metadata->size == 1000);
  printf("✓ test_get_file_metadata\n");
}

void test_list_files() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  auto files = mgr.list_files("/cloud/");
  assert(files.size() >= 0);
  printf("✓ test_list_files\n");
}

void test_create_directory() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  bool result = mgr.create_directory("/cloud/newdir");
  assert(result);
  printf("✓ test_create_directory\n");
}

void test_get_upload_count() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  mgr.upload_file("/local/f1.txt", "/cloud/f1.txt");
  mgr.upload_file("/local/f2.txt", "/cloud/f2.txt");
  assert(mgr.get_upload_count() == 2);
  printf("✓ test_get_upload_count\n");
}

void test_get_download_count() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  mgr.download_file("/cloud/f1.txt", "/local/f1.txt");
  mgr.download_file("/cloud/f2.txt", "/local/f2.txt");
  assert(mgr.get_download_count() == 2);
  printf("✓ test_get_download_count\n");
}

void test_get_total_bytes_transferred() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  mgr.upload_file("/local/f1.txt", "/cloud/f1.txt");
  long bytes = mgr.get_total_bytes_transferred();
  assert(bytes >= 1000);
  printf("✓ test_get_total_bytes_transferred\n");
}

void test_clear_statistics() {
  CloudStorageManager mgr;
  auto storage = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, storage);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  mgr.upload_file("/local/f1.txt", "/cloud/f1.txt");
  mgr.clear_statistics();
  assert(mgr.get_upload_count() == 0);
  printf("✓ test_clear_statistics\n");
}

void test_multiple_providers() {
  CloudStorageManager mgr;
  auto s3 = std::make_shared<MockCloudStorage>();
  auto azure = std::make_shared<MockCloudStorage>();
  mgr.register_provider(StorageProvider::AWS_S3, s3);
  mgr.register_provider(StorageProvider::AZURE_BLOB, azure);
  mgr.set_active_provider(StorageProvider::AWS_S3);
  assert(mgr.get_active_provider() != nullptr);
  printf("✓ test_multiple_providers\n");
}

void test_file_metadata_fields() {
  FileMetadata meta{"file.txt", "/path/file.txt", 5000, "text/plain", 123456, "hash123"};
  assert(meta.size == 5000);
  assert(meta.content_type == "text/plain");
  printf("✓ test_file_metadata_fields\n");
}

void test_upload_result_success() {
  UploadResult result{true, "/cloud/file.txt", 2000, ""};
  assert(result.success);
  assert(result.bytes_uploaded == 2000);
  printf("✓ test_upload_result_success\n");
}

void test_upload_result_failure() {
  UploadResult result{false, "", 0, "Network error"};
  assert(!result.success);
  assert(result.error_message == "Network error");
  printf("✓ test_upload_result_failure\n");
}

void test_set_provider_nonexistent() {
  CloudStorageManager mgr;
  bool result = mgr.set_active_provider(StorageProvider::AWS_S3);
  assert(!result);
  printf("✓ test_set_provider_nonexistent\n");
}

void test_register_null_provider() {
  CloudStorageManager mgr;
  bool result = mgr.register_provider(StorageProvider::AWS_S3, nullptr);
  assert(!result);
  printf("✓ test_register_null_provider\n");
}

int main() {
  printf("Running GAP-070: CloudStorage Tests\n");
  printf("===================================\n\n");
  
  test_register_provider();
  test_set_active_provider();
  test_get_active_provider();
  test_connect_to_provider();
  test_disconnect_provider();
  test_upload_file();
  test_download_file();
  test_delete_file();
  test_get_file_metadata();
  test_list_files();
  test_create_directory();
  test_get_upload_count();
  test_get_download_count();
  test_get_total_bytes_transferred();
  test_clear_statistics();
  test_multiple_providers();
  test_file_metadata_fields();
  test_upload_result_success();
  test_upload_result_failure();
  test_set_provider_nonexistent();
  test_register_null_provider();
  
  printf("\n===================================\n");
  printf("All 21 tests passed!\n");
  return 0;
}
