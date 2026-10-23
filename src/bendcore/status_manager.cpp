/**
 * BendCore Status Manager Implementation
 * Manages application status updates and real-time data
 */

#include "status_manager.hpp"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace bendcore {

StatusManager::StatusManager() {
  state_.last_update = std::chrono::system_clock::now();
}

StatusManager::~StatusManager() = default;

const StatusState& StatusManager::get_state() const {
  return state_;
}

void StatusManager::set_state(const StatusState& state) {
  state_ = state;
  state_.last_update = std::chrono::system_clock::now();
  notify_status_change();
}

void StatusManager::set_step_hint(const std::string& hint) {
  if (state_.step_hint != hint) {
    state_.step_hint = hint;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_cursor_position(double x, double y, double z) {
  if (state_.cursor_x != x || state_.cursor_y != y || state_.cursor_z != z) {
    state_.cursor_x = x;
    state_.cursor_y = y;
    state_.cursor_z = z;
    state_.last_update = std::chrono::system_clock::now();
    // Throttle notifications for cursor updates (more frequent than other updates)
  }
}

void StatusManager::set_active_plane(const std::string& plane) {
  if (state_.active_plane != plane) {
    state_.active_plane = plane;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_snap_mode(bool enabled, const std::string& snap_type) {
  if (state_.snap_mode != enabled || state_.snap_type != snap_type) {
    state_.snap_mode = enabled;
    if (!snap_type.empty()) {
      state_.snap_type = snap_type;
    }
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_units(const std::string& units) {
  if (state_.units != units) {
    state_.units = units;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_standard(const std::string& standard) {
  if (state_.standard != standard) {
    state_.standard = standard;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_violations(int errors, int warnings, int info) {
  if (state_.violation_errors != errors || state_.violation_warnings != warnings ||
      state_.violation_info != info) {
    state_.violation_errors = errors;
    state_.violation_warnings = warnings;
    state_.violation_info = info;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::set_calculating(bool is_calculating) {
  if (state_.is_calculating != is_calculating) {
    state_.is_calculating = is_calculating;
    state_.last_update = std::chrono::system_clock::now();
    notify_status_change();
  }
}

void StatusManager::register_listener(const std::string& listener_id,
                                       StatusListener listener) {
  listeners_[listener_id] = listener;
}

void StatusManager::unregister_listener(const std::string& listener_id) {
  listeners_.erase(listener_id);
}

void StatusManager::notify_status_change() {
  for (auto& listener : listeners_) {
    listener.second(state_);
  }
}

json StatusManager::to_json() const {
  json j;
  j["stepHint"] = state_.step_hint;
  j["cursorX"] = state_.cursor_x;
  j["cursorY"] = state_.cursor_y;
  j["cursorZ"] = state_.cursor_z;
  j["activePlane"] = state_.active_plane;
  j["snapMode"] = state_.snap_mode;
  j["snapType"] = state_.snap_type;
  j["units"] = state_.units;
  j["standard"] = state_.standard;

  j["violations"] = json::object();
  j["violations"]["errors"] = state_.violation_errors;
  j["violations"]["warnings"] = state_.violation_warnings;
  j["violations"]["info"] = state_.violation_info;

  j["isCalculating"] = state_.is_calculating;

  // Format timestamp
  auto time_t = std::chrono::system_clock::to_time_t(state_.last_update);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  j["lastUpdate"] = oss.str();

  return j;
}

}  // namespace bendcore
