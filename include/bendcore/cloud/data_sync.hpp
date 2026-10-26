#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <any>

namespace bendcore::cloud {

class DataSync {
public:
    virtual ~DataSync() = default;
    virtual bool SyncData(const std::string& local_path, const std::string& remote_path) = 0;
    virtual bool ResolveConflict(const std::string& local_version, const std::string& remote_version) = 0;
    virtual bool IsSynced(const std::string& path) const = 0;
    virtual std::vector<std::string> GetPendingChanges() const = 0;
    virtual bool CommitChanges(const std::string& message) = 0;
};

} // namespace bendcore::cloud
