/**
 * JSON Template Format Specification for BendCore
 * Q4 Task 2: JSON Template System
 * Defines the structure for template data with full metadata, constraints, and manufacturing info
 * C++20, Windows x64 (MSVC)
 */

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace bendcore {

/**
 * JSON Template Format - Version 1.0
 * Полная спецификация JSON шаблонов с метаданными, ограничениями и информацией о производстве
 */
struct JSONTemplate {
  // === VERSION & IDENTIFICATION ===
  std::string version = "1.0";        // Версия формата шаблона
  std::string id;                     // Уникальный идентификатор: "std-90-deg-v2"
  std::string name;                   // Название: "Standard 90° Bend v2"
  std::string description;            // Описание использования

  // === METADATA ===
  struct Metadata {
    std::string author;               // Автор шаблона
    std::string created_at;           // Дата создания (ISO 8601)
    std::string updated_at;           // Дата обновления
    std::string category;             // Категория: "standard", "reinforced", "custom"
    int popularity = 0;               // Популярность 0-100
    std::vector<std::string> tags;    // Теги для поиска
  } metadata;

  // === CONSTRAINTS ===
  struct Range {
    double min = 0.0;
    double max = 100.0;
  };

  struct Constraints {
    Range outer_diameter;             // Наружный диаметр (mm)
    Range wall_thickness;             // Толщина стенки (mm)
    Range bend_radius_ratio;          // Соотношение R/D (1.5-3.0)
    double max_bend_angle = 180.0;    // Максимальный угол изгиба (°)
    int max_consecutive_bends = 2;    // Максимум последовательных изгибов
    double min_bend_spacing = 10.0;   // Минимальное расстояние между изгибами (mm)
  } constraints;

  // === MANUFACTURING ===
  struct Manufacturing {
    std::vector<std::string> materials;    // Материалы: ["Steel", "Aluminum"]
    std::string method;                    // Метод: "3D CNC", "Hydraulic", "CNC 5-axis"
    double setup_time_min = 0.0;           // Время настройки (минуты)
    double cost_per_unit = 0.0;            // Стоимость за единицу
    int min_batch_size = 1;                // Минимальный размер партии
  } manufacturing;

  // === COMPATIBILITY & INFO ===
  std::vector<std::string> compatible_cad_formats;  // "STEP", "IGES", "DXF"
  std::string warranty_info;                         // Информация о гарантии
  std::vector<std::string> notes;                    // Дополнительные замечания

  // === SERIALIZATION ===
  friend void to_json(nlohmann::json& j, const JSONTemplate& t) {
    j = nlohmann::json{
      {"version", t.version},
      {"id", t.id},
      {"name", t.name},
      {"description", t.description},
      {"metadata", {
        {"author", t.metadata.author},
        {"created_at", t.metadata.created_at},
        {"updated_at", t.metadata.updated_at},
        {"category", t.metadata.category},
        {"popularity", t.metadata.popularity},
        {"tags", t.metadata.tags}
      }},
      {"constraints", {
        {"outer_diameter", {
          {"min", t.constraints.outer_diameter.min},
          {"max", t.constraints.outer_diameter.max}
        }},
        {"wall_thickness", {
          {"min", t.constraints.wall_thickness.min},
          {"max", t.constraints.wall_thickness.max}
        }},
        {"bend_radius_ratio", {
          {"min", t.constraints.bend_radius_ratio.min},
          {"max", t.constraints.bend_radius_ratio.max}
        }},
        {"max_bend_angle", t.constraints.max_bend_angle},
        {"max_consecutive_bends", t.constraints.max_consecutive_bends},
        {"min_bend_spacing", t.constraints.min_bend_spacing}
      }},
      {"manufacturing", {
        {"materials", t.manufacturing.materials},
        {"method", t.manufacturing.method},
        {"setup_time_min", t.manufacturing.setup_time_min},
        {"cost_per_unit", t.manufacturing.cost_per_unit},
        {"min_batch_size", t.manufacturing.min_batch_size}
      }},
      {"compatible_cad_formats", t.compatible_cad_formats},
      {"warranty_info", t.warranty_info},
      {"notes", t.notes}
    };
  }

  friend void from_json(const nlohmann::json& j, JSONTemplate& t) {
    if (j.contains("version")) j.at("version").get_to(t.version);
    if (j.contains("id")) j.at("id").get_to(t.id);
    if (j.contains("name")) j.at("name").get_to(t.name);
    if (j.contains("description")) j.at("description").get_to(t.description);

    if (j.contains("metadata")) {
      auto& m = j.at("metadata");
      if (m.contains("author")) m.at("author").get_to(t.metadata.author);
      if (m.contains("created_at")) m.at("created_at").get_to(t.metadata.created_at);
      if (m.contains("updated_at")) m.at("updated_at").get_to(t.metadata.updated_at);
      if (m.contains("category")) m.at("category").get_to(t.metadata.category);
      if (m.contains("popularity")) m.at("popularity").get_to(t.metadata.popularity);
      if (m.contains("tags")) m.at("tags").get_to(t.metadata.tags);
    }

    if (j.contains("constraints")) {
      auto& c = j.at("constraints");
      if (c.contains("outer_diameter")) {
        auto& od = c.at("outer_diameter");
        t.constraints.outer_diameter.min = od.at("min");
        t.constraints.outer_diameter.max = od.at("max");
      }
      if (c.contains("wall_thickness")) {
        auto& wt = c.at("wall_thickness");
        t.constraints.wall_thickness.min = wt.at("min");
        t.constraints.wall_thickness.max = wt.at("max");
      }
      if (c.contains("bend_radius_ratio")) {
        auto& br = c.at("bend_radius_ratio");
        t.constraints.bend_radius_ratio.min = br.at("min");
        t.constraints.bend_radius_ratio.max = br.at("max");
      }
      if (c.contains("max_bend_angle")) c.at("max_bend_angle").get_to(t.constraints.max_bend_angle);
      if (c.contains("max_consecutive_bends")) c.at("max_consecutive_bends").get_to(t.constraints.max_consecutive_bends);
      if (c.contains("min_bend_spacing")) c.at("min_bend_spacing").get_to(t.constraints.min_bend_spacing);
    }

    if (j.contains("manufacturing")) {
      auto& mfg = j.at("manufacturing");
      if (mfg.contains("materials")) mfg.at("materials").get_to(t.manufacturing.materials);
      if (mfg.contains("method")) mfg.at("method").get_to(t.manufacturing.method);
      if (mfg.contains("setup_time_min")) mfg.at("setup_time_min").get_to(t.manufacturing.setup_time_min);
      if (mfg.contains("cost_per_unit")) mfg.at("cost_per_unit").get_to(t.manufacturing.cost_per_unit);
      if (mfg.contains("min_batch_size")) mfg.at("min_batch_size").get_to(t.manufacturing.min_batch_size);
    }

    if (j.contains("compatible_cad_formats")) j.at("compatible_cad_formats").get_to(t.compatible_cad_formats);
    if (j.contains("warranty_info")) j.at("warranty_info").get_to(t.warranty_info);
    if (j.contains("notes")) j.at("notes").get_to(t.notes);
  }
};

}  // namespace bendcore
