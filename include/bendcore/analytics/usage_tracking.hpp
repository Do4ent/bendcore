#pragma once
#include <string>
namespace bendcore::analytics {
class UsageTracking {
public:
    virtual bool TrackEvent(const std::string& event_name) = 0;
    virtual int GetEventCount(const std::string& event_name) = 0;
    virtual ~UsageTracking() = default;
};
}
