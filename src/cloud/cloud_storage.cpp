#include "bendcore/cloud/cloud_storage.hpp"
#include <mutex>

namespace bendcore::cloud {

CloudStorageManager::CloudStorageManager() 
    : active_provider_(StorageProvider::LOCAL), upload_count_(0), download_count_(0), total_bytes_(0) {}

CloudStorageManager::~CloudStorageManager() = default;

bool CloudStorageManager::register_provider(StorageProvider provider, std::shared_ptr<ICloudStorage> storage) {
  std::unique_lock<std::shared_mutex> lock(manager_mutex_);
  if (!storage) return false;
  providers_[provider] = storage;
  return true;
}

bool CloudStorageManager::set_active_provider(StorageProvider provider) {
  std::unique_lock<std::shared_mutex> lock(manager_mutex_);
  if (providers_.find(provider) == providers_.end()) return false;
  active_provider_ = provider;
  return true;
}

std::shared_ptr<ICloudStorage> CloudStorageManager::get_active_provider() const {
  std::shared_lock<std::shared_mutex> lock(manager_mutex_);
  auto it = providers_.find(active_provider_);
  if (it != providers_.end()) return it->second;
  return nullptr;
}

bool CloudStorageManager::connect_to_provider(const std::map<std::string, std::string>& credentials) {
  auto provider = get_active_provider();
  if (!provider) return false;
  return provider->connect(credentials);
}

bool CloudStorageManager::disconnect_provider() {
  auto provider = get_active_provider();
  if (!provider) return false;
  return provider->disconnect();
}

UploadResult CloudStorageManager::upload_file(const std::string& local_path, const std::string& cloud_path) {
  auto provider = get_active_provider();
  if (!provider) return {false, cloud_path, 0, "No active provider"};
  
  auto result = provider->upload_file(local_path, cloud_path);
  
  if (result.success) {
    std::unique_lock<std::shared_mutex> lock(manager_mutex_);
    upload_count_++;
    total_bytes_ += result.bytes_uploaded;
  }
  return result;
}

bool CloudStorageManager::download_file(const std::string& cloud_path, const std::string& local_path) {
  auto provider = get_active_provider();
  if (!provider) return false;
  
  bool result = provider->download_file(cloud_path, local_path);
  if (result) {
    std::unique_lock<std::shared_mutex> lock(manager_mutex_);
    download_count_++;
  }
  return result;
}

bool CloudStorageManager::delete_file(const std::string& cloud_path) {
  auto provider = get_active_provider();
  if (!provider) return false;
  return provider->delete_file(cloud_path);
}

std::optional<FileMetadata> CloudStorageManager::get_file_metadata(const std::string& cloud_path) const {
  auto provider = get_active_provider();
  if (!provider) return std::nullopt;
  return provider->get_file_metadata(cloud_path);
}

std::vector<FileMetadata> CloudStorageManager::list_files(const std::string& directory) const {
  auto provider = get_active_provider();
  if (!provider) return {};
  return provider->list_files(directory);
}

bool CloudStorageManager::create_directory(const std::string& path) {
  auto provider = get_active_provider();
  if (!provider) return false;
  return provider->create_directory(path);
}

int CloudStorageManager::get_upload_count() const {
  std::shared_lock<std::shared_mutex> lock(manager_mutex_);
  return upload_count_;
}

int CloudStorageManager::get_download_count() const {
  std::shared_lock<std::shared_mutex> lock(manager_mutex_);
  return download_count_;
}

long CloudStorageManager::get_total_bytes_transferred() const {
  std::shared_lock<std::shared_mutex> lock(manager_mutex_);
  return total_bytes_;
}

void CloudStorageManager::clear_statistics() {
  std::unique_lock<std::shared_mutex> lock(manager_mutex_);
  upload_count_ = 0;
  download_count_ = 0;
  total_bytes_ = 0;
}

} // namespace bendcore::cloud
