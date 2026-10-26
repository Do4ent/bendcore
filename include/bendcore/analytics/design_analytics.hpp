#pragma once
#include <string>
#include <map>
#include <any>
namespace bendcore::analytics {
class DesignAnalytics {
public:
    virtual bool TrackDesignMetrics(const std::string& design_id) = 0;
    virtual std::map<std::string, std::any> GetMetrics(const std::string& design_id) = 0;
    virtual ~DesignAnalytics() = default;
};
}
