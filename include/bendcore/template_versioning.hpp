/**
 * Template Versioning System for BendCore
 * Q4 Task 2: JSON Template System
 * Manages template versions with semantic versioning and migration
 * C++20, Windows x64 (MSVC)
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include "json_template_format.hpp"

namespace bendcore {

/**
 * Semantic version helper
 * Парсер и сравнение семантических версий (1.0, 1.1, 2.0)
 */
struct SemanticVersion {
  int major = 1;
  int minor = 0;
  int patch = 0;

  /**
   * Создать версию из строки "1.0.0"
   */
  static std::optional<SemanticVersion> parse(const std::string& version_string);

  /**
   * Конвертировать в строку
   */
  std::string to_string() const;

  /**
   * Сравнение версий
   */
  bool operator<(const SemanticVersion& other) const;
  bool operator<=(const SemanticVersion& other) const;
  bool operator>(const SemanticVersion& other) const;
  bool operator>=(const SemanticVersion& other) const;
  bool operator==(const SemanticVersion& other) const;
};

/**
 * Template Version - конкретная версия шаблона в истории
 * A specific version of a template in history
 */
struct TemplateVersion {
  std::string template_id;              // ID базового шаблона
  std::string version;                  // Семантическая версия ("1.0", "1.1", "2.0")
  JSONTemplate data;                    // Данные шаблона в этой версии
  std::string release_notes;            // Заметки о выпуске
  std::string published_at;             // Дата публикации (ISO 8601)
  bool is_latest = false;               // Это последняя версия?
  std::string published_by;             // Автор версии

  // Информация о совместимости
  bool is_deprecated = false;           // Устарела ли версия?
  std::string migration_guide;          // Руководство миграции на новую версию
};

/**
 * Migration Info - информация для миграции между версиями
 * Information for migrating between versions
 */
struct MigrationInfo {
  std::string from_version;
  std::string to_version;
  bool is_automatic = false;            // Возможна ли автоматическая миграция?
  std::string migration_instructions;   // Инструкции для миграции
  std::vector<std::string> breaking_changes;  // Ломающие изменения
};

/**
 * Template Version Manager
 * Управляет версионированием шаблонов и миграцией
 */
class TemplateVersionManager {
 public:
  TemplateVersionManager();

  // === VERSION MANAGEMENT ===

  /**
   * Создать новую версию шаблона
   * Create new version of template
   */
  bool create_new_version(
    const std::string& template_id,
    const JSONTemplate& tmpl,
    const std::string& release_notes,
    const std::string& publisher_id
  );

  /**
   * Получить конкретную версию
   * Get specific template version
   */
  std::optional<TemplateVersion> get_version(
    const std::string& template_id,
    const std::string& version
  ) const;

  /**
   * Получить последнюю версию
   * Get latest version of template
   */
  std::optional<TemplateVersion> get_latest_version(
    const std::string& template_id
  ) const;

  /**
   * Получить все версии шаблона
   * Get all versions of template
   */
  std::vector<TemplateVersion> get_all_versions(
    const std::string& template_id
  ) const;

  /**
   * Получить историю версий (с метаданными)
   * Get version history
   */
  std::vector<std::pair<std::string, std::string>> get_version_history(
    const std::string& template_id
  ) const;

  // === MIGRATION ===

  /**
   * Мигрировать шаблон на новую версию
   * Migrate template to new version
   */
  std::optional<JSONTemplate> migrate_to_version(
    const JSONTemplate& old_template,
    const std::string& from_version,
    const std::string& to_version
  );

  /**
   * Проверить возможность автоматической миграции
   * Check if automatic migration is possible
   */
  bool can_auto_migrate(
    const std::string& from_version,
    const std::string& to_version
  ) const;

  /**
   * Получить инструкции миграции
   * Get migration instructions
   */
  std::optional<MigrationInfo> get_migration_info(
    const std::string& from_version,
    const std::string& to_version
  ) const;

  // === DEPRECATION ===

  /**
   * Пометить версию как устаревшую
   * Mark version as deprecated
   */
  bool deprecate_version(
    const std::string& template_id,
    const std::string& version,
    const std::string& migration_guide
  );

  /**
   * Получить список устаревших версий
   * Get deprecated versions
   */
  std::vector<TemplateVersion> get_deprecated_versions(
    const std::string& template_id
  ) const;

  /**
   * Получить предупреждение об использовании устаревшей версии
   * Get deprecation warning
   */
  std::optional<std::string> get_deprecation_warning(
    const std::string& template_id,
    const std::string& version
  ) const;

  // === COMPATIBILITY ===

  /**
   * Проверить обратную совместимость
   * Check backward compatibility
   */
  bool check_backward_compatibility(
    const TemplateVersion& old_version,
    const TemplateVersion& new_version
  ) const;

  /**
   * Получить информацию о ломающих изменениях
   * Get breaking changes information
   */
  std::vector<std::string> get_breaking_changes(
    const std::string& from_version,
    const std::string& to_version
  ) const;

  // === STATISTICS ===

  /**
   * Получить количество версий для шаблона
   * Get number of versions for template
   */
  size_t get_version_count(const std::string& template_id) const;

  /**
   * Получить самую популярную версию (по использованию)
   * Get most popular version
   */
  std::optional<std::string> get_most_used_version(
    const std::string& template_id
  ) const;

 private:
  std::vector<TemplateVersion> versions_;
  std::vector<MigrationInfo> migration_info_;

  // Вспомогательные методы
  TemplateVersion* find_version_mut(const std::string& id, const std::string& version);
  const TemplateVersion* find_version(const std::string& id, const std::string& version) const;
  bool perform_automatic_migration(JSONTemplate& tmpl) const;
};

}  // namespace bendcore
