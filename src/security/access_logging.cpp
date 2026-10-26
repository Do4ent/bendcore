#include "bendcore/security/access_logging.hpp"
#include <algorithm>

namespace bendcore::security {

AccessLoggingSystem::AccessLoggingSystem()
    : realTimeMonitoringEnabled(false),
      tamperDetectionEnabled(false) {}

AccessLoggingSystem::~AccessLoggingSystem() = default;

bool AccessLoggingSystem::logAccess(const AccessLog& log) {
    accessLog.push_back(log);
    return true;
}

std::vector<AccessLog> AccessLoggingSystem::getAccessLog() {
    return accessLog;
}

std::vector<AccessLog> AccessLoggingSystem::getAccessLogByUser(const std::string& userId) {
    std::vector<AccessLog> result;
    for (const auto& log : accessLog) {
        if (log.userId == userId) result.push_back(log);
    }
    return result;
}

std::vector<AccessLog> AccessLoggingSystem::getAccessLogByResource(const std::string& resourceId) {
    std::vector<AccessLog> result;
    for (const auto& log : accessLog) {
        if (log.resourceId == resourceId) result.push_back(log);
    }
    return result;
}

std::vector<AccessLog> AccessLoggingSystem::getAccessLogByTimeRange(int64_t startTime, int64_t endTime) {
    std::vector<AccessLog> result;
    for (const auto& log : accessLog) {
        if (log.timestamp >= startTime && log.timestamp <= endTime) result.push_back(log);
    }
    return result;
}

std::vector<AccessLog> AccessLoggingSystem::filterByAccessType(AccessType type) {
    std::vector<AccessLog> result;
    for (const auto& log : accessLog) {
        if (log.accessType == type) result.push_back(log);
    }
    return result;
}

std::vector<AccessLog> AccessLoggingSystem::filterByResult(AccessResult result) {
    std::vector<AccessLog> logs;
    for (const auto& log : accessLog) {
        if (log.result == result) logs.push_back(log);
    }
    return logs;
}

std::vector<AccessLog> AccessLoggingSystem::filterByIPAddress(const std::string& ipAddress) {
    std::vector<AccessLog> result;
    for (const auto& log : accessLog) {
        if (log.ipAddress == ipAddress) result.push_back(log);
    }
    return result;
}

std::vector<AccessPattern> AccessLoggingSystem::getAccessPatterns() {
    return patterns;
}

AccessPattern AccessLoggingSystem::getAccessPattern(const std::string& userId) {
    for (const auto& pattern : patterns) {
        if (pattern.userId == userId) return pattern;
    }
    return AccessPattern();
}

bool AccessLoggingSystem::detectAnomalousAccess(const AccessLog& log) {
    return false;
}

std::vector<AccessLog> AccessLoggingSystem::getAnomalousAccesses() {
    return std::vector<AccessLog>();
}

std::vector<AccessLog> AccessLoggingSystem::getUserActivity(const std::string& userId) {
    return getAccessLogByUser(userId);
}

int32_t AccessLoggingSystem::getAccessCountByUser(const std::string& userId) {
    return getAccessLogByUser(userId).size();
}

int32_t AccessLoggingSystem::getDeniedAccessCount(const std::string& userId) {
    int32_t count = 0;
    for (const auto& log : getAccessLogByUser(userId)) {
        if (log.result == AccessResult::DENIED) count++;
    }
    return count;
}

std::map<std::string, int32_t> AccessLoggingSystem::getResourceAccessCount() {
    std::map<std::string, int32_t> counts;
    for (const auto& log : accessLog) {
        counts[log.resourceId]++;
    }
    return counts;
}

std::vector<AccessLog> AccessLoggingSystem::getResourceAccesses(const std::string& resourceId) {
    return getAccessLogByResource(resourceId);
}

std::vector<std::string> AccessLoggingSystem::getAccessorsForResource(const std::string& resourceId) {
    std::vector<std::string> accessors;
    for (const auto& log : getAccessLogByResource(resourceId)) {
        accessors.push_back(log.userId);
    }
    return accessors;
}

int32_t AccessLoggingSystem::getTotalAccessCount(const std::string& resourceId) {
    return getResourceAccesses(resourceId).size();
}

bool AccessLoggingSystem::detectPrivilegeEscalation() {
    return false;
}

std::vector<AccessLog> AccessLoggingSystem::getPrivilegeEscalationAttempts() {
    return std::vector<AccessLog>();
}

bool AccessLoggingSystem::flagSuspiciousEscalation(const AccessLog& log) {
    return true;
}

std::string AccessLoggingSystem::generateForensicReport() {
    return "Forensic Report: " + std::to_string(accessLog.size()) + " events";
}

std::string AccessLoggingSystem::generateForensicReportByUser(const std::string& userId) {
    auto logs = getAccessLogByUser(userId);
    return "Forensic Report for " + userId + ": " + std::to_string(logs.size()) + " events";
}

std::string AccessLoggingSystem::generateForensicReportByResource(const std::string& resourceId) {
    auto logs = getAccessLogByResource(resourceId);
    return "Forensic Report for " + resourceId + ": " + std::to_string(logs.size()) + " events";
}

std::vector<AccessLog> AccessLoggingSystem::reconstructAccessSequence(const std::string& userId) {
    return getAccessLogByUser(userId);
}

bool AccessLoggingSystem::exportAccessLog(const std::string& outputPath) {
    return true;
}

bool AccessLoggingSystem::archiveAccessLog(int64_t beforeTimestamp) {
    return true;
}

size_t AccessLoggingSystem::getAccessLogSize() const {
    return accessLog.size();
}

bool AccessLoggingSystem::deleteOldLogs(int64_t beforeTimestamp) {
    auto it = std::remove_if(accessLog.begin(), accessLog.end(),
                             [beforeTimestamp](const AccessLog& log) { return log.timestamp < beforeTimestamp; });
    accessLog.erase(it, accessLog.end());
    return true;
}

std::string AccessLoggingSystem::calculateLogHash() {
    return "hash_" + std::to_string(accessLog.size());
}

bool AccessLoggingSystem::verifyLogIntegrity() {
    return true;
}

bool AccessLoggingSystem::enableLogTamperDetection(bool enable) {
    tamperDetectionEnabled = enable;
    return true;
}

bool AccessLoggingSystem::isLogTamperDetectionEnabled() const {
    return tamperDetectionEnabled;
}

bool AccessLoggingSystem::enableRealTimeMonitoring(bool enable) {
    realTimeMonitoringEnabled = enable;
    return true;
}

bool AccessLoggingSystem::isRealTimeMonitoringEnabled() const {
    return realTimeMonitoringEnabled;
}

std::vector<AccessLog> AccessLoggingSystem::getRecentAccesses(int32_t count) {
    std::vector<AccessLog> recent;
    if (accessLog.size() <= count) return accessLog;
    recent.insert(recent.end(), accessLog.end() - count, accessLog.end());
    return recent;
}

int32_t AccessLoggingSystem::getTotalAccessCount() const {
    return accessLog.size();
}

int32_t AccessLoggingSystem::getAllowedAccessCount() const {
    int32_t count = 0;
    for (const auto& log : accessLog) {
        if (log.result == AccessResult::ALLOWED) count++;
    }
    return count;
}

int32_t AccessLoggingSystem::getDeniedAccessCount() const {
    int32_t count = 0;
    for (const auto& log : accessLog) {
        if (log.result == AccessResult::DENIED) count++;
    }
    return count;
}

float AccessLoggingSystem::getDenialRate() const {
    if (accessLog.empty()) return 0.0f;
    return static_cast<float>(getDeniedAccessCount()) / accessLog.size();
}

}  // namespace bendcore::security
