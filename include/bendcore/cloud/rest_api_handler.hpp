#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>
#include <functional>

namespace bendcore::cloud {

// HTTP methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
};

// HTTP response status codes
enum class HttpStatus {
    OK = 200,
    Created = 201,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    Conflict = 409,
    InternalServerError = 500
};

// Request context
struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string path;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> queryParams;
    std::string body;
    std::string userId;  // From auth header
    std::string token;   // Auth token
};

// Response context
struct HttpResponse {
    HttpStatus status = HttpStatus::OK;
    std::map<std::string, std::string> headers;
    std::string body;
    int64_t timestamp = 0;
    int latencyMs = 0;
};

// REST API endpoint definition
struct ApiEndpoint {
    std::string path;
    HttpMethod method;
    std::string description;
    std::string requiredRole;  // "admin", "editor", "viewer"
    bool requiresAuth = true;
};

// REST API error response
struct ApiError {
    int code = 0;
    std::string message;
    std::string detail;
    std::string timestamp;
    std::string path;
};

// Authentication context
struct AuthContext {
    std::string userId;
    std::string token;
    std::string role;
    int64_t issuedAt = 0;
    int64_t expiresAt = 0;
    bool isValid = false;
};

// REST API Handler
class RestApiHandler {
public:
    explicit RestApiHandler();
    ~RestApiHandler();

    // Route registration
    void registerEndpoint(
        const std::string& path,
        HttpMethod method,
        const std::string& description,
        const std::string& requiredRole = "viewer"
    );

    // Request handling
    HttpResponse handleRequest(const HttpRequest& request);

    // Authentication
    AuthContext authenticateToken(const std::string& token);

    bool validateRequest(const HttpRequest& request);

    // Response builders
    HttpResponse buildJsonResponse(
        const std::string& jsonBody,
        HttpStatus status = HttpStatus::OK
    );

    HttpResponse buildErrorResponse(
        const ApiError& error,
        HttpStatus status = HttpStatus::BadRequest
    );

    HttpResponse buildEmptyResponse(HttpStatus status = HttpStatus::OK);

    // CORS handling
    bool handleCorsRequest(const HttpRequest& request);

    // Rate limiting
    bool checkRateLimit(const std::string& userId);

    // Request logging
    void logRequest(const HttpRequest& request, const HttpResponse& response);

    // API endpoint discovery
    std::vector<ApiEndpoint> getAvailableEndpoints() const;

    // Health check
    struct HealthStatus {
        bool isHealthy = true;
        std::string version;
        int64_t uptime = 0;
        int requestsProcessed = 0;
        double averageLatencyMs = 0.0;
    };

    HealthStatus getHealthStatus() const;

    // API version info
    std::string getApiVersion() const { return apiVersion; }

    // Endpoint implementation mapping
    typedef std::function<HttpResponse(const HttpRequest&)> EndpointHandler;

    void setEndpointHandler(
        const std::string& path,
        HttpMethod method,
        EndpointHandler handler
    );

    // Cloud project endpoints
    HttpResponse handleGetProjects(const HttpRequest& request);
    HttpResponse handleCreateProject(const HttpRequest& request);
    HttpResponse handleGetProject(const HttpRequest& request);
    HttpResponse handleUpdateProject(const HttpRequest& request);
    HttpResponse handleDeleteProject(const HttpRequest& request);

    // Collaboration endpoints
    HttpResponse handleGetPresence(const HttpRequest& request);
    HttpResponse handleUpdatePresence(const HttpRequest& request);
    HttpResponse handleAcquireLock(const HttpRequest& request);
    HttpResponse handleReleaseLock(const HttpRequest& request);

    // Notification endpoints
    HttpResponse handleGetNotifications(const HttpRequest& request);
    HttpResponse handleMarkAsRead(const HttpRequest& request);

    // Version endpoints
    HttpResponse handleGetVersionHistory(const HttpRequest& request);
    HttpResponse handleCreateVersion(const HttpRequest& request);
    HttpResponse handleRestoreVersion(const HttpRequest& request);

    // Backup endpoints
    HttpResponse handleCreateBackup(const HttpRequest& request);
    HttpResponse handleRestoreBackup(const HttpRequest& request);
    HttpResponse handleListBackups(const HttpRequest& request);

private:
    std::vector<ApiEndpoint> endpoints;
    std::map<std::string, std::map<HttpMethod, EndpointHandler>> handlers;
    std::string apiVersion = "1.0.0";
    int64_t startTime = 0;
    int requestCount = 0;
    double totalLatencyMs = 0.0;
};

}  // namespace bendcore::cloud
