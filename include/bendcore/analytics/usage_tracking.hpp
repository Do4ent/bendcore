#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace bendcore::analytics {

struct UserEvent {
    std::string userId, eventType, eventName;
    int64_t timestamp;
    std::map<std::string, std::string> properties;
};

struct UserSession {
    std::string sessionId, userId;
    int64_t startTime, endTime;
    int32_t eventCount;
};

class UsageTracking {
public:
    UsageTracking();
    ~UsageTracking();
    void trackEvent(const std::string& uid, const std::string& type,
                   const std::string& name, const std::map<std::string, std::string>& props);
    void startSession(const std::string& sid, const std::string& uid);
    void endSession(const std::string& sid);
    std::vector<UserEvent> getEvents(const std::string& uid, int32_t max = 100) const;
    std::vector<UserSession> getSessions(const std::string& uid) const;
    std::map<std::string, int32_t> getEventFrequency(const std::string& uid) const;
    int32_t getTotalEvents() const;
    std::string generateReport(const std::string& uid) const;
    std::string generateJsonReport() const;
    void clearEvents(const std::string& uid);
    void clearAll();
private:
    mutable std::mutex m_;
    std::map<std::string, std::vector<UserEvent>> events_;
    std::map<std::string, UserSession> sessions_;
};

}
