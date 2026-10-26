#pragma once
#include <string>
#include <vector>
namespace bendcore::cloud {
class CloudBackup {
public:
    virtual bool CreateBackup(const std::string& path) = 0;
    virtual bool RestoreBackup(const std::string& backup_id, const std::string& path) = 0;
    virtual std::vector<std::string> ListBackups() const = 0;
    virtual bool DeleteBackup(const std::string& backup_id) = 0;
    virtual ~CloudBackup() = default;
};
}
