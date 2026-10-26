#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// PRODUCTION DEPLOYMENT - GAP-093
// ==============================================================================
// Deployment automation, rollback procedures, hotfixes, release management

enum class DeploymentStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    ROLLED_BACK
};

enum class ReleaseType {
    MAJOR,
    MINOR,
    PATCH,
    HOTFIX,
    BETA
};

struct DeploymentConfig {
    std::string version;
    ReleaseType releaseType;
    std::string targetEnvironment;  // "staging", "production"
    bool autoRollback;
    int32_t healthCheckIntervalSeconds;
    int32_t rollbackThreshold;
};

struct Deployment {
    std::string deploymentId;
    std::string version;
    DeploymentStatus status;
    int64_t startTime;
    int64_t endTime;
    std::string deployedBy;
    std::string targetEnvironment;
    bool isRolledBack;
    std::string rollbackReason;
};

struct Hotfix {
    std::string hotfixId;
    std::string version;
    std::string description;
    std::vector<std::string> affectedComponents;
    int64_t createdAt;
    bool isApplied;
};

class ProductionDeploymentManager {
public:
    ProductionDeploymentManager();
    ~ProductionDeploymentManager();

    // Deployment management
    bool createDeployment(const DeploymentConfig& config, std::string& outDeploymentId);
    bool executeDeployment(const std::string& deploymentId);
    bool cancelDeployment(const std::string& deploymentId);
    DeploymentStatus getDeploymentStatus(const std::string& deploymentId);

    // Pre-deployment checks
    bool performPreDeploymentValidation(const std::string& version);
    bool checkDependencies(const std::string& version);
    bool validateDeploymentPackage(const std::string& packagePath);
    bool runSmokeTests();

    // Deployment execution
    bool deployToStaging(const std::string& version);
    bool deployToProduction(const std::string& version);
    bool deployWithCanaryRelease(const std::string& version, float canaryPercentage);
    bool deployWithBlueGreenStrategy(const std::string& version);

    // Health checks
    bool runHealthCheck();
    bool waitForHealthy(int32_t timeoutSeconds);
    bool isApplicationHealthy() const;
    std::vector<std::string> getHealthIssues();

    // Rollback procedures
    bool createRollbackPoint(const std::string& deploymentId);
    bool rollback(const std::string& deploymentId);
    bool rollbackToVersion(const std::string& version);
    bool automatedRollback();
    bool hasRollbackPoint(const std::string& deploymentId) const;

    // Hotfix management
    bool createHotfix(const std::string& version, const std::string& description, Hotfix& outHotfix);
    bool applyHotfix(const std::string& hotfixId);
    bool removeHotfix(const std::string& hotfixId);
    std::vector<Hotfix> getPendingHotfixes();
    std::vector<Hotfix> getAppliedHotfixes();

    // Version management
    bool setCurrentVersion(const std::string& version);
    std::string getCurrentVersion() const;
    std::vector<std::string> getAvailableVersions();
    std::vector<std::string> getReleaseHistory();

    // Deployment monitoring
    std::vector<Deployment> getDeploymentHistory();
    Deployment getDeployment(const std::string& deploymentId);
    int32_t getSuccessfulDeploymentCount() const;
    float getDeploymentSuccessRate() const;

    // Configuration management
    bool backupConfiguration();
    bool restoreConfiguration(int64_t timestamp);
    bool validateConfiguration();
    bool synchronizeConfiguration();

    // Release management
    bool scheduleRelease(const std::string& version, int64_t releaseTime);
    bool cancelRelease(const std::string& version);
    bool isReleaseScheduled(const std::string& version) const;
    std::vector<std::string> getScheduledReleases();

    // Database migration
    bool executeDatabaseMigration(const std::string& version);
    bool validateDatabaseConsistency();
    bool rollbackDatabaseMigration(const std::string& version);

    // Feature flags
    bool enableFeatureFlag(const std::string& featureName);
    bool disableFeatureFlag(const std::string& featureName);
    bool isFeatureFlagEnabled(const std::string& featureName) const;
    std::vector<std::string> getActiveFeatureFlags();

    // Deployment reporting
    std::string generateDeploymentReport();
    std::string generateReleaseNotes(const std::string& version);
    bool exportDeploymentMetrics(const std::string& outputPath);

private:
    std::map<std::string, Deployment> deployments;
    std::vector<Hotfix> hotfixes;
    std::string currentVersion;
    std::map<std::string, std::string> rollbackPoints;
    std::map<std::string, bool> featureFlags;
};

}  // namespace bendcore::hardening
