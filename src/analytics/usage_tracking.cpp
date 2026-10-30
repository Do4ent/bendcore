#include "bendcore/analytics/usage_tracking.hpp"
#include <algorithm>
#include <sstream>

namespace bendcore::analytics {

UsageTracking::UsageTracking() {}
UsageTracking::~UsageTracking() {
    std::lock_guard<std::mutex> l(m_);
    events_.clear();
    sessions_.clear();
}
void UsageTracking::trackEvent(const std::string& uid, const std::string& type,
                              const std::string& name, const std::map<std::string, std::string>& props) {
    std::lock_guard<std::mutex> l(m_);
    events_[uid].push_back({uid, type, name, std::chrono::system_clock::now().time_since_epoch().count(), props});
}
void UsageTracking::startSession(const std::string& sid, const std::string& uid) {
    std::lock_guard<std::mutex> l(m_);
    sessions_[sid] = {sid, uid, std::chrono::system_clock::now().time_since_epoch().count(), 0, 0};
}
void UsageTracking::endSession(const std::string& sid) {
    std::lock_guard<std::mutex> l(m_);
    auto it = sessions_.find(sid);
    if (it != sessions_.end()) it->second.endTime = std::chrono::system_clock::now().time_since_epoch().count();
}
std::vector<UserEvent> UsageTracking::getEvents(const std::string& uid, int32_t max) const {
    std::lock_guard<std::mutex> l(m_);
    auto it = events_.find(uid);
    if (it == events_.end()) return {};
    std::vector<UserEvent> result;
    size_t start = it->second.size() > static_cast<size_t>(max) ? it->second.size() - max : 0;
    for (size_t i = start; i < it->second.size(); ++i) result.push_back(it->second[i]);
    return result;
}
std::vector<UserSession> UsageTracking::getSessions(const std::string& uid) const {
    std::lock_guard<std::mutex> l(m_);
    std::vector<UserSession> result;
    for (const auto& e : sessions_) {
        if (e.second.userId == uid) result.push_back(e.second);
    }
    return result;
}
std::map<std::string, int32_t> UsageTracking::getEventFrequency(const std::string& uid) const {
    std::lock_guard<std::mutex> l(m_);
    std::map<std::string, int32_t> freq;
    auto it = events_.find(uid);
    if (it != events_.end()) {
        for (const auto& e : it->second) freq[e.eventName]++;
    }
    return freq;
}
int32_t UsageTracking::getTotalEvents() const {
    std::lock_guard<std::mutex> l(m_);
    int32_t total = 0;
    for (const auto& e : events_) total += e.second.size();
    return total;
}
std::string UsageTracking::generateReport(const std::string& uid) const {
    std::lock_guard<std::mutex> l(m_);
    std::ostringstream oss;
    oss << "=== Usage Report ===\nUser: " << uid << "\nEvents: ";
    auto it = events_.find(uid);
    oss << (it != events_.end() ? it->second.size() : 0) << "\nSessions: " << getSessions(uid).size() << "\n";
    return oss.str();
}
std::string UsageTracking::generateJsonReport() const {
    std::lock_guard<std::mutex> l(m_);
    return "{\"users\":" + std::to_string(events_.size()) + ",\"total\":" + std::to_string(getTotalEvents()) + "}";
}
void UsageTracking::clearEvents(const std::string& uid) {
    std::lock_guard<std::mutex> l(m_);
    events_.erase(uid);
}
void UsageTracking::clearAll() {
    std::lock_guard<std::mutex> l(m_);
    events_.clear();
    sessions_.clear();
}

}
