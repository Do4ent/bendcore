/**
 * JSON Template Loader Implementation
 * Q4 Task 2: JSON Template System
 */

#include "bendcore/json_template_loader.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace bendcore {

// ============ VALIDATION ============

ValidationResult JSONTemplateLoader::validate_metadata(
  const JSONTemplate::Metadata& metadata
) {
  ValidationResult result;

  // Проверка обязательных полей
  if (metadata.author.empty()) {
    result.errors.push_back("Metadata: author cannot be empty");
  }
  if (metadata.category.empty()) {
    result.errors.push_back("Metadata: category cannot be empty");
  }

  // Валидная категория?
  static const std::vector<std::string> valid_categories = {
    "standard", "reinforced", "custom", "large-assembly"
  };
  if (std::find(valid_categories.begin(), valid_categories.end(), metadata.category)
      == valid_categories.end()) {
    result.warnings.push_back(
      "Metadata: unknown category '" + metadata.category + "'"
    );
  }

  // Проверка популярности
  if (metadata.popularity < 0 || metadata.popularity > 100) {
    result.errors.push_back("Metadata: popularity must be 0-100");
  }

  return result;
}

ValidationResult JSONTemplateLoader::validate_constraints(
  const JSONTemplate::Constraints& constraints
) {
  ValidationResult result;

  // Проверка диапазонов
  if (constraints.outer_diameter.min < 0 || constraints.outer_diameter.max < 0) {
    result.errors.push_back("Constraints: outer_diameter values cannot be negative");
  }
  if (constraints.outer_diameter.min > constraints.outer_diameter.max) {
    result.errors.push_back("Constraints: outer_diameter min > max");
  }

  if (constraints.wall_thickness.min < 0 || constraints.wall_thickness.max < 0) {
    result.errors.push_back("Constraints: wall_thickness values cannot be negative");
  }
  if (constraints.wall_thickness.min > constraints.wall_thickness.max) {
    result.errors.push_back("Constraints: wall_thickness min > max");
  }

  // Bend radius ratio должен быть >= 1.0
  if (constraints.bend_radius_ratio.min < 1.0) {
    result.warnings.push_back("Constraints: bend_radius_ratio.min < 1.0 (unusual)");
  }
  if (constraints.bend_radius_ratio.min > constraints.bend_radius_ratio.max) {
    result.errors.push_back("Constraints: bend_radius_ratio min > max");
  }

  // Максимальный угол изгиба
  if (constraints.max_bend_angle <= 0 || constraints.max_bend_angle > 360) {
    result.errors.push_back("Constraints: max_bend_angle must be between 0 and 360");
  }

  // Максимум последовательных изгибов
  if (constraints.max_consecutive_bends < 1) {
    result.errors.push_back("Constraints: max_consecutive_bends must be >= 1");
  }

  if (constraints.min_bend_spacing < 0) {
    result.errors.push_back("Constraints: min_bend_spacing cannot be negative");
  }

  return result;
}

ValidationResult JSONTemplateLoader::validate_manufacturing(
  const JSONTemplate::Manufacturing& mfg
) {
  ValidationResult result;

  if (mfg.materials.empty()) {
    result.warnings.push_back("Manufacturing: no materials specified");
  }

  if (mfg.method.empty()) {
    result.warnings.push_back("Manufacturing: method not specified");
  }

  if (mfg.setup_time_min < 0) {
    result.errors.push_back("Manufacturing: setup_time_min cannot be negative");
  }

  if (mfg.cost_per_unit < 0) {
    result.errors.push_back("Manufacturing: cost_per_unit cannot be negative");
  }

  if (mfg.min_batch_size < 1) {
    result.errors.push_back("Manufacturing: min_batch_size must be >= 1");
  }

  return result;
}

ValidationResult JSONTemplateLoader::validate_template(const JSONTemplate& tmpl) {
  ValidationResult result;

  // Проверка основных полей
  if (tmpl.id.empty()) {
    result.errors.push_back("Template ID cannot be empty");
  }
  if (tmpl.name.empty()) {
    result.errors.push_back("Template name cannot be empty");
  }
  if (tmpl.version.empty()) {
    result.errors.push_back("Template version cannot be empty");
  }

  // Версия поддерживаеся?
  if (!check_version_compatibility(tmpl.version)) {
    result.errors.push_back(
      "Template version '" + tmpl.version + "' is not supported"
    );
  }

  // Валидируем подструктуры
  auto metadata_result = validate_metadata(tmpl.metadata);
  result.errors.insert(result.errors.end(),
    metadata_result.errors.begin(), metadata_result.errors.end());
  result.warnings.insert(result.warnings.end(),
    metadata_result.warnings.begin(), metadata_result.warnings.end());

  auto constraints_result = validate_constraints(tmpl.constraints);
  result.errors.insert(result.errors.end(),
    constraints_result.errors.begin(), constraints_result.errors.end());
  result.warnings.insert(result.warnings.end(),
    constraints_result.warnings.begin(), constraints_result.warnings.end());

  auto manufacturing_result = validate_manufacturing(tmpl.manufacturing);
  result.errors.insert(result.errors.end(),
    manufacturing_result.errors.begin(), manufacturing_result.errors.end());
  result.warnings.insert(result.warnings.end(),
    manufacturing_result.warnings.begin(), manufacturing_result.warnings.end());

  result.is_valid = result.errors.empty();
  return result;
}

