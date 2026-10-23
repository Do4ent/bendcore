/**
 * JSON Template Loader for BendCore
 * Q4 Task 2: JSON Template System
 * Loads, validates, and manages template JSON files
 * C++20, Windows x64 (MSVC)
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include "json_template_format.hpp"

namespace bendcore {

/**
 * Validation result with error and warning information
 * Результат валидации с информацией об ошибках и предупреждениях
 */
struct ValidationResult {
  bool is_valid = true;
  std::vector<std::string> errors;    // Критические ошибки
  std::vector<std::string> warnings;  // Предупреждения (не блокируют загрузку)

  // Удобный метод для проверки
  bool has_errors() const { return !errors.empty(); }
  bool has_warnings() const { return !warnings.empty(); }
};

/**
 * JSON Template Loader
 * Загружает и валидирует JSON шаблоны из файлов
 */
class JSONTemplateLoader {
 public:
  /**
   * Загрузить один шаблон из файла
   * Load single template from JSON file
   */
  static std::optional<JSONTemplate> load_from_file(const std::string& file_path);

  /**
   * Загрузить все шаблоны из директории
   * Load all templates from directory (recursively)
   */
  static std::vector<JSONTemplate> load_all_from_directory(
    const std::string& directory_path
  );

  /**
   * Валидировать шаблон по JSON Schema
   * Validate template against schema
   */
  static ValidationResult validate_template(const JSONTemplate& tmpl);

  /**
   * Валидировать JSON строку по схеме
   * Validate JSON string against schema
   */
  static ValidationResult validate_json(const std::string& json_string);

  /**
   * Загрузить и валидировать одновременно
   * Load and validate a file in one call
   */
  static std::optional<JSONTemplate> load_and_validate(
    const std::string& file_path,
    ValidationResult* out_result = nullptr
  );

  /**
   * Проверить версию схемы и совместимость
   * Check schema version and backward compatibility
   */
  static bool check_version_compatibility(const std::string& version);

  /**
   * Мигрировать старый формат в новый (if needed)
   * Migrate old template format to new version
   */
  static std::optional<JSONTemplate> migrate_template(
    const std::string& old_version,
    const JSONTemplate& tmpl
  );

 private:
  static constexpr const char* CURRENT_VERSION = "1.0";
  static constexpr const char* MIN_SUPPORTED_VERSION = "1.0";

  // Валидация отдельных полей
  static ValidationResult validate_metadata(const JSONTemplate::Metadata& metadata);
  static ValidationResult validate_constraints(const JSONTemplate::Constraints& constraints);
  static ValidationResult validate_manufacturing(const JSONTemplate::Manufacturing& mfg);
};

/**
 * Template Repository - управляет кэшем загруженных шаблонов
 * Manages loaded templates cache and provides fast lookup
 */
class TemplateRepository {
 public:
  TemplateRepository() = default;

  /**
   * Загрузить шаблоны из директории в репозиторий
   * Load templates directory into memory
   */
  bool load_directory(const std::string& directory_path);

  /**
   * Получить шаблон по ID
   * Get template by ID from cache
   */
  std::optional<JSONTemplate> get_template(const std::string& id) const;

  /**
   * Получить все шаблоны по категории
   * Get all templates in category
   */
  std::vector<JSONTemplate> get_templates_by_category(const std::string& category) const;

  /**
   * Поиск по тегам
   * Search templates by tags
   */
  std::vector<JSONTemplate> search_by_tags(const std::vector<std::string>& tags) const;

  /**
   * Получить все шаблоны
   * Get all loaded templates
   */
  const std::vector<JSONTemplate>& get_all_templates() const {
    return templates_;
  }

  /**
   * Количество загруженных шаблонов
   * Number of loaded templates
   */
  size_t count() const { return templates_.size(); }

  /**
   * Очистить кэш
   * Clear cache
   */
  void clear() { templates_.clear(); }

 private:
  std::vector<JSONTemplate> templates_;
};

}  // namespace bendcore
