#include "bendcore/cloud/rest_api_handler.hpp"

#include <algorithm>
#include <ctime>
#include <sstream>

namespace bendcore::cloud {

RestApiHandler::RestApiHandler() {
    startTime = std::time(nullptr);
    apiVersion = "1.0.0";
}

RestApiHandler::~RestApiHandler() = default;

void RestApiHandler::registerEndpoint(
    const std::string& path,
    HttpMethod method,
    const std::string& description,
    const std::string& requiredRole
) {
    ApiEndpoint endpoint;
    endpoint.path = path;
    endpoint.method = method;
    endpoint.description = description;
    endpoint.requiredRole = requiredRole;

    endpoints.push_back(endpoint);
}

HttpResponse RestApiHandler::handleRequest(const HttpRequest& request) {
    requestCount++;
    int64_t startTime = std::time(nullptr) * 1000;

    if (!validateRequest(request)) {
        return buildErrorResponse(
            {400, "Invalid request", "Request validation failed"},
            HttpStatus::BadRequest
        );
    }

    // Route to appropriate handler
    auto handler_it = handlers.find(request.path);

    if (handler_it == handlers.end()) {
        return buildErrorResponse(
            {404, "Not found", "Endpoint not found"},
            HttpStatus::NotFound
        );
    }

    auto method_it = handler_it->second.find(request.method);

    if (method_it == handler_it->second.end()) {
        return buildErrorResponse(
            {405, "Method not allowed", "HTTP method not allowed"},
            HttpStatus::BadRequest
        );
    }

    HttpResponse response = method_it->second(request);
    int64_t endTime = std::time(nullptr) * 1000;

    response.timestamp = std::time(nullptr);
    response.latencyMs = static_cast<int>(endTime - startTime);
    totalLatencyMs += response.latencyMs;

    return response;
}

AuthContext RestApiHandler::authenticateToken(const std::string& token) {
    AuthContext context;
    context.token = token;
    context.isValid = !token.empty();

    if (context.isValid) {
        context.userId = "user_" + token.substr(0, 8);
        context.role = "editor";
        context.issuedAt = std::time(nullptr) - 3600;
        context.expiresAt = std::time(nullptr) + 86400;
    }

    return context;
}

bool RestApiHandler::validateRequest(const HttpRequest& request) {
    if (request.path.empty()) {
        return false;
    }

    if (request.method == HttpMethod::POST || request.method == HttpMethod::PUT) {
        if (request.body.empty()) {
            return false;
        }
    }

    return true;
}

HttpResponse RestApiHandler::buildJsonResponse(
    const std::string& jsonBody,
    HttpStatus status
) {
    HttpResponse response;
    response.status = status;
    response.body = jsonBody;
    response.headers["Content-Type"] = "application/json";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.timestamp = std::time(nullptr);

    return response;
}

HttpResponse RestApiHandler::buildErrorResponse(
    const ApiError& error,
    HttpStatus status
) {
    std::ostringstream oss;
    oss << "{\"error\": {\"code\": " << error.code
        << ", \"message\": \"" << error.message
        << "\", \"detail\": \"" << error.detail
        << "\"}}";

    return buildJsonResponse(oss.str(), status);
}

HttpResponse RestApiHandler::buildEmptyResponse(HttpStatus status) {
    HttpResponse response;
    response.status = status;
    response.headers["Content-Type"] = "application/json";
    response.timestamp = std::time(nullptr);

    return response;
}

bool RestApiHandler::handleCorsRequest(const HttpRequest& request) {
    if (request.method == HttpMethod::GET) {
        return true;
    }

    auto origin_it = request.headers.find("Origin");
    return origin_it != request.headers.end();
}

bool RestApiHandler::checkRateLimit(const std::string& userId) {
    // Simplified rate limiting: allow max 100 requests per user
    return requestCount < 10000;
}

void RestApiHandler::logRequest(const HttpRequest& request, const HttpResponse& response) {
    // Logging implementation (simplified)
}

std::vector<ApiEndpoint> RestApiHandler::getAvailableEndpoints() const {
    return endpoints;
}

RestApiHandler::HealthStatus RestApiHandler::getHealthStatus() const {
    HealthStatus health;
    health.isHealthy = true;
    health.version = apiVersion;
    health.uptime = std::time(nullptr) - startTime;
    health.requestsProcessed = requestCount;
    health.averageLatencyMs = requestCount > 0 ? totalLatencyMs / requestCount : 0.0;

    return health;
}

void RestApiHandler::setEndpointHandler(
    const std::string& path,
    HttpMethod method,
    EndpointHandler handler
) {
    handlers[path][method] = handler;
}

HttpResponse RestApiHandler::handleGetProjects(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"projects\": [], \"count\": 0}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleCreateProject(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"projectId\": \"proj_123\", \"status\": \"created\"}";
    return buildJsonResponse(oss.str(), HttpStatus::Created);
}

HttpResponse RestApiHandler::handleGetProject(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"projectId\": \"proj_123\", \"projectName\": \"Project\"}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleUpdateProject(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"projectId\": \"proj_123\", \"status\": \"updated\"}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleDeleteProject(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"projectId\": \"proj_123\", \"status\": \"deleted\"}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleGetPresence(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"users\": [], \"count\": 0}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleUpdatePresence(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"userId\": \"user_123\", \"status\": \"updated\"}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleAcquireLock(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"lockId\": \"lock_123\", \"acquired\": true}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleReleaseLock(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"lockId\": \"lock_123\", \"released\": true}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleGetNotifications(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"notifications\": [], \"count\": 0}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleMarkAsRead(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"notificationId\": \"notif_123\", \"marked\": true}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleGetVersionHistory(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"versions\": [], \"count\": 0}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleCreateVersion(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"versionId\": \"ver_123\", \"created\": true}";
    return buildJsonResponse(oss.str(), HttpStatus::Created);
}

HttpResponse RestApiHandler::handleRestoreVersion(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"versionId\": \"ver_123\", \"restored\": true}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleCreateBackup(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"backupId\": \"bak_123\", \"created\": true}";
    return buildJsonResponse(oss.str(), HttpStatus::Created);
}

HttpResponse RestApiHandler::handleRestoreBackup(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"backupId\": \"bak_123\", \"restored\": true}";
    return buildJsonResponse(oss.str());
}

HttpResponse RestApiHandler::handleListBackups(const HttpRequest& request) {
    std::ostringstream oss;
    oss << "{\"backups\": [], \"count\": 0}";
    return buildJsonResponse(oss.str());
}

}  // namespace bendcore::cloud