ValidationResult JSONTemplateLoader::validate_json(const std::string& json_string) {
  ValidationResult result;

  try {
    auto j = json::parse(json_string);
    JSONTemplate tmpl = j.get<JSONTemplate>();
    return validate_template(tmpl);
  } catch (const std::exception& e) {
    result.is_valid = false;
    result.errors.push_back("JSON parse error: " + std::string(e.what()));
    return result;
  }
}

// ============ LOADING ============

std::optional<JSONTemplate> JSONTemplateLoader::load_from_file(
  const std::string& file_path
) {
  try {
    std::ifstream file(file_path);
    if (!file.is_open()) {
      return std::nullopt;
    }

    json j;
    file >> j;
    file.close();

    auto tmpl = j.get<JSONTemplate>();
    return tmpl;
  } catch (const std::exception& e) {
    return std::nullopt;
  }
}

std::vector<JSONTemplate> JSONTemplateLoader::load_all_from_directory(
  const std::string& directory_path
) {
  std::vector<JSONTemplate> templates;

  try {
    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
      return templates;
    }

    for (const auto& entry : fs::recursive_directory_iterator(directory_path)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        if (auto tmpl = load_from_file(entry.path().string())) {
          templates.push_back(tmpl.value());
        }
      }
    }
  } catch (const std::exception& e) {
    // Игнорируем ошибки файловой системы
  }

  return templates;
}

std::optional<JSONTemplate> JSONTemplateLoader::load_and_validate(
  const std::string& file_path,
  ValidationResult* out_result
) {
  auto result = load_from_file(file_path);
  if (!result.has_value()) {
    if (out_result) {
      out_result->is_valid = false;
      out_result->errors.push_back("Failed to load template file");
    }
    return std::nullopt;
  }

  auto validation = validate_template(result.value());
  if (out_result) {
    *out_result = validation;
  }

  if (!validation.is_valid) {
    return std::nullopt;
  }

  return result;
}

// ============ VERSION COMPATIBILITY ============

bool JSONTemplateLoader::check_version_compatibility(const std::string& version) {
  // Для версии 1.0 - 1.x совместимо
  if (version.find("1.") == 0) {
    return true;
  }
  return false;
}

std::optional<JSONTemplate> JSONTemplateLoader::migrate_template(
  const std::string& old_version,
  const JSONTemplate& tmpl
) {
  // Пока миграция не требуется, все версии 1.x совместимы
  if (old_version.find("1.") == 0) {
    return tmpl;
  }

  return std::nullopt;
}

// ============ TEMPLATE REPOSITORY ============

bool TemplateRepository::load_directory(const std::string& directory_path) {
  templates_ = JSONTemplateLoader::load_all_from_directory(directory_path);
  return !templates_.empty();
}

std::optional<JSONTemplate> TemplateRepository::get_template(
  const std::string& id
) const {
  for (const auto& tmpl : templates_) {
    if (tmpl.id == id) {
      return tmpl;
    }
  }
  return std::nullopt;
}

std::vector<JSONTemplate> TemplateRepository::get_templates_by_category(
  const std::string& category
) const {
  std::vector<JSONTemplate> result;
  for (const auto& tmpl : templates_) {
    if (tmpl.metadata.category == category) {
      result.push_back(tmpl);
    }
  }
  return result;
}

std::vector<JSONTemplate> TemplateRepository::search_by_tags(
  const std::vector<std::string>& tags
) const {
  std::vector<JSONTemplate> result;

  for (const auto& tmpl : templates_) {
    for (const auto& tag : tags) {
      if (std::find(tmpl.metadata.tags.begin(), tmpl.metadata.tags.end(), tag)
          != tmpl.metadata.tags.end()) {
        result.push_back(tmpl);
        break;  // Добавили шаблон один раз
      }
    }
  }

  return result;
}

}  // namespace bendcore
