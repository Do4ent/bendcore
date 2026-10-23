#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>

namespace bendcore {

/**
 * GrowthBook Analytics Client
 *
 * Provides feature flag evaluation, A/B testing variants, and analytics event tracking.
 * Thread-safe singleton for application-wide analytics.
 */
class GrowthBookClient {
public:
  struct Config {
    std::string api_key;                          // GrowthBook API key from environment
    std::string sdk_key;                          // SDK key for feature flags
    std::string environment;                      // "development", "staging", "production"
    bool enable_analytics = true;                 // Enable event tracking
    bool track_page_views = false;                // Track page view events
    double event_batch_delay_ms = 5000.0;        // Batch events every 5 seconds
  };

  /**
   * Get singleton instance
   */
  static GrowthBookClient& getInstance();

  /**
   * Initialize GrowthBook client with configuration
   */
  void initialize(const Config& config);

  /**
   * Check if client is initialized
   */
  bool is_initialized() const;

  /**
   * Evaluate if a feature flag is enabled for current context
   */
  bool is_feature_enabled(const std::string& feature_name);

  /**
   * Get feature value/configuration
   */
  std::string get_feature_value(const std::string& feature_name);

  /**
   * Get GrowthBook API key
   */
  const std::string& get_api_key() const;

  /**
   * Get SDK key
   */
  const std::string& get_sdk_key() const;

  /**
   * Get environment (development/staging/production)
   */
  const std::string& get_environment() const;

  /**
   * Check if analytics is enabled
   */
  bool is_analytics_enabled() const;

  /**
   * Get event batch delay in milliseconds
   */
  double get_batch_delay_ms() const;

  /**
   * Load feature flags from GrowthBook API
   * @return true if successful, false if network error
   */
  bool refresh_features();

  /**
   * Get raw features JSON from GrowthBook
   */
  const nlohmann::json& get_features_json() const;

  /**
   * Set custom user context (user_id, attributes for targeting)
   */
  void set_user_context(const std::string& user_id,
                       const nlohmann::json& attributes = nlohmann::json::object());

  /**
   * Get user ID
   */
  const std::string& get_user_id() const;

private:
  GrowthBookClient();
  ~GrowthBookClient();

  // Prevent copy/move
  GrowthBookClient(const GrowthBookClient&) = delete;
  GrowthBookClient& operator=(const GrowthBookClient&) = delete;

  Config config_;
  bool initialized_ = false;
  mutable std::mutex mutex_;
  nlohmann::json features_;
  std::string user_id_;
  nlohmann::json user_context_;
  std::chrono::system_clock::time_point last_refresh_;
  static constexpr std::chrono::seconds FEATURE_CACHE_TTL{300};  // 5 minutes
};

}  // namespace bendcore
