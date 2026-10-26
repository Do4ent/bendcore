#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace bendcore::cloud {

/**
 * Cloud storage abstraction for multiple providers (S3, Azure, Google Cloud)
 */
class CloudStorage {
public:
    virtual ~CloudStorage() = default;

    /**
     * Upload file to cloud storage
     */
    virtual bool UploadFile(const std::string& local_path, const std::string& remote_path) = 0;

    /**
     * Download file from cloud storage
     */
    virtual bool DownloadFile(const std::string& remote_path, const std::string& local_path) = 0;

    /**
     * Delete file from cloud storage
     */
    virtual bool DeleteFile(const std::string& remote_path) = 0;

    /**
     * List files in directory
     */
    virtual std::vector<std::string> ListFiles(const std::string& directory) = 0;

    /**
     * Check if file exists
     */
    virtual bool FileExists(const std::string& remote_path) = 0;

    /**
     * Get file size
     */
    virtual size_t GetFileSize(const std::string& remote_path) = 0;

    /**
     * Get file metadata
     */
    virtual std::map<std::string, std::string> GetFileMetadata(const std::string& remote_path) = 0;

    /**
     * Create directory in cloud storage
     */
    virtual bool CreateDirectory(const std::string& directory) = 0;
};

/**
 * AWS S3 implementation
 */
class S3CloudStorage : public CloudStorage {
public:
    S3CloudStorage(const std::string& bucket, const std::string& region = "us-east-1");

    bool UploadFile(const std::string& local_path, const std::string& remote_path) override;
    bool DownloadFile(const std::string& remote_path, const std::string& local_path) override;
    bool DeleteFile(const std::string& remote_path) override;
    std::vector<std::string> ListFiles(const std::string& directory) override;
    bool FileExists(const std::string& remote_path) override;
    size_t GetFileSize(const std::string& remote_path) override;
    std::map<std::string, std::string> GetFileMetadata(const std::string& remote_path) override;
    bool CreateDirectory(const std::string& directory) override;

private:
    std::string bucket_;
    std::string region_;
};

/**
 * Azure Blob Storage implementation
 */
class AzureCloudStorage : public CloudStorage {
public:
    AzureCloudStorage(const std::string& connection_string, const std::string& container);

    bool UploadFile(const std::string& local_path, const std::string& remote_path) override;
    bool DownloadFile(const std::string& remote_path, const std::string& local_path) override;
    bool DeleteFile(const std::string& remote_path) override;
    std::vector<std::string> ListFiles(const std::string& directory) override;
    bool FileExists(const std::string& remote_path) override;
    size_t GetFileSize(const std::string& remote_path) override;
    std::map<std::string, std::string> GetFileMetadata(const std::string& remote_path) override;
    bool CreateDirectory(const std::string& directory) override;

private:
    std::string connection_string_;
    std::string container_;
};

/**
 * Google Cloud Storage implementation
 */
class GCSCloudStorage : public CloudStorage {
public:
    GCSCloudStorage(const std::string& project_id, const std::string& bucket);

    bool UploadFile(const std::string& local_path, const std::string& remote_path) override;
    bool DownloadFile(const std::string& remote_path, const std::string& local_path) override;
    bool DeleteFile(const std::string& remote_path) override;
    std::vector<std::string> ListFiles(const std::string& directory) override;
    bool FileExists(const std::string& remote_path) override;
    size_t GetFileSize(const std::string& remote_path) override;
    std::map<std::string, std::string> GetFileMetadata(const std::string& remote_path) override;
    bool CreateDirectory(const std::string& directory) override;

private:
    std::string project_id_;
    std::string bucket_;
};

} // namespace bendcore::cloud
