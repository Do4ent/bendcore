/**
 * Template Versioning System Implementation
 * Q4 Task 2: JSON Template System
 */

#include "bendcore/template_versioning.hpp"
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace bendcore {

// ============ SEMANTIC VERSION ============

std::optional<SemanticVersion> SemanticVersion::parse(const std::string& version_string) {
  SemanticVersion version;
  int parts[3] = {0, 0, 0};
  int part_count = 0;

  std::istringstream iss(version_string);
  std::string token;

  while (std::getline(iss, token, '.') && part_count < 3) {
    try {
      parts[part_count++] = std::stoi(token);
    } catch (...) {
      return std::nullopt;
    }
  }

  if (part_count > 0) {
    version.major = parts[0];
    if (part_count > 1) version.minor = parts[1];
    if (part_count > 2) version.patch = parts[2];
    return version;
  }

  return std::nullopt;
}

std::string SemanticVersion::to_string() const {
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
  if (major != other.major) return major < other.major;
  if (minor != other.minor) return minor < other.minor;
  return patch < other.patch;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
  return *this < other || *this == other;
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
  return other < *this;
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
  return other <= *this;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
  return major == other.major && minor == other.minor && patch == other.patch;
}

// ============ VERSION MANAGER ============

static std::string get_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

TemplateVersionManager::TemplateVersionManager() {
  // Инициализация миграций по умолчанию
  MigrationInfo migration_1_0_to_1_1;
  migration_1_0_to_1_1.from_version = "1.0";
  migration_1_0_to_1_1.to_version = "1.1";
  migration_1_0_to_1_1.is_automatic = true;
  migration_1_0_to_1_1.migration_instructions = "Automatic migration from 1.0 to 1.1";
  migration_info_.push_back(migration_1_0_to_1_1);
}

// === VERSION MANAGEMENT ===

bool TemplateVersionManager::create_new_version(
  const std::string& template_id,
  const JSONTemplate& tmpl,
  const std::string& release_notes,
  const std::string& publisher_id
) {
  TemplateVersion version;
  version.template_id = template_id;
  version.data = tmpl;
  version.release_notes = release_notes;
  version.published_at = get_timestamp();
  version.published_by = publisher_id;
  version.is_deprecated = false;

  // Определяем новый номер версии
  int next_minor = 0;
  for (const auto& v : versions_) {
    if (v.template_id == template_id) {
      auto parsed = SemanticVersion::parse(v.version);
      if (parsed.has_value()) {
        next_minor = std::max(next_minor, parsed.value().minor + 1);
      }
      v.is_latest = false;  // Старая версия больше не последняя
    }
  }

  version.version = "1." + std::to_string(next_minor);
  version.is_latest = true;

  versions_.push_back(version);
  return true;
}

std::optional<TemplateVersion> TemplateVersionManager::get_version(
  const std::string& template_id,
  const std::string& version
) const {
  const auto* v = find_version(template_id, version);
  if (v) {
    return *v;
  }
  return std::nullopt;
}

std::optional<TemplateVersion> TemplateVersionManager::get_latest_version(
  const std::string& template_id
) const {
  for (const auto& v : versions_) {
    if (v.template_id == template_id && v.is_latest) {
      return v;
    }
  }
  return std::nullopt;
}

std::vector<TemplateVersion> TemplateVersionManager::get_all_versions(
  const std::string& template_id
) const {
  std::vector<TemplateVersion> results;
  for (const auto& v : versions_) {
    if (v.template_id == template_id) {
      results.push_back(v);
    }
  }

  // Сортируем по версии (новые в начале)
  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    auto av = SemanticVersion::parse(a.version);
    auto bv = SemanticVersion::parse(b.version);
    if (av && bv) {
      return av.value() > bv.value();
    }
    return a.published_at > b.published_at;
  });

  return results;
}

std::vector<std::pair<std::string, std::string>> TemplateVersionManager::get_version_history(
  const std::string& template_id
) const {
  std::vector<std::pair<std::string, std::string>> history;
  auto versions = get_all_versions(template_id);

  for (const auto& v : versions) {
    history.push_back({v.version, v.release_notes});
  }

  return history;
}

// === MIGRATION ===

std::optional<JSONTemplate> TemplateVersionManager::migrate_to_version(
  const JSONTemplate& old_template,
  const std::string& from_version,
  const std::string& to_version
) {
  if (from_version == to_version) {
    return old_template;
  }

  // Проверяем, есть ли инструкции миграции
  auto migration = get_migration_info(from_version, to_version);
  if (!migration.has_value()) {
    return std::nullopt;
  }

  if (!migration.value().is_automatic) {
    return std::nullopt;  // Требуется ручная миграция
  }

  // Выполняем автоматическую миграцию
  JSONTemplate migrated = old_template;
  migrated.version = to_version;

  return migrated;
}

