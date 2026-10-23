/**
 * BendCore Icon Theme Manager Implementation
 */

#include "icon_theme_manager.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace bendcore {

IconThemeManager::IconThemeManager() : current_preset_(ThemePreset::kLight) {
  init_light_theme();
}

void IconThemeManager::init_light_theme() {
  current_preset_ = ThemePreset::kLight;
  theme_name_ = "Light";
  colors_ = {
      {"tube", "#1F6FC5"},
      {"create", "#2E9E5B"},
      {"delete", "#D93A3A"},
      {"import", "#0E97A3"},
      {"links", "#8A4FD8"},
      {"edit", "#E8820C"},
      {"update", "#F5B517"},
      {"freeze", "#2D9CDB"},
      {"auxiliary", "#C9920A"},
      {"axes_x", "#FF0000"},
      {"axes_y", "#00FF00"},
      {"axes_z", "#0000FF"},
      {"neutral", "#757575"},
  };
}

void IconThemeManager::init_dark_theme() {
  current_preset_ = ThemePreset::kDark;
  theme_name_ = "Dark";
  colors_ = {
      {"tube", "#5BA3E3"},
      {"create", "#5BC55B"},
      {"delete", "#FF6B6B"},
      {"import", "#4ECAD4"},
      {"links", "#B792FF"},
      {"edit", "#FFB84D"},
      {"update", "#FFD93D"},
      {"freeze", "#6DBFFF"},
      {"auxiliary", "#FFD700"},
      {"axes_x", "#FF6666"},
      {"axes_y", "#66FF66"},
      {"axes_z", "#6666FF"},
      {"neutral", "#A0A0A0"},
  };
}

void IconThemeManager::init_high_contrast_theme() {
  current_preset_ = ThemePreset::kHighContrast;
  theme_name_ = "High Contrast";
  colors_ = {
      {"tube", "#0033FF"},
      {"create", "#00CC00"},
      {"delete", "#FF0000"},
      {"import", "#00CCCC"},
      {"links", "#FF00FF"},
      {"edit", "#FF6600"},
      {"update", "#FFFF00"},
      {"freeze", "#0088FF"},
      {"auxiliary", "#FF8800"},
      {"axes_x", "#FF0000"},
      {"axes_y", "#00FF00"},
      {"axes_z", "#0000FF"},
      {"neutral", "#000000"},
  };
}

void IconThemeManager::init_colorblind_theme() {
  current_preset_ = ThemePreset::kColorblind;
  theme_name_ = "Colorblind Safe";
  colors_ = {
      {"tube", "#0173B2"},
      {"create", "#DE8F05"},
      {"delete", "#CC78BC"},
      {"import", "#CA9161"},
      {"links", "#029E73"},
      {"edit", "#ECE133"},
      {"update", "#56B4E9"},
      {"freeze", "#009E73"},
      {"auxiliary", "#F0E442"},
      {"axes_x", "#E69F00"},
      {"axes_y", "#56B4E9"},
      {"axes_z", "#0173B2"},
      {"neutral", "#999999"},
  };
}

void IconThemeManager::set_preset(ThemePreset preset) {
  switch (preset) {
    case ThemePreset::kLight:
      init_light_theme();
      break;
    case ThemePreset::kDark:
      init_dark_theme();
      break;
    case ThemePreset::kHighContrast:
      init_high_contrast_theme();
      break;
    case ThemePreset::kColorblind:
      init_colorblind_theme();
      break;
    case ThemePreset::kCustom:
      current_preset_ = ThemePreset::kCustom;
      theme_name_ = "Custom";
      break;
  }
}

ThemePreset IconThemeManager::get_current_preset() const {
  return current_preset_;
}

std::string IconThemeManager::get_color(const std::string& category) const {
  auto it = colors_.find(category);
  if (it != colors_.end()) {
    return it->second;
  }
  return colors_.at("neutral");  // Default to neutral
}

ThemeColors IconThemeManager::get_colors() const {
  return ThemeColors{
      get_color("tube"),
      get_color("create"),
      get_color("delete"),
      get_color("import"),
      get_color("links"),
      get_color("edit"),
      get_color("update"),
      get_color("freeze"),
      get_color("auxiliary"),
      get_color("axes_x"),
      get_color("axes_y"),
      get_color("axes_z"),
      get_color("neutral"),
  };
}

