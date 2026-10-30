#pragma once
#include "cloud_storage_manager.hpp"
#include "realtime_collaboration.hpp"
#include "rest_api_handler.hpp"
#include "serialization.hpp"
#include "user_management.hpp"
#include "analytics_monitoring.hpp"

#include <memory>
#include <string>

namespace bendcore::cloud {

// Unified Cloud Manager for all cloud and collaboration services
class CloudManager {
public:
    explicit CloudManager();
    ~CloudManager();

    // Initialize with configuration
    bool initialize(
        const std::string& configFilePath,
        StorageType storageType = StorageType::Hybrid
    );

    // Component accessors
    ProjectCloudStorageManager& getStorageManager() { return *storageManager; }
    RealtimeCollaborationManager& getCollaborationManager() { return *collaborationManager; }
    RestApiHandler& getApiHandler() { return *apiHandler; }
    DataSerializer& getSerializer() { return *serializer; }
    UserManagementSystem& getUserManager() { return *userManager; }
    AnalyticsMonitoringSystem& getAnalytics() { return *analyticsManager; }

    // System operations
    bool startCloudServices();
    bool stopCloudServices();
    bool isRunning() const { return isServiceRunning; }

    // Configuration
    struct CloudConfiguration {
        std::string storageEndpoint;
        std::string apiEndpoint;
        std::string region;
        std::string authProvider;
        int maxConnections = 100;
        int requestTimeoutSeconds = 30;
        bool enableCompression = true;
        bool enableEncryption = true;
        bool enableAnalytics = true;
        bool enableAuditLog = true;
    };

    bool loadConfiguration(const CloudConfiguration& config);
    CloudConfiguration getConfiguration() const;

    // Status and monitoring
    struct SystemStatus {
        bool isRunning = false;
        int activeConnections = 0;
        int requestsProcessed = 0;
        double uptime = 0.0;
        std::string lastError;
        int errorCount = 0;
    };

    SystemStatus getSystemStatus() const;

    // Backup and recovery
    bool createFullBackup(const std::string& backupPath);
    bool restoreFromBackup(const std::string& backupPath);

    // Integration with CAD document
    bool syncCADDocument(
        const std::string& projectId,
        const std::string& documentPath
    );

    bool downloadCADDocument(
        const std::string& projectId,
        const std::string& outputPath
    );

    // Version management integration
    bool createDocumentVersion(
        const std::string& projectId,
        const std::string& documentPath,
        const std::string& message
    );

    // Sharing and collaboration
    bool shareProject(
        const std::string& projectId,
        const std::string& targetUser,
        const std::string& permission = "view"
    );

    bool startCollaborativeSession(
        const std::string& projectId,
        const std::string& userId
    );

    bool endCollaborativeSession(
        const std::string& sessionId
    );

private:
    std::unique_ptr<ProjectCloudStorageManager> storageManager;
    std::unique_ptr<RealtimeCollaborationManager> collaborationManager;
    std::unique_ptr<RestApiHandler> apiHandler;
    std::unique_ptr<DataSerializer> serializer;
    std::unique_ptr<UserManagementSystem> userManager;
    std::unique_ptr<AnalyticsMonitoringSystem> analyticsManager;

    CloudConfiguration configuration;
    bool isServiceRunning = false;
    int64_t startTime = 0;
    int totalRequests = 0;
    int totalErrors = 0;
};

}  // namespace bendcore::cloud