bool TemplateVersionManager::can_auto_migrate(
  const std::string& from_version,
  const std::string& to_version
) const {
  auto migration = get_migration_info(from_version, to_version);
  if (!migration.has_value()) {
    return false;
  }
  return migration.value().is_automatic;
}

std::optional<MigrationInfo> TemplateVersionManager::get_migration_info(
  const std::string& from_version,
  const std::string& to_version
) const {
  for (const auto& migration : migration_info_) {
    if (migration.from_version == from_version && migration.to_version == to_version) {
      return migration;
    }
  }
  return std::nullopt;
}

// === DEPRECATION ===

bool TemplateVersionManager::deprecate_version(
  const std::string& template_id,
  const std::string& version,
  const std::string& migration_guide
) {
  auto* v = find_version_mut(template_id, version);
  if (v == nullptr) {
    return false;
  }

  v->is_deprecated = true;
  v->migration_guide = migration_guide;
  return true;
}

std::vector<TemplateVersion> TemplateVersionManager::get_deprecated_versions(
  const std::string& template_id
) const {
  std::vector<TemplateVersion> results;
  for (const auto& v : versions_) {
    if (v.template_id == template_id && v.is_deprecated) {
      results.push_back(v);
    }
  }
  return results;
}

std::optional<std::string> TemplateVersionManager::get_deprecation_warning(
  const std::string& template_id,
  const std::string& version
) const {
  const auto* v = find_version(template_id, version);
  if (v && v->is_deprecated) {
    return "This version is deprecated. " + v->migration_guide;
  }
  return std::nullopt;
}

// === COMPATIBILITY ===

bool TemplateVersionManager::check_backward_compatibility(
  const TemplateVersion& old_version,
  const TemplateVersion& new_version
) const {
  // Проверяем основные поля
  if (old_version.template_data.constraints.outer_diameter.min !=
      new_version.template_data.constraints.outer_diameter.min) {
    return false;  // Изменено ограничение
  }

  if (old_version.template_data.constraints.outer_diameter.max !=
      new_version.template_data.constraints.outer_diameter.max) {
    return false;
  }

  // Если материалы изменились - это может быть проблемой
  if (old_version.template_data.manufacturing.materials !=
      new_version.template_data.manufacturing.materials) {
    return false;
  }

  return true;
}

std::vector<std::string> TemplateVersionManager::get_breaking_changes(
  const std::string& from_version,
  const std::string& to_version
) const {
  std::vector<std::string> changes;

  auto from_v = get_version("*", from_version);
  auto to_v = get_version("*", to_version);

  if (from_v.has_value() && to_v.has_value()) {
    if (!check_backward_compatibility(from_v.value(), to_v.value())) {
      changes.push_back("Constraint changes detected");
      changes.push_back("Manufacturing method or materials may have changed");
    }
  }

  auto migration = get_migration_info(from_version, to_version);
  if (migration.has_value()) {
    changes = migration.value().breaking_changes;
  }

  return changes;
}

// === STATISTICS ===

size_t TemplateVersionManager::get_version_count(const std::string& template_id) const {
  size_t count = 0;
  for (const auto& v : versions_) {
    if (v.template_id == template_id) {
      count++;
    }
  }
  return count;
}

std::optional<std::string> TemplateVersionManager::get_most_used_version(
  const std::string& template_id
) const {
  auto versions = get_all_versions(template_id);
  if (versions.empty()) {
    return std::nullopt;
  }

  // Пока просто возвращаем последнюю версию
  return versions[0].version;
}

// === PRIVATE HELPERS ===

TemplateVersion* TemplateVersionManager::find_version_mut(
  const std::string& id,
  const std::string& version
) {
  for (auto& v : versions_) {
    if (v.template_id == id && v.version == version) {
      return &v;
    }
  }
  return nullptr;
}

const TemplateVersion* TemplateVersionManager::find_version(
  const std::string& id,
  const std::string& version
) const {
  for (const auto& v : versions_) {
    if (v.template_id == id && v.version == version) {
      return &v;
    }
  }
  return nullptr;
}

bool TemplateVersionManager::perform_automatic_migration(JSONTemplate& tmpl) const {
  // Реализация автоматической миграции
  return true;
}

}  // namespace bendcore
