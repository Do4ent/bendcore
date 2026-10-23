#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>

namespace bendcore {

/**
 * Event categories for analytics tracking
 */
enum class EventCategory {
  IMPORT,           // CAD import events (STEP, DXF)
  TEMPLATE,         // Template operations (search, match, create)
  SEARCH,           // Search & filtering
  FORMULA,          // Parametric dimension creation
  TUBE_DESIGN,      // Tube design operations
  EXPORT,           // Output/export operations
  SYSTEM,           // System events (session start/end, errors)
};

/**
 * Analytics event structure
 * Represents a single user action to be tracked
 */
struct AnalyticsEvent {
  std::string event_name;                          // "import_step_success", "template_match_requested", etc.
  EventCategory category;                          // Event category
  std::map<std::string, std::string> properties;  // Custom event properties {od: "25.4", material: "Steel"}
  std::string user_id;                            // Anonymous user ID or email hash
  std::string session_id;                         // Session identifier
  std::chrono::system_clock::time_point timestamp; // Event timestamp
  std::string version;                            // BendCore version

  /**
   * Convert event to JSON for transmission
   */
  nlohmann::json to_json() const;
};

/**
 * Analytics Event Queue
 *
 * Collects events and manages batching/flushing
 * Provides background thread for periodic flushing
 */
class AnalyticsEventQueue {
public:
  AnalyticsEventQueue();
  ~AnalyticsEventQueue();

  /**
   * Track a new event
   */
  void track_event(const AnalyticsEvent& event);

  /**
   * Flush pending events (send to GrowthBook)
   */
  void flush();

  /**
   * Get current queue size
   */
  size_t get_queue_size() const;

  /**
   * Get time since last flush
   */
  std::chrono::milliseconds get_last_flush_age() const;

  /**
   * Start background flush thread
   */
  void start_background_flush(std::chrono::milliseconds flush_interval);

  /**
   * Stop background flush thread
   */
  void stop_background_flush();

  /**
   * Check if background flushing is active
   */
  bool is_background_flush_active() const;

  /**
   * Get all queued events (for testing)
   */
  std::vector<AnalyticsEvent> get_events() const;

private:
  std::vector<AnalyticsEvent> event_queue_;
  mutable std::mutex mutex_;
  std::thread flush_thread_;
  bool background_flush_active_ = false;
  std::chrono::system_clock::time_point last_flush_;

  void background_flush_loop(std::chrono::milliseconds flush_interval);
};

/**
 * Helper functions to create common event types
 */
namespace events {

// Import events
AnalyticsEvent import_step_opened(const std::string& user_id, const std::string& session_id);
AnalyticsEvent import_step_success(const std::string& user_id, const std::string& session_id,
                                   const std::map<std::string, std::string>& props);
AnalyticsEvent import_step_error(const std::string& user_id, const std::string& session_id,
                                 const std::string& error_msg);

// Template events
AnalyticsEvent template_match_requested(const std::string& user_id, const std::string& session_id);
AnalyticsEvent template_match_success(const std::string& user_id, const std::string& session_id,
                                      const std::map<std::string, std::string>& props);
AnalyticsEvent template_selected(const std::string& user_id, const std::string& session_id,
                                 const std::string& template_id);

// Search events
AnalyticsEvent search_opened(const std::string& user_id, const std::string& session_id);
AnalyticsEvent search_query(const std::string& user_id, const std::string& session_id,
                            const std::string& query);

// Formula events
AnalyticsEvent formula_created(const std::string& user_id, const std::string& session_id);
AnalyticsEvent formula_evaluated(const std::string& user_id, const std::string& session_id);
AnalyticsEvent formula_error(const std::string& user_id, const std::string& session_id,
                             const std::string& error_msg);

// Design events
AnalyticsEvent tube_created(const std::string& user_id, const std::string& session_id);
AnalyticsEvent tube_saved(const std::string& user_id, const std::string& session_id);
AnalyticsEvent bend_added(const std::string& user_id, const std::string& session_id);

// Export events
AnalyticsEvent export_dwf(const std::string& user_id, const std::string& session_id);
AnalyticsEvent export_pdf(const std::string& user_id, const std::string& session_id);
AnalyticsEvent export_step(const std::string& user_id, const std::string& session_id);

// System events
AnalyticsEvent session_start(const std::string& user_id);
AnalyticsEvent session_end(const std::string& user_id, const std::string& session_id);
AnalyticsEvent app_error(const std::string& user_id, const std::string& session_id,
                        const std::string& error_msg);

}  // namespace events

}  // namespace bendcore
