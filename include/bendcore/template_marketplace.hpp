/**
 * Template Marketplace API for BendCore
 * Q4 Task 2: JSON Template System
 * Provides marketplace functionality: publishing, searching, rating, installing
 * C++20, Windows x64 (MSVC)
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "json_template_format.hpp"

namespace bendcore {

/**
 * Marketplace Template - расширение JSONTemplate с информацией о маркетплейсе
 * Extended template data with marketplace metadata
 */
struct MarketplaceTemplate {
  JSONTemplate template_data;           // Основные данные шаблона
  int download_count = 0;               // Количество загрузок
  double average_rating = 0.0;          // Средняя оценка (0-5 звёзд)
  std::string status;                   // "approved", "pending", "rejected"
  std::string published_by;             // ID пользователя/команды автора
  std::vector<std::string> reviews;     // Текстовые отзывы

  // Дополнительные поля маркетплейса
  std::string published_at;             // Дата публикации
  bool featured = false;                // Избранный шаблон
  int view_count = 0;                   // Количество просмотров
};

/**
 * Rating - оценка шаблона пользователем
 * User rating for template
 */
struct TemplateRating {
  std::string template_id;
  std::string user_id;
  int stars;                            // 1-5
  std::string review_text;              // Текст отзыва
  std::string rated_at;                 // ISO 8601
};

/**
 * Template Marketplace - главный класс маркетплейса
 * Main marketplace API class
 */
class TemplateMarketplace {
 public:
  TemplateMarketplace();

  // === PUBLISH & MANAGE ===

  /**
   * Опубликовать новый шаблон в маркетплейсе
   * Publish new template to marketplace
   */
  bool publish_template(
    const JSONTemplate& tmpl,
    const std::string& publisher_id
  );

  /**
   * Обновить опубликованный шаблон
   * Update published template
   */
  bool update_template(const JSONTemplate& tmpl);

  /**
   * Удалить шаблон (только владелец)
   * Delete template (owner only)
   */
  bool delete_template(
    const std::string& template_id,
    const std::string& user_id
  );

  // === SEARCH & FILTER ===

  /**
   * Поиск по названию и описанию
   * Search templates by keyword
   */
  std::vector<MarketplaceTemplate> search_templates(
    const std::string& query,
    int limit = 20
  ) const;

  /**
   * Получить все шаблоны в категории
   * Get templates by category
   */
  std::vector<MarketplaceTemplate> get_by_category(
    const std::string& category,
    int limit = 20
  ) const;

  /**
   * Фильтр по параметрам
   * Get templates by rating range
   */
  std::vector<MarketplaceTemplate> get_by_rating(
    double min_rating,
    double max_rating = 5.0,
    int limit = 20
  ) const;

  /**
   * Получить избранные шаблоны
   * Get featured templates
   */
  std::vector<MarketplaceTemplate> get_featured_templates(int limit = 10) const;

  /**
   * Получить популярные (по загрузкам)
   * Get trending templates
   */
  std::vector<MarketplaceTemplate> get_trending_templates(int limit = 10) const;

  /**
   * Получить недавно добавленные
   * Get recently published templates
   */
  std::vector<MarketplaceTemplate> get_recent_templates(int limit = 10) const;

  // === RATINGS & REVIEWS ===

  /**
   * Оценить шаблон (1-5 звёзд)
   * Rate template
   */
  bool rate_template(
    const std::string& template_id,
    const std::string& user_id,
    int stars,
    const std::string& review = ""
  );

  /**
   * Получить все оценки шаблона
   * Get all ratings for template
   */
  std::vector<TemplateRating> get_template_ratings(const std::string& template_id) const;

  /**
   * Получить среднюю оценку
   * Get average rating
   */
  double get_average_rating(const std::string& template_id) const;

  // === INSTALLATION ===

  /**
   * Установить шаблон в локальную библиотеку
   * Install template to local library
   */
  bool install_template(const std::string& template_id);

  /**
   * Получить список установленных шаблонов пользователя
   * Get user's installed templates
   */
  std::vector<JSONTemplate> get_installed_templates(const std::string& user_id) const;

  /**
   * Проверить установлен ли шаблон
   * Check if template is installed
   */
  bool is_installed(const std::string& template_id) const;

  // === STATISTICS ===

  /**
   * Получить детальную информацию о шаблоне
   * Get template details with stats
   */
  std::optional<MarketplaceTemplate> get_template_details(const std::string& template_id) const;

  /**
   * Увеличить счётчик просмотров
   * Increment view counter
   */
  void increment_view_count(const std::string& template_id);

  /**
   * Получить топ авторов
   * Get top publishers
   */
  std::vector<std::pair<std::string, int>> get_top_publishers(int limit = 10) const;

  // === MODERATION ===

  /**
   * Одобрить шаблон (только модератор)
   * Approve template (moderator only)
   */
  bool approve_template(const std::string& template_id);

  /**
   * Отклонить шаблон с причиной
   * Reject template with reason
   */
  bool reject_template(
    const std::string& template_id,
    const std::string& reason
  );

  /**
   * Пометить как избранный
   * Mark as featured
   */
  bool set_featured(const std::string& template_id, bool featured);

  /**
   * Получить ожидающие модерации шаблоны
   * Get pending templates for moderation
   */
  std::vector<MarketplaceTemplate> get_pending_templates() const;

 private:
  std::vector<MarketplaceTemplate> templates_;
  std::vector<TemplateRating> ratings_;
  std::vector<std::string> installed_templates_;

  // Вспомогательные методы
  MarketplaceTemplate* find_template_mut(const std::string& id);
  const MarketplaceTemplate* find_template(const std::string& id) const;
  double recalculate_average_rating(const std::string& template_id);
};

}  // namespace bendcore
