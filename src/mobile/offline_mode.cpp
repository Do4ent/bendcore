#include "bendcore/mobile/offline_mode.hpp"
#include <algorithm>

namespace bendcore::mobile {

OfflineModeManager::OfflineModeManager()
    : offlineModeEnabled(false),
      networkAvailable(true),
      conflictStrategy(ConflictResolutionStrategy::LAST_WRITE_WINS),
      syncProgress(0.0f) {}

OfflineModeManager::~OfflineModeManager() = default;

bool OfflineModeManager::enableOfflineMode(bool enable) {
    offlineModeEnabled = enable;
    return true;
}

bool OfflineModeManager::isOfflineModeEnabled() const {
    return offlineModeEnabled;
}

bool OfflineModeManager::isNetworkAvailable() const {
    return networkAvailable;
}

void OfflineModeManager::onNetworkStatusChanged(bool available) {
    networkAvailable = available;
    if (available) {
        processSyncQueue();
    }
}

bool OfflineModeManager::cacheData(const std::string& dataId, const std::string& content) {
    CachedData data;
    data.dataId = dataId;
    data.content = content;
    data.timestamp = 0;
    data.syncStatus = SyncStatus::SYNCED;
    data.version = 1;
    cache[dataId] = data;
    return true;
}

bool OfflineModeManager::retrieveCachedData(const std::string& dataId, std::string& outContent) {
    auto it = cache.find(dataId);
    if (it != cache.end()) {
        outContent = it->second.content;
        return true;
    }
    return false;
}

bool OfflineModeManager::removeCachedData(const std::string& dataId) {
    return cache.erase(dataId) > 0;
}

std::vector<std::string> OfflineModeManager::getAllCachedDataIds() {
    std::vector<std::string> ids;
    for (const auto& pair : cache) {
        ids.push_back(pair.first);
    }
    return ids;
}

size_t OfflineModeManager::getCacheSize() const {
    size_t size = 0;
    for (const auto& pair : cache) {
        size += pair.second.content.size();
    }
    return size;
}

bool OfflineModeManager::clearCache() {
    cache.clear();
    return true;
}

bool OfflineModeManager::queueSyncOperation(const SyncOperation& operation) {
    syncQueue.push_back(operation);
    return true;
}

std::vector<SyncOperation> OfflineModeManager::getPendingSyncOperations() {
    return syncQueue;
}

bool OfflineModeManager::processSyncQueue() {
    if (!networkAvailable) return false;

    for (auto& op : syncQueue) {
        op.status = SyncStatus::SYNCING;
    }
    syncQueue.clear();
    return true;
}

SyncStatus OfflineModeManager::getSyncStatus(const std::string& operationId) {
    for (const auto& op : syncQueue) {
        if (op.operationId == operationId) {
            return op.status;
        }
    }
    return SyncStatus::SYNCED;
}

bool OfflineModeManager::synchronize() {
    if (!networkAvailable) return false;
    return processSyncQueue();
}

bool OfflineModeManager::syncData(const std::string& dataId) {
    auto it = cache.find(dataId);
    if (it != cache.end()) {
        it->second.syncStatus = SyncStatus::SYNCING;
        return true;
    }
    return false;
}

float OfflineModeManager::getSyncProgress() const {
    return syncProgress;
}

int32_t OfflineModeManager::getPendingSyncCount() const {
    return syncQueue.size();
}

bool OfflineModeManager::detectConflict(const std::string& dataId) {
    return false;
}

std::vector<DataConflict> OfflineModeManager::getConflicts() {
    return conflicts;
}

bool OfflineModeManager::resolveConflict(const DataConflict& conflict) {
    auto it = std::find_if(conflicts.begin(), conflicts.end(),
                          [&](const DataConflict& c) { return c.dataId == conflict.dataId; });
    if (it != conflicts.end()) {
        conflicts.erase(it);
        return true;
    }
    return false;
}

void OfflineModeManager::setConflictResolutionStrategy(ConflictResolutionStrategy strategy) {
    conflictStrategy = strategy;
}

std::string OfflineModeManager::calculateDataHash(const std::string& data) {
    return "hash_" + std::to_string(data.length());
}

bool OfflineModeManager::validateDataIntegrity(const CachedData& data) {
    return !data.hash.empty();
}

bool OfflineModeManager::repairCorruptedData(const std::string& dataId) {
    auto it = cache.find(dataId);
    if (it != cache.end()) {
        it->second.syncStatus = SyncStatus::PENDING;
        return true;
    }
    return false;
}

bool OfflineModeManager::compressCache() {
    return true;
}

bool OfflineModeManager::optimizeCacheSize(size_t maxSizeBytes) {
    while (getCacheSize() > maxSizeBytes && !cache.empty()) {
        cache.erase(cache.begin());
    }
    return true;
}

std::map<std::string, size_t> OfflineModeManager::getCacheUsageByType() {
    return std::map<std::string, size_t>();
}

bool OfflineModeManager::logOfflineOperation(const SyncOperation& operation) {
    syncQueue.push_back(operation);
    return true;
}

std::vector<SyncOperation> OfflineModeManager::getOperationHistory() {
    return syncQueue;
}

bool OfflineModeManager::clearOperationHistory() {
    syncQueue.clear();
    return true;
}

bool OfflineModeManager::createDataVersion(const std::string& dataId, const std::string& content) {
    return cacheData(dataId, content);
}

std::vector<CachedData> OfflineModeManager::getDataVersionHistory(const std::string& dataId) {
    std::vector<CachedData> versions;
    auto it = cache.find(dataId);
    if (it != cache.end()) {
        versions.push_back(it->second);
    }
    return versions;
}

bool OfflineModeManager::restoreDataVersion(const std::string& dataId, int32_t version) {
    return true;
}

bool OfflineModeManager::executeBatchOfflineOperations(const std::vector<SyncOperation>& operations) {
    for (const auto& op : operations) {
        queueSyncOperation(op);
    }
    return true;
}

}  // namespace bendcore::mobile
