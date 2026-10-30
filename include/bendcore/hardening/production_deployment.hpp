#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// PRODUCTION DEPLOYMENT - GAP-093
// ==============================================================================

enum class DeploymentStatus {
    PENDING,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
    ROLLED_BACK
};

enum class EnvironmentType {
    DEVELOPMENT,
    STAGING,
    PRODUCTION
};

struct DeploymentConfig {
    std::string app_name;
    std::string version;
    EnvironmentType environment;
    bool auto_rollback = true;
};

struct CanaryHealth {
    double health_percentage;
    int error_rate;
    bool is_healthy;
};

struct HealthResult {
    bool is_healthy;
    double health_percentage;
    std::vector<std::string> failed_checks;
};

struct DeploymentMetrics {
    double deployment_time_seconds;
    int services_updated;
    bool zero_downtime;
};

class DeploymentOrchestrator {
public:
    DeploymentOrchestrator();
    void configure(const DeploymentConfig& config);
    const DeploymentConfig& getConfig() const;
    bool canDeploy() const;
    DeploymentStatus getStatus() const;
    bool deploy();
private:
    DeploymentConfig config_;
    DeploymentStatus status_;
    bool configured_;
};

class BlueGreenDeployment {
public:
    BlueGreenDeployment();
    void setupBlueEnvironment();
    void setupGreenEnvironment();
    std::string getActiveEnvironment() const;
    bool deployToGreen();
    bool validateGreen();
    bool switchToGreen();
    bool rollbackToBlue();
private:
    std::string active_env_;
    bool blue_ready_;
    bool green_ready_;
    bool green_valid_;
};

class CanaryDeployment {
public:
    CanaryDeployment();
    void setCanaryPercentage(int pct);
    int getCanaryPercentage() const;
    bool deployCanary(const std::string& version);
    bool monitorCanary(int seconds);
    CanaryHealth getCanaryHealth() const;
    bool promoteCanaryToProduction();
    bool rollbackCanary();
private:
    int canary_percentage_;
    std::string canary_version_;
    bool deployed_;
};

class RollbackManager {
public:
    RollbackManager();
    void createCheckpoint(const std::string& version);
    std::vector<std::string> getAvailableCheckpoints() const;
    bool rollbackToCheckpoint(const std::string& version);
    int getRollbackCount() const;
private:
    std::vector<std::string> checkpoints_;
    int rollback_count_;
};

class HealthCheckManager {
public:
    HealthCheckManager();
    void registerHealthCheck(const std::string& name, std::function<bool()> check);
    HealthResult runAllChecks();
    bool isSystemHealthy() const;
private:
    std::vector<std::pair<std::string, std::function<bool()>>> checks_;
    bool last_healthy_;
};

class DeploymentAutomation {
public:
    DeploymentAutomation();
    bool validatePrerequisites();
    bool prepareDeployment();
    bool executeDeployment();
    bool verifyDeployment();
};

class DeploymentLogger {
public:
    DeploymentLogger();
    void logDeploymentStart(const std::string& version);
    void logDeploymentProgress(const std::string& message);
    void logDeploymentSuccess(const DeploymentMetrics& metrics);
    void logDeploymentFailure(const std::string& reason);
    std::vector<std::string> getDeploymentLog() const;
private:
    std::vector<std::string> log_;
};

class DeploymentValidator {
public:
    DeploymentValidator();
    bool validatePackage(const std::string& package_path);
    bool validateDependencies();
    bool validateConfiguration();
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
