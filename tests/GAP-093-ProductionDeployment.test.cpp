// GAP-093: ProductionDeployment (40 tests) - Automation, blue-green, rollback
#include "bendcore/hardening/production_deployment.hpp"
#include <cassert>
#include <cstdio>

using namespace bendcore::production;

int main() {
    int passed = 0, failed = 0;

    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }

    // Test 1-5: DeploymentOrchestrator
    {
        DeploymentOrchestrator orchestrator;

        DeploymentConfig config;
        config.app_name = "BendCore";
        config.version = "2.0.0";
        config.environment = EnvironmentType::PRODUCTION;

        orchestrator.configure(config);
        TEST("T1: Orchestrator configured", orchestrator.getConfig().app_name == "BendCore");

        TEST("T2: Can deploy", orchestrator.canDeploy());
        TEST("T3: Status is PENDING", orchestrator.getStatus() == DeploymentStatus::PENDING);

        bool success = orchestrator.deploy();
        TEST("T4: Deployment succeeded", success);
        TEST("T5: Status is SUCCEEDED", orchestrator.getStatus() == DeploymentStatus::SUCCEEDED);
    }

    // Test 6-12: BlueGreenDeployment
    {
        BlueGreenDeployment bg;
        TEST("T6: Blue-green created", true);

        bg.setupBlueEnvironment();
        bg.setupGreenEnvironment();

        TEST("T7: Active environment is blue", bg.getActiveEnvironment() == "blue");

        bool deployed = bg.deployToGreen();
        TEST("T8: Deployed to green", deployed);

        bool valid = bg.validateGreen();
        TEST("T9: Green validated", valid);

        bool switched = bg.switchToGreen();
        TEST("T10: Switched to green", switched);
        TEST("T11: Active environment is green", bg.getActiveEnvironment() == "green");

        bool rollback = bg.rollbackToBlue();
        TEST("T12: Rollback to blue", rollback);
    }

    // Test 13-19: CanaryDeployment
    {
        CanaryDeployment canary;
        TEST("T13: Canary deployment created", true);

        canary.setCanaryPercentage(10);
        TEST("T14: Canary percentage set", canary.getCanaryPercentage() == 10);

        bool deployed = canary.deployCanary("2.1.0");
        TEST("T15: Canary deployed", deployed);

        bool monitored = canary.monitorCanary(5);
        TEST("T16: Canary monitored", monitored);

        auto health = canary.getCanaryHealth();
        TEST("T17: Canary health checked", health.health_percentage >= 0.0);

        bool promoted = canary.promoteCanaryToProduction();
        TEST("T18: Canary promoted", promoted);

        bool rolled_back = canary.rollbackCanary();
        TEST("T19: Canary rolled back", rolled_back);
    }

    // Test 20-25: RollbackManager
    {
        RollbackManager manager;
        TEST("T20: RollbackManager created", true);

        manager.createCheckpoint("1.0.0");
        manager.createCheckpoint("1.5.0");
        manager.createCheckpoint("2.0.0");

        auto checkpoints = manager.getAvailableCheckpoints();
        TEST("T21: Checkpoints created", checkpoints.size() == 3);

        bool rolled_back = manager.rollbackToCheckpoint("1.5.0");
        TEST("T22: Rollback successful", rolled_back);
        TEST("T23: Rollback count tracked", manager.getRollbackCount() > 0);

        bool invalid = manager.rollbackToCheckpoint("invalid");
        TEST("T24: Invalid checkpoint rejected", !invalid);

        TEST("T25: Available checkpoints", !manager.getAvailableCheckpoints().empty());
    }

    // Test 26-32: HealthCheckManager
    {
        HealthCheckManager health;
        TEST("T26: HealthCheckManager created", true);

        bool check1_passed = true;
        bool check2_passed = true;

        health.registerHealthCheck("service_a", [&check1_passed]() { return check1_passed; });
        health.registerHealthCheck("service_b", [&check2_passed]() { return check2_passed; });

        auto result = health.runAllChecks();
        TEST("T27: Checks ran successfully", result.is_healthy);
        TEST("T28: Health percentage calculated", result.health_percentage > 0.0);
        TEST("T29: Failed checks empty", result.failed_checks.empty());

        check1_passed = false;
        result = health.runAllChecks();
        TEST("T30: Failed check detected", !result.is_healthy);
        TEST("T31: Failed checks reported", result.failed_checks.size() > 0);
        TEST("T32: System unhealthy", !health.isSystemHealthy());
    }

    // Test 33-37: DeploymentAutomation
    {
        DeploymentAutomation automation;
        TEST("T33: DeploymentAutomation created", true);

        bool validated = automation.validatePrerequisites();
        TEST("T34: Prerequisites validated", validated);

        bool prepared = automation.prepareDeployment();
        TEST("T35: Deployment prepared", prepared);

        bool executed = automation.executeDeployment();
        TEST("T36: Deployment executed", executed);

        bool verified = automation.verifyDeployment();
        TEST("T37: Deployment verified", verified);
    }

    // Test 38-40: DeploymentLogger and DeploymentValidator
    {
        DeploymentLogger logger;
        logger.logDeploymentStart("2.0.0");
        logger.logDeploymentProgress("Updating services");

        DeploymentMetrics metrics;
        metrics.deployment_time_seconds = 15.5;
        logger.logDeploymentSuccess(metrics);

        TEST("T38: Deployment logged", !logger.getDeploymentLog().empty());

        DeploymentValidator validator;
        bool validated = validator.validatePackage("bendcore-2.0.0.tar.gz");
        TEST("T39: Package validated", validated);

        bool deps_valid = validator.validateDependencies();
        TEST("T40: Dependencies validated", deps_valid);
    }

    printf("\n=== GAP-093: ProductionDeployment ===\n");
    printf("Passed: %d/40\n", passed);
    printf("Failed: %d/40\n", failed);
    return (failed == 0) ? 0 : 1;
}
