/**
 * BendCore Icon Theme Manager
 * Manages icon themes and color schemes
 */

#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

enum class ThemePreset {
  kLight,
  kDark,
  kHighContrast,
  kColorblind,
  kCustom,
};

struct ThemeColors {
  std::string tube;
  std::string create;
  std::string delete_color;
  std::string import_color;
  std::string links;
  std::string edit;
  std::string update;
  std::string freeze;
  std::string auxiliary;
  std::string axes_x;
  std::string axes_y;
  std::string axes_z;
  std::string neutral;
};

/**
 * Icon Theme Manager
 */
class IconThemeManager {
 public:
  static IconThemeManager& instance() {
    static IconThemeManager manager;
    return manager;
  }

  /**
   * Set theme preset
   */
  void set_preset(ThemePreset preset);

  /**
   * Get current theme preset
   */
  ThemePreset get_current_preset() const;

  /**
   * Get color for a category
   */
  std::string get_color(const std::string& category) const;

  /**
   * Get all colors for current theme
   */
  ThemeColors get_colors() const;

  /**
   * Get color as JSON object
   */
  json get_colors_json() const;

  /**
   * Get theme name
   */
  std::string get_theme_name() const;

  /**
   * Set custom color
   */
  void set_custom_color(const std::string& category, const std::string& color);

  /**
   * Load theme from JSON
   */
  bool load_from_json(const std::string& json_string);

  /**
   * Export theme as JSON
   */
  std::string export_to_json() const;

  /**
   * Check if color is valid hex
   */
  static bool is_valid_hex_color(const std::string& color);

  /**
   * Convert RGB to hex
   */
  static std::string rgb_to_hex(uint8_t r, uint8_t g, uint8_t b);

  /**
   * Convert hex to RGB
   */
  static bool hex_to_rgb(const std::string& hex, uint8_t& r, uint8_t& g,
                         uint8_t& b);

  /**
   * Adjust color brightness
   */
  static std::string adjust_brightness(const std::string& hex,
                                        int percent);

 private:
  IconThemeManager();

  /**
   * Initialize light theme
   */
  void init_light_theme();

  /**
   * Initialize dark theme
   */
  void init_dark_theme();

  /**
   * Initialize high contrast theme
   */
  void init_high_contrast_theme();

  /**
   * Initialize colorblind theme
   */
  void init_colorblind_theme();

  ThemePreset current_preset_;
  std::string theme_name_;
  std::map<std::string, std::string> colors_;
};

}  // namespace bendcore
