/**
 * BendCore Status Manager
 * Manages application status updates and real-time data
 * Phase 32: Professional UI
 */

#ifndef BENDCORE_STATUS_MANAGER_HPP
#define BENDCORE_STATUS_MANAGER_HPP

#include <string>
#include <functional>
#include <chrono>

namespace bendcore {

/**
 * Status bar state
 */
struct StatusState {
  std::string step_hint = "Ready";
  double cursor_x = 0.0;
  double cursor_y = 0.0;
  double cursor_z = 0.0;
  std::string active_plane = "XY Plane";
  bool snap_mode = false;
  std::string snap_type = "grid";
  std::string units = "mm";
  std::string standard = "ISO";
  int violation_errors = 0;
  int violation_warnings = 0;
  int violation_info = 0;
  bool is_calculating = false;
  std::chrono::system_clock::time_point last_update;
};

/**
 * Status manager for real-time status updates
 */
class StatusManager {
 public:
  StatusManager();
  ~StatusManager();

  // State management
  const StatusState& get_state() const;
  void set_state(const StatusState& state);

  // Individual updates
  void set_step_hint(const std::string& hint);
  void set_cursor_position(double x, double y, double z);
  void set_active_plane(const std::string& plane);
  void set_snap_mode(bool enabled, const std::string& snap_type = "");
  void set_units(const std::string& units);
  void set_standard(const std::string& standard);
  void set_violations(int errors, int warnings, int info);
  void set_calculating(bool is_calculating);

  // Status listeners
  using StatusListener = std::function<void(const StatusState&)>;
  void register_listener(const std::string& listener_id, StatusListener listener);
  void unregister_listener(const std::string& listener_id);

  // Notify listeners
  void notify_status_change();

  // Convert to JSON for IPC
  nlohmann::json to_json() const;

 private:
  StatusState state_;
  std::map<std::string, StatusListener> listeners_;
};

}  // namespace bendcore

#endif  // BENDCORE_STATUS_MANAGER_HPP