json IconThemeManager::get_colors_json() const {
  json colors = json::object();
  for (const auto& [key, value] : colors_) {
    colors[key] = value;
  }
  return colors;
}

std::string IconThemeManager::get_theme_name() const {
  return theme_name_;
}

void IconThemeManager::set_custom_color(const std::string& category,
                                        const std::string& color) {
  if (!is_valid_hex_color(color)) {
    throw std::invalid_argument("Invalid hex color: " + color);
  }
  colors_[category] = color;
  current_preset_ = ThemePreset::kCustom;
  theme_name_ = "Custom";
}

bool IconThemeManager::load_from_json(const std::string& json_string) {
  try {
    json theme = json::parse(json_string);

    if (theme.contains("colors") && theme["colors"].is_object()) {
      for (auto& [key, value] : theme["colors"].items()) {
        if (is_valid_hex_color(value.get<std::string>())) {
          colors_[key] = value.get<std::string>();
        }
      }
    }

    if (theme.contains("name")) {
      theme_name_ = theme["name"].get<std::string>();
    }

    if (theme.contains("preset")) {
      std::string preset_str = theme["preset"].get<std::string>();
      if (preset_str == "light") {
        current_preset_ = ThemePreset::kLight;
      } else if (preset_str == "dark") {
        current_preset_ = ThemePreset::kDark;
      } else if (preset_str == "highContrast") {
        current_preset_ = ThemePreset::kHighContrast;
      } else if (preset_str == "colorblind") {
        current_preset_ = ThemePreset::kColorblind;
      } else {
        current_preset_ = ThemePreset::kCustom;
      }
    }

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error loading theme from JSON: " << e.what() << std::endl;
    return false;
  }
}

std::string IconThemeManager::export_to_json() const {
  json theme;
  theme["name"] = theme_name_;

  // Map preset enum to string
  switch (current_preset_) {
    case ThemePreset::kLight:
      theme["preset"] = "light";
      break;
    case ThemePreset::kDark:
      theme["preset"] = "dark";
      break;
    case ThemePreset::kHighContrast:
      theme["preset"] = "highContrast";
      break;
    case ThemePreset::kColorblind:
      theme["preset"] = "colorblind";
      break;
    case ThemePreset::kCustom:
      theme["preset"] = "custom";
      break;
  }

  theme["colors"] = get_colors_json();
  return theme.dump(2);
}

bool IconThemeManager::is_valid_hex_color(const std::string& color) {
  if (color.length() != 7) {
    return false;
  }

  if (color[0] != '#') {
    return false;
  }

  for (size_t i = 1; i < color.length(); ++i) {
    if (!std::isxdigit(color[i])) {
      return false;
    }
  }

  return true;
}

std::string IconThemeManager::rgb_to_hex(uint8_t r, uint8_t g, uint8_t b) {
  std::stringstream ss;
  ss << "#" << std::hex << std::setfill('0') << std::setw(2) << (int)r
     << std::setw(2) << (int)g << std::setw(2) << (int)b;
  std::string result = ss.str();
  // Convert to uppercase
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

bool IconThemeManager::hex_to_rgb(const std::string& hex, uint8_t& r,
                                   uint8_t& g, uint8_t& b) {
  if (!is_valid_hex_color(hex)) {
    return false;
  }

  std::stringstream ss;
  unsigned int color;
  ss << std::hex << hex.substr(1);
  ss >> color;

  r = (color >> 16) & 0xFF;
  g = (color >> 8) & 0xFF;
  b = color & 0xFF;

  return true;
}

std::string IconThemeManager::adjust_brightness(const std::string& hex,
                                                 int percent) {
  uint8_t r, g, b;
  if (!hex_to_rgb(hex, r, g, b)) {
    return hex;
  }

  // Adjust brightness
  float factor = 1.0f + (percent / 100.0f);
  r = std::min(255, (int)(r * factor));
  g = std::min(255, (int)(g * factor));
  b = std::min(255, (int)(b * factor));

  return rgb_to_hex(r, g, b);
}

}  // namespace bendcore
