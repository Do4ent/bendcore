#include "bendcore/collaboration/multi_user_manager.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace bendcore::collaboration {

MultiUserManager::MultiUserManager()
    : current_state_{} {
  current_state_.last_updated = std::chrono::system_clock::now();
}

MultiUserManager::~MultiUserManager() = default;

UserSession MultiUserManager::create_session(const std::string& user_id, const std::string& user_name) {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

  UserSession session;
  session.user_id = user_id;
  session.session_id = generate_session_id();
  session.user_name = user_name;
  session.created_at = std::chrono::system_clock::now();
  session.last_activity = session.created_at;
  session.connection_id = generate_connection_id();
  session.is_active = true;

  sessions_[session.session_id] = session;
  user_sessions_[user_id].push_back(session);

  if (on_join_) {
    on_join_(session);
  }

  return session;
}

std::optional<UserSession> MultiUserManager::get_session(const std::string& session_id) const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<UserSession> MultiUserManager::get_active_sessions() const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  std::vector<UserSession> result;
  for (const auto& pair : sessions_) {
    if (pair.second.is_active) {
      result.push_back(pair.second);
    }
  }
  return result;
}

std::vector<UserSession> MultiUserManager::get_user_sessions(const std::string& user_id) const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = user_sessions_.find(user_id);
  if (it != user_sessions_.end()) {
    return it->second;
  }
  return {};
}

bool MultiUserManager::update_session_activity(const std::string& session_id) {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  it->second.last_activity = std::chrono::system_clock::now();
  return true;
}

bool MultiUserManager::deactivate_session(const std::string& session_id) {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  it->second.is_active = false;

  if (on_leave_) {
    on_leave_(it->second);
  }

  return true;
}

bool MultiUserManager::close_session(const std::string& session_id) {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  UserSession session = it->second;
  sessions_.erase(it);

  auto user_it = user_sessions_.find(session.user_id);
  if (user_it != user_sessions_.end()) {
    auto& sessions = user_it->second;
    sessions.erase(
        std::remove_if(sessions.begin(), sessions.end(),
                       [&session_id](const UserSession& s) { return s.session_id == session_id; }),
        sessions.end());
  }

  if (on_leave_) {
    on_leave_(session);
  }

  return true;
}

void MultiUserManager::record_edit(const ConcurrentEdit& edit) {
  {
    std::unique_lock<std::shared_mutex> lock(edits_mutex_);
    edits_.push_back(edit);

    // Maintain max history size
    if (edits_.size() > max_edit_history_) {
      edits_.erase(edits_.begin());
    }
  }

  if (on_edit_) {
    on_edit_(edit);
  }
}

std::vector<ConcurrentEdit> MultiUserManager::get_recent_edits(size_t limit) const {
  std::shared_lock<std::shared_mutex> lock(edits_mutex_);

  std::vector<ConcurrentEdit> result;
  size_t start = (limit == 0 || edits_.size() <= limit) ? 0 : edits_.size() - limit;

  for (size_t i = start; i < edits_.size(); ++i) {
    result.push_back(edits_[i]);
  }

  return result;
}

std::vector<ConcurrentEdit> MultiUserManager::get_user_edits(const std::string& user_id, size_t limit) const {
  std::shared_lock<std::shared_mutex> lock(edits_mutex_);

  std::vector<ConcurrentEdit> result;
  for (const auto& edit : edits_) {
    if (edit.user_id == user_id) {
      result.push_back(edit);
    }
  }

  if (limit > 0 && result.size() > limit) {
    result.erase(result.begin(), result.end() - limit);
  }

  return result;
}

std::vector<ConcurrentEdit> MultiUserManager::get_target_edits(const std::string& target_id, int since_version) const {
  std::shared_lock<std::shared_mutex> lock(edits_mutex_);

  std::vector<ConcurrentEdit> result;
  for (const auto& edit : edits_) {
    if (edit.target_id == target_id && edit.version >= since_version) {
      result.push_back(edit);
    }
  }

  return result;
}

SessionState MultiUserManager::synchronize_state() {
  std::shared_lock<std::shared_mutex> state_lock(state_mutex_);
  std::shared_lock<std::shared_mutex> session_lock(sessions_mutex_);

  SessionState state = current_state_;
  state.version = state_version_.load();
  state.last_updated = std::chrono::system_clock::now();
  state.active_users = get_active_sessions();
  state.recent_edits = get_recent_edits(100);

  return state;
}

void MultiUserManager::update_state(const std::string& key, const std::string& value) {
  {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    current_state_.state_data[key] = value;
    increment_state_version();
  }

  if (on_state_update_) {
    on_state_update_(synchronize_state());
  }
}

SessionState MultiUserManager::get_state() const {
  std::shared_lock<std::shared_mutex> lock(state_mutex_);
  return current_state_;
}

int MultiUserManager::increment_state_version() {
  return ++state_version_;
}

int MultiUserManager::get_state_version() const {
  return state_version_.load();
}

size_t MultiUserManager::get_active_user_count() const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  std::set<std::string> active_users;
  for (const auto& pair : sessions_) {
    if (pair.second.is_active) {
      active_users.insert(pair.second.user_id);
    }
  }
  return active_users.size();
}

bool MultiUserManager::is_user_active(const std::string& user_id) const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = user_sessions_.find(user_id);
  if (it == user_sessions_.end()) {
    return false;
  }

  for (const auto& session : it->second) {
    if (session.is_active) {
      return true;
    }
  }
  return false;
}

bool MultiUserManager::set_session_metadata(const std::string& session_id, const std::string& key, const std::string& value) {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }

  it->second.metadata[key] = value;
  return true;
}

std::optional<std::string> MultiUserManager::get_session_metadata(const std::string& session_id, const std::string& key) const {
  std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return std::nullopt;
  }

  auto meta_it = it->second.metadata.find(key);
  if (meta_it != it->second.metadata.end()) {
    return meta_it->second;
  }
  return std::nullopt;
}

void MultiUserManager::clear_sessions() {
  std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
  sessions_.clear();
  user_sessions_.clear();
}

void MultiUserManager::clear_edits() {
  std::unique_lock<std::shared_mutex> lock(edits_mutex_);
  edits_.clear();
}

void MultiUserManager::on_session_joined(SessionEventCallback callback) {
  on_join_ = callback;
}

void MultiUserManager::on_session_left(SessionEventCallback callback) {
  on_leave_ = callback;
}

void MultiUserManager::on_edit_recorded(ConcurrentEditCallback callback) {
  on_edit_ = callback;
}

void MultiUserManager::on_state_updated(StateUpdateCallback callback) {
  on_state_update_ = callback;
}

int MultiUserManager::get_concurrent_edit_count(const std::string& target_id) const {
  std::shared_lock<std::shared_mutex> lock(edits_mutex_);

  std::set<std::string> editors;
  for (const auto& edit : edits_) {
    if (edit.target_id == target_id) {
      editors.insert(edit.user_id);
    }
  }
  return static_cast<int>(editors.size());
}

int MultiUserManager::get_max_concurrent_editors() const {
  return max_concurrent_editors_;
}

std::string MultiUserManager::generate_session_id() const {
  uuid_t uuid;
  uuid_generate(uuid);

  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "sess_" + ss.str();
}

std::string MultiUserManager::generate_connection_id() const {
  uuid_t uuid;
  uuid_generate(uuid);

  std::stringstream ss;
  for (int i = 0; i < 16; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
  }
  return "conn_" + ss.str();
}

} // namespace bendcore::collaboration
