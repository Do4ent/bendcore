#include "bendcore/growthbook_client.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace bendcore {

GrowthBookClient& GrowthBookClient::getInstance() {
  static GrowthBookClient instance;
  return instance;
}

GrowthBookClient::GrowthBookClient() = default;

GrowthBookClient::~GrowthBookClient() = default;

void GrowthBookClient::initialize(const Config& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  config_ = config;

  // Load API keys from environment if not provided
  if (config_.api_key.empty()) {
    const char* env_api = std::getenv("GB_API_KEY");
    if (env_api) {
      config_.api_key = env_api;
    }
  }

  if (config_.sdk_key.empty()) {
    const char* env_sdk = std::getenv("GB_SDK_KEY");
    if (env_sdk) {
      config_.sdk_key = env_sdk;
    }
  }

  // Set default environment if not specified
  if (config_.environment.empty()) {
    config_.environment = "production";
  }

  initialized_ = !config_.api_key.empty() && !config_.sdk_key.empty();
  last_refresh_ = std::chrono::system_clock::now() - FEATURE_CACHE_TTL;

  if (initialized_) {
    // Generate default user ID
    user_id_ = "user_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    features_ = nlohmann::json::object();
  }
}

bool GrowthBookClient::is_initialized() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return initialized_;
}

bool GrowthBookClient::is_feature_enabled(const std::string& feature_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_) {
    return false;
  }

  // Check if feature exists in the features JSON
  if (features_.contains(feature_name)) {
    const auto& feature = features_[feature_name];
    if (feature.is_object() && feature.contains("enabled")) {
      return feature["enabled"].get<bool>();
    }
    // If it's just a boolean value, return it directly
    if (feature.is_boolean()) {
      return feature.get<bool>();
    }
  }

  return false;
}

std::string GrowthBookClient::get_feature_value(const std::string& feature_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_ || !features_.contains(feature_name)) {
    return "";
  }

  const auto& feature = features_[feature_name];

  // If it has a "value" field, return that
  if (feature.is_object() && feature.contains("value")) {
    return feature["value"].dump();
  }

  // Otherwise return the whole feature as JSON string
  return feature.dump();
}

const std::string& GrowthBookClient::get_api_key() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.api_key;
}

const std::string& GrowthBookClient::get_sdk_key() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.sdk_key;
}

const std::string& GrowthBookClient::get_environment() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.environment;
}

bool GrowthBookClient::is_analytics_enabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return initialized_ && config_.enable_analytics;
}

double GrowthBookClient::get_batch_delay_ms() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return config_.event_batch_delay_ms;
}

bool GrowthBookClient::refresh_features() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_) {
    return false;
  }

  // Check TTL
  auto now = std::chrono::system_clock::now();
  if (now - last_refresh_ < FEATURE_CACHE_TTL) {
    return true;  // Cache still valid
  }

  // In a real implementation, this would make an HTTP request to GrowthBook API
  // For now, we'll simulate a successful fetch
  features_ = nlohmann::json::object();
  last_refresh_ = now;

  return true;
}

const nlohmann::json& GrowthBookClient::get_features_json() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return features_;
}

void GrowthBookClient::set_user_context(const std::string& user_id,
                                        const nlohmann::json& attributes) {
  std::lock_guard<std::mutex> lock(mutex_);
  user_id_ = user_id;
  user_context_ = attributes;
}

const std::string& GrowthBookClient::get_user_id() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return user_id_;
}

}  // namespace bendcore
