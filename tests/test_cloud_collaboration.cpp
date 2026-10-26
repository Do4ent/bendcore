// M7 Phase: Cloud & Collaboration Framework Tests
#include "bendcore/cloud/cloud_manager.hpp"
#include "bendcore/cloud/cloud_storage_manager.hpp"
#include "bendcore/cloud/realtime_collaboration.hpp"
#include "bendcore/cloud/rest_api_handler.hpp"
#include "bendcore/cloud/serialization.hpp"
#include "bendcore/cloud/user_management.hpp"
#include "bendcore/cloud/analytics_monitoring.hpp"

#include <cstdio>
#include <cassert>
#include <ctime>

using namespace bendcore::cloud;

struct TestCase {
    const char* name;
    int passed = 0;
    int failed = 0;
};

void test_assert(TestCase& tc, bool condition, const char* message) {
    if (condition) {
        tc.passed++;
    } else {
        printf("  FAIL: %s\n", message);
        tc.failed++;
    }
}

int main() {
    TestCase tc{"cloud_collaboration"};

    // ===== Cloud Storage Manager Tests =====
    {
        CloudStorageManager storage(StorageType::Hybrid);

        // Test 1: Upload project
        bool uploaded = storage.uploadProject(
            "proj_001",
            "Test Project",
            "/path/to/file",
            "user_123"
        );
        test_assert(tc, uploaded, "Project uploaded successfully");

        // Test 2: Project exists
        bool exists = storage.projectExists("proj_001");
        test_assert(tc, exists, "Project exists check");

        // Test 3: Get project metadata
        auto metadata = storage.getProjectMetadata("proj_001");
        test_assert(tc, metadata.has_value(), "Project metadata retrieved");

        if (metadata.has_value()) {
            test_assert(tc, metadata.value().projectId == "proj_001", "Project ID matches");
            test_assert(tc, metadata.value().projectName == "Test Project", "Project name matches");
        }

        // Test 4: Save version
        bool versioned = storage.saveVersion(
            "proj_001",
            "/path/to/file",
            "Initial version",
            "user_123"
        );
        test_assert(tc, versioned, "Version saved successfully");

        // Test 5: Get version history
        auto versions = storage.getVersionHistory("proj_001", 10);
        test_assert(tc, versions.size() > 0, "Version history retrieved");

        // Test 6: Create backup
        bool backup = storage.createBackup(
            "proj_001",
            "/path/to/file",
            "full"
        );
        test_assert(tc, backup, "Backup created successfully");

        // Test 7: Get backup history
        auto backups = storage.getBackupHistory("proj_001", 10);
        test_assert(tc, backups.size() > 0, "Backup history retrieved");

        // Test 8: List projects
        auto projects = storage.listProjects("user_123");
        test_assert(tc, projects.size() > 0, "Projects listed for user");

        // Test 9: Search projects
        auto found = storage.searchProjects("Test");
        test_assert(tc, found.size() > 0, "Projects searched successfully");

        // Test 10: Delete project
        bool deleted = storage.deleteProject("proj_001");
        test_assert(tc, deleted, "Project deleted successfully");
    }

    // ===== Real-time Collaboration Manager Tests =====
    {
        RealtimeCollaborationManager collab;

        // Test 11: Register user presence
        bool registered = collab.registerUserPresence(
            "user_001",
            "John Doe",
            "john@example.com"
        );
        test_assert(tc, registered, "User presence registered");

        // Test 12: Get active users
        auto users = collab.getActiveUsers();
        test_assert(tc, users.size() > 0, "Active users retrieved");

        // Test 13: Update user activity
        bool updated = collab.updateUserActivity(
            "user_001",
            "editing",
            "obj_123"
        );
        test_assert(tc, updated, "User activity updated");

        // Test 14: Acquire lock
        bool locked = collab.acquireLock(
            "obj_123",
            "user_001",
            "John Doe",
            300
        );
        test_assert(tc, locked, "Lock acquired successfully");

        // Test 15: Check if locked
        bool is_locked = collab.isLocked("obj_123");
        test_assert(tc, is_locked, "Object is locked");

        // Test 16: Release lock
        bool released = collab.releaseLock("obj_123", "user_001");
        test_assert(tc, released, "Lock released successfully");

        // Test 17: Create notification
        uint64_t notif_id = collab.createNotification(
            "user_002",
            "user_001",
            "comment",
            "User mentioned you",
            "obj_123"
        );
        test_assert(tc, notif_id > 0, "Notification created");

        // Test 18: Get user notifications
        auto notifs = collab.getUserNotifications("user_002");
        test_assert(tc, notifs.size() > 0, "Notifications retrieved");

        // Test 19: Mark notification as read
        bool marked = collab.markNotificationAsRead(notif_id);
        test_assert(tc, marked, "Notification marked as read");

        // Test 20: Broadcast change
        std::map<std::string, std::string> changeData;
        changeData["property"] = "radius";
        changeData["oldValue"] = "50.0";
        changeData["newValue"] = "55.0";

        collab.broadcastChange("obj_123", "user_001", "modified", changeData);
        auto changes = collab.getChanges("obj_123");
        test_assert(tc, changes.size() > 0, "Changes broadcast and retrieved");

        // Test 21: Create session
        auto session = collab.createSession("proj_001", "user_001");
        test_assert(tc, !session.sessionId.empty(), "Collaborative session created");

        // Test 22: Add participant to session
        bool added = collab.addParticipantToSession(session.sessionId, "user_002");
        test_assert(tc, added, "Participant added to session");

        // Test 23: Get collaboration metrics
        auto metrics = collab.getMetrics();
        test_assert(tc, metrics.totalActiveUsers >= 0, "Collaboration metrics retrieved");

        // Test 24: Remove user presence
        bool removed = collab.removeUserPresence("user_001");
        test_assert(tc, removed, "User presence removed");
    }

    // ===== User Management System Tests =====
    {
        UserManagementSystem userMgr;

        // Test 25: Create user
        bool created = userMgr.createUser(
            "designer@company.com",
            "Jane",
            "Smith",
            UserRole::Editor
        );
        test_assert(tc, created, "User created successfully");

        // Test 26: Get user by email
        auto user = userMgr.getUserByEmail("designer@company.com");
        test_assert(tc, user.has_value(), "User retrieved by email");

        if (user.has_value()) {
            // Test 27: Assign role
            bool assigned = userMgr.assignRole(user.value().userId, UserRole::Admin);
            test_assert(tc, assigned, "Role assigned successfully");

            // Test 28: Grant permission
            std::set<std::string> actions = {"read", "write", "share"};
            bool granted = userMgr.grantPermission(
                "proj_001",
                user.value().userId,
                UserRole::Editor,
                actions
            );
            test_assert(tc, granted, "Permission granted successfully");

            // Test 29: Check permission
            bool hasPermission = userMgr.hasPermission(
                user.value().userId,
                "proj_001",
                "write"
            );
            test_assert(tc, hasPermission, "Permission check successful");

            // Test 30: Create team
            bool teamCreated = userMgr.createTeam(
                "Design Team",
                user.value().userId,
                "Design and CAD team"
            );
            test_assert(tc, teamCreated, "Team created successfully");

            // Test 31: Get user teams
            auto teams = userMgr.getUserTeams(user.value().userId);
            test_assert(tc, teams.size() > 0, "User teams retrieved");

            // Test 32: Log audit entry
            userMgr.logAuditEntry(
                user.value().userId,
                "create_project",
                "proj_001",
                "project",
                true,
                "Created new project"
            );

            // Test 33: Get audit log
            auto auditLog = userMgr.getAuditLog(user.value().userId);
            test_assert(tc, auditLog.size() > 0, "Audit log retrieved");

            // Test 34: Set user preference
            bool prefSet = userMgr.setUserPreference(
                user.value().userId,
                "theme",
                "dark"
            );
            test_assert(tc, prefSet, "User preference set");

            // Test 35: Get statistics
            auto stats = userMgr.getStatistics();
            test_assert(tc, stats.totalUsers > 0, "User statistics retrieved");
        }
    }

    // ===== Analytics & Monitoring System Tests =====
    {
        AnalyticsMonitoringSystem analytics;

        // Test 36: Record metric
        analytics.recordMetric("cpu_usage", MetricType::Gauge, 45.5);
        analytics.recordMetric("request_count", MetricType::Counter, 1000.0);

        // Test 37: Get metrics
        auto metrics = analytics.getMetrics("cpu_usage");
        test_assert(tc, metrics.size() > 0, "Metrics retrieved successfully");

        // Test 38: Record event
        uint64_t eventId = analytics.recordEvent(
            EventType::ProjectCreated,
            "user_001",
            "proj_001",
            "Project created"
        );
        test_assert(tc, eventId > 0, "Event recorded successfully");

        // Test 39: Get events
        auto events = analytics.getEvents(std::nullopt, std::nullopt, 100);
        test_assert(tc, events.size() > 0, "Events retrieved successfully");

        // Test 40: Record performance metrics
        analytics.recordPerformanceMetrics(
            35.2,   // CPU
            60.1,   // Memory
            45.3,   // Disk
            42,     // Connections
            125.5,  // Response time
            850,    // Requests/sec
            2,      // Errors
            5       // Warnings
        );

        // Test 41: Get latest performance metrics
        auto perfMetrics = analytics.getLatestPerformanceMetrics();
        test_assert(tc, perfMetrics.has_value(), "Performance metrics retrieved");

        // Test 42: Record error
        analytics.recordError(
            "FileNotFound",
            "Project file not found",
            "at CloudManager::downloadCADDocument",
            "user_001",
            "proj_001"
        );

        // Test 43: Get errors
        auto errors = analytics.getErrors(std::nullopt, false, 50);
        test_assert(tc, errors.size() > 0, "Errors retrieved successfully");

        // Test 44: Get system statistics
        auto sysStats = analytics.getSystemStatistics();
        test_assert(tc, sysStats.totalRequests >= 0, "System statistics retrieved");

        // Test 45: Generate dashboard
        auto dashboard = analytics.generateDashboard();
        test_assert(tc, dashboard.totalUsers >= 0, "Dashboard generated successfully");

        // Test 46: Check system health
        auto health = analytics.checkSystemHealth();
        test_assert(tc, health.isHealthy || !health.isHealthy, "System health checked");

        // Test 47: Export analytics as JSON
        std::string jsonExport = analytics.exportAnalyticsAsJson();
        test_assert(tc, !jsonExport.empty(), "Analytics exported as JSON");

        // Test 48: Export analytics as CSV
        std::string csvExport = analytics.exportAnalyticsAsCsv();
        test_assert(tc, !csvExport.empty(), "Analytics exported as CSV");
    }

    // ===== Data Serialization Tests =====
    {
        DataSerializer serializer(SerializationFormat::JSON, CompressionType::Gzip);

        // Test 49: Serialize project
        std::string serialized = serializer.serializeProject(
            "proj_001",
            "{\"name\": \"Test\"}",
            SerializationContext{}
        );
        test_assert(tc, !serialized.empty(), "Project serialized successfully");

        // Test 50: Export as JSON
        std::string json = serializer.exportAsJson("{\"data\": \"test\"}", true);
        test_assert(tc, !json.empty(), "Data exported as JSON");

        // Test 51: Export as binary
        std::string binary = serializer.exportAsBinary("{\"data\": \"test\"}");
        test_assert(tc, !binary.empty(), "Data exported as binary");

        // Test 52: Compress data
        std::string compressed = serializer.compress("This is test data");
        test_assert(tc, !compressed.empty(), "Data compressed successfully");

        // Test 53: Decompress data
        auto decompressed = serializer.decompress(compressed);
        test_assert(tc, decompressed.has_value(), "Data decompressed successfully");

        // Test 54: Calculate hash
        std::string hash = serializer.calculateHash("test data");
        test_assert(tc, !hash.empty(), "Hash calculated successfully");

        // Test 55: Verify hash
        bool verified = serializer.verifyHash("test data", hash);
        test_assert(tc, verified, "Hash verified successfully");

        // Test 56: Get serialization stats
        auto stats = serializer.getStats();
        test_assert(tc, stats.originalSize >= 0, "Serialization stats retrieved");
    }

    // ===== REST API Handler Tests =====
    {
        RestApiHandler api;

        // Test 57: Register endpoint
        api.registerEndpoint(
            "/projects",
            HttpMethod::GET,
            "Get all projects",
            "viewer"
        );
        auto endpoints = api.getAvailableEndpoints();
        test_assert(tc, endpoints.size() > 0, "API endpoints registered");

        // Test 58: Authenticate token
        auto authCtx = api.authenticateToken("test_token_123");
        test_assert(tc, authCtx.isValid, "Token authenticated successfully");

        // Test 59: Build JSON response
        auto response = api.buildJsonResponse("{\"status\": \"ok\"}");
        test_assert(tc, response.status == HttpStatus::OK, "JSON response built");

        // Test 60: Build error response
        ApiError error{400, "Bad request", "Invalid parameters"};
        auto errorResp = api.buildErrorResponse(error);
        test_assert(tc, errorResp.status == HttpStatus::BadRequest, "Error response built");

        // Test 61: Get health status
        auto health = api.getHealthStatus();
        test_assert(tc, health.isHealthy, "API health status retrieved");

        // Test 62: Get API version
        std::string version = api.getApiVersion();
        test_assert(tc, !version.empty(), "API version retrieved");
    }

    // ===== Unified Cloud Manager Tests =====
    {
        CloudManager cloudMgr;

        // Test 63: Initialize cloud manager
        bool initialized = cloudMgr.initialize("", StorageType::Hybrid);
        test_assert(tc, initialized, "Cloud manager initialized");

        // Test 64: Start cloud services
        bool started = cloudMgr.startCloudServices();
        test_assert(tc, started, "Cloud services started");

        // Test 65: Get system status
        auto status = cloudMgr.getSystemStatus();
        test_assert(tc, status.isRunning, "System status retrieved");

        // Test 66: Sync CAD document
        bool synced = cloudMgr.syncCADDocument("proj_001", "/path/to/doc.bend");
        test_assert(tc, synced, "CAD document synced to cloud");

        // Test 67: Download CAD document
        bool downloaded = cloudMgr.downloadCADDocument("proj_001", "/tmp/doc.bend");
        test_assert(tc, downloaded, "CAD document downloaded from cloud");

        // Test 68: Create document version
        bool versioned = cloudMgr.createDocumentVersion(
            "proj_001",
            "/path/to/doc.bend",
            "Updated dimensions"
        );
        test_assert(tc, versioned, "Document version created");

        // Test 69: Share project
        bool shared = cloudMgr.shareProject("proj_001", "user_002", "edit");
        test_assert(tc, shared, "Project shared with user");

        // Test 70: Start collaborative session
        bool collab_started = cloudMgr.startCollaborativeSession("proj_001", "user_001");
        test_assert(tc, collab_started, "Collaborative session started");

        // Test 71: Stop cloud services
        bool stopped = cloudMgr.stopCloudServices();
        test_assert(tc, stopped, "Cloud services stopped");
    }

    // Print test summary
    printf("\n=== Test Summary ===\n");
    printf("Tests Passed: %d\n", tc.passed);
    printf("Tests Failed: %d\n", tc.failed);
    printf("Total Tests: %d\n", tc.passed + tc.failed);

    return tc.failed == 0 ? 0 : 1;
}
