#pragma once
#include "../document/entity_id.hpp"
#include <string>

namespace BendCore::CadCore {
class SessionManager {
public:
    SessionManager(const EntityId& session_id) : session_id_(session_id), user_count_(0) {}
    bool startSession() noexcept;
    bool addUser([[maybe_unused]] const std::string& username) noexcept { user_count_++; return true; }
    [[nodiscard]] int getUserCount() const noexcept { return user_count_; }
    
private:
    EntityId session_id_;
    int user_count_;
};
}
