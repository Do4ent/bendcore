#ifndef BENDCORE_COLLABORATION_MULTI_USER_MANAGER_HPP
#define BENDCORE_COLLABORATION_MULTI_USER_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <optional>
#include <atomic>
#include <shared_mutex>

namespace bendcore::collaboration {

/**
 * User session and presence information
 */
struct UserSession {
  std::string user_id;
  std::string session_id;
  std::string user_name;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_activity;
  std::string connection_id;
  bool is_active = true;
  std::map<std::string, std::string> metadata;
};

/**
 * Multi-user concurrent edit event
 */
struct ConcurrentEdit {
  std::string user_id;
  std::string session_id;
  std::chrono::system_clock::time_point timestamp;
  std::string operation_type;
  std::string target_id;
  std::string content;
  int version = 0;
};

/**
 * Session state snapshot
 */
struct SessionState {
  int version = 0;
  std::chrono::system_clock::time_point last_updated;
  std::vector<UserSession> active_users;
  std::vector<ConcurrentEdit> recent_edits;
  std::map<std::string, std::string> state_data;
};

/**
 * Callback types for session events
 */
using SessionEventCallback = std::function<void(const UserSession&)>;
using ConcurrentEditCallback = std::function<void(const ConcurrentEdit&)>;
using StateUpdateCallback = std::function<void(const SessionState&)>;

/**
 * MultiUserManager handles multi-user session management, state synchronization,
 * and concurrent user tracking. Provides APIs for creating sessions, tracking
 * active users, synchronizing state across users, and managing concurrent edits.
 *
 * Thread-safe implementation with read-write locks.
 */
class MultiUserManager {
 public:
  /**
   * Create a new MultiUserManager instance
   */
  MultiUserManager();

  /**
   * Destructor
   */
  ~MultiUserManager();

  /**
   * Create a new user session
   *
   * @param user_id Unique user identifier
   * @param user_name Display name for the user
   * @return New UserSession object
   */
  UserSession create_session(const std::string& user_id, const std::string& user_name);

  /**
   * Get session by ID
   *
   * @param session_id Session identifier
   * @return Optional UserSession if found
   */
  std::optional<UserSession> get_session(const std::string& session_id) const;

  /**
   * Get all active sessions
   *
   * @return Vector of active UserSession objects
   */
  std::vector<UserSession> get_active_sessions() const;

  /**
   * Get sessions for a specific user
   *
   * @param user_id User identifier
   * @return Vector of UserSession objects for that user
   */
  std::vector<UserSession> get_user_sessions(const std::string& user_id) const;

  /**
   * Update session activity (heartbeat)
   *
   * @param session_id Session identifier
   * @return True if session was updated, false if not found
   */
  bool update_session_activity(const std::string& session_id);

  /**
   * Mark session as inactive
   *
   * @param session_id Session identifier
   * @return True if session was deactivated, false if not found
   */
  bool deactivate_session(const std::string& session_id);

  /**
   * Close and remove a session
   *
   * @param session_id Session identifier
   * @return True if session was removed, false if not found
   */
  bool close_session(const std::string& session_id);

  /**
   * Record a concurrent edit operation
   *
   * @param edit The ConcurrentEdit to record
   */
  void record_edit(const ConcurrentEdit& edit);

  /**
   * Get recent edits (last N)
   *
   * @param limit Maximum number of edits to return (0 = all)
   * @return Vector of ConcurrentEdit objects
   */
  std::vector<ConcurrentEdit> get_recent_edits(size_t limit = 100) const;

  /**
   * Get edits by user
   *
   * @param user_id User identifier
   * @param limit Maximum number of edits to return (0 = all)
   * @return Vector of ConcurrentEdit objects
   */
  std::vector<ConcurrentEdit> get_user_edits(const std::string& user_id, size_t limit = 100) const;

  /**
   * Get edits for a specific target (e.g., document ID)
   *
   * @param target_id Target identifier
   * @param since_version Only get edits after this version
   * @return Vector of ConcurrentEdit objects
   */
  std::vector<ConcurrentEdit> get_target_edits(const std::string& target_id, int since_version = 0) const;

  /**
   * Synchronize session state
   *
   * @return Current SessionState snapshot
   */
  SessionState synchronize_state();

  /**
   * Update session state data
   *
   * @param key State key
   * @param value State value
   */
  void update_state(const std::string& key, const std::string& value);

  /**
   * Get session state
   *
   * @return Current SessionState
   */
  SessionState get_state() const;

  /**
   * Increment state version
   *
   * @return New version number
   */
  int increment_state_version();

  /**
   * Get current state version
   *
   * @return Current version number
   */
  int get_state_version() const;

  /**
   * Get number of active users
   *
   * @return Count of active users
   */
  size_t get_active_user_count() const;

  /**
   * Check if user has any active sessions
   *
   * @param user_id User identifier
   * @return True if user has at least one active session
   */
  bool is_user_active(const std::string& user_id) const;

  /**
   * Set session metadata
   *
   * @param session_id Session identifier
   * @param key Metadata key
   * @param value Metadata value
   * @return True if metadata was set, false if session not found
   */
  bool set_session_metadata(const std::string& session_id, const std::string& key, const std::string& value);

  /**
   * Get session metadata
   *
   * @param session_id Session identifier
   * @param key Metadata key
   * @return Optional metadata value if found
   */
  std::optional<std::string> get_session_metadata(const std::string& session_id, const std::string& key) const;

  /**
   * Clear all sessions (for testing)
   */
  void clear_sessions();

  /**
   * Clear all edits (for testing)
   */
  void clear_edits();

  /**
   * Register callback for session join events
   *
   * @param callback Function to call when user joins
   */
  void on_session_joined(SessionEventCallback callback);

  /**
   * Register callback for session leave events
   *
   * @param callback Function to call when user leaves
   */
  void on_session_left(SessionEventCallback callback);

  /**
   * Register callback for concurrent edits
   *
   * @param callback Function to call when edit is recorded
   */
  void on_edit_recorded(ConcurrentEditCallback callback);

  /**
   * Register callback for state updates
   *
   * @param callback Function to call when state is updated
   */
  void on_state_updated(StateUpdateCallback callback);

  /**
   * Get number of sessions with target_id
   *
   * @param target_id Target identifier
   * @return Number of sessions editing this target
   */
  int get_concurrent_edit_count(const std::string& target_id) const;

  /**
   * Get maximum concurrent edits to a target
   *
   * @return Maximum number of concurrent editors
   */
  int get_max_concurrent_editors() const;

 private:
  mutable std::shared_mutex sessions_mutex_;
  mutable std::shared_mutex edits_mutex_;
  mutable std::shared_mutex state_mutex_;

  std::map<std::string, UserSession> sessions_;
  std::map<std::string, std::vector<UserSession>> user_sessions_;
  std::vector<ConcurrentEdit> edits_;
  SessionState current_state_;
  std::atomic<int> state_version_{0};

  SessionEventCallback on_join_;
  SessionEventCallback on_leave_;
  ConcurrentEditCallback on_edit_;
  StateUpdateCallback on_state_update_;

  int max_concurrent_editors_ = 100;
  size_t max_edit_history_ = 10000;

  std::string generate_session_id() const;
  std::string generate_connection_id() const;
};

} // namespace bendcore::collaboration

#endif // BENDCORE_COLLABORATION_MULTI_USER_MANAGER_HPP
