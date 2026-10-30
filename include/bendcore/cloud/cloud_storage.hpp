#ifndef BENDCORE_CLOUD_CLOUD_STORAGE_HPP
#define BENDCORE_CLOUD_CLOUD_STORAGE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <cstdint>

namespace bendcore::cloud {

/**
 * Storage provider type
 */
enum class StorageProvider { AWS_S3, AZURE_BLOB, GOOGLE_CLOUD, LOCAL };

/**
 * File metadata
 */
struct FileMetadata {
  std::string name;
  std::string path;
  int64_t size;
  std::string content_type;
  long modified_time;
  std::string etag;
};

/**
 * Upload result
 */
struct UploadResult {
  bool success;
  std::string file_path;
  int64_t bytes_uploaded;
  std::string error_message;
};

/**
 * Cloud storage interface
 */
class ICloudStorage {
 public:
  virtual ~ICloudStorage() = default;
  virtual bool connect(const std::map<std::string, std::string>& credentials) = 0;
  virtual bool disconnect() = 0;
  virtual bool is_connected() const = 0;
  virtual UploadResult upload_file(const std::string& local_path, const std::string& cloud_path) = 0;
  virtual bool download_file(const std::string& cloud_path, const std::string& local_path) = 0;
  virtual bool delete_file(const std::string& cloud_path) = 0;
  virtual std::optional<FileMetadata> get_file_metadata(const std::string& cloud_path) const = 0;
  virtual std::vector<FileMetadata> list_files(const std::string& directory) const = 0;
  virtual bool create_directory(const std::string& path) = 0;
};

/**
 * Cloud storage manager
 */
class CloudStorageManager {
 public:
  CloudStorageManager();
  ~CloudStorageManager();

  bool register_provider(StorageProvider provider, std::shared_ptr<ICloudStorage> storage);
  bool set_active_provider(StorageProvider provider);
  std::shared_ptr<ICloudStorage> get_active_provider() const;
  bool connect_to_provider(const std::map<std::string, std::string>& credentials);
  bool disconnect_provider();

  UploadResult upload_file(const std::string& local_path, const std::string& cloud_path);
  bool download_file(const std::string& cloud_path, const std::string& local_path);
  bool delete_file(const std::string& cloud_path);
  std::optional<FileMetadata> get_file_metadata(const std::string& cloud_path) const;
  std::vector<FileMetadata> list_files(const std::string& directory) const;
  bool create_directory(const std::string& path);

  int get_upload_count() const;
  int get_download_count() const;
  long get_total_bytes_transferred() const;
  void clear_statistics();

 private:
  mutable std::shared_mutex manager_mutex_;
  std::map<StorageProvider, std::shared_ptr<ICloudStorage>> providers_;
  StorageProvider active_provider_;
  int upload_count_;
  int download_count_;
  long total_bytes_;
};

} // namespace bendcore::cloud

#endif // BENDCORE_CLOUD_CLOUD_STORAGE_HPP
