/**
 * Template Marketplace Implementation
 * Q4 Task 2: JSON Template System
 */

#include "bendcore/template_marketplace.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace bendcore {

// ============ HELPER FUNCTIONS ============

static std::string get_current_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// ============ MARKETPLACE IMPL ============

TemplateMarketplace::TemplateMarketplace() {}

// === PUBLISH & MANAGE ===

bool TemplateMarketplace::publish_template(
  const JSONTemplate& tmpl,
  const std::string& publisher_id
) {
  // Проверяем на дубликаты
  if (find_template(tmpl.id) != nullptr) {
    return false;  // Уже существует
  }

  MarketplaceTemplate marketplace_tmpl;
  marketplace_tmpl.template_data = tmpl;
  marketplace_tmpl.published_by = publisher_id;
  marketplace_tmpl.status = "pending";  // Требует модерации
  marketplace_tmpl.published_at = get_current_timestamp();
  marketplace_tmpl.download_count = 0;
  marketplace_tmpl.view_count = 0;
  marketplace_tmpl.average_rating = 0.0;

  templates_.push_back(marketplace_tmpl);
  return true;
}

bool TemplateMarketplace::update_template(const JSONTemplate& tmpl) {
  auto* marketplace_tmpl = find_template_mut(tmpl.id);
  if (marketplace_tmpl == nullptr) {
    return false;
  }

  marketplace_tmpl->template_data = tmpl;
  return true;
}

bool TemplateMarketplace::delete_template(
  const std::string& template_id,
  const std::string& user_id
) {
  auto it = std::find_if(templates_.begin(), templates_.end(),
    [&](const MarketplaceTemplate& t) { return t.template_data.id == template_id; }
  );

  if (it == templates_.end()) {
    return false;
  }

  // Проверяем право собственности
  if (it->published_by != user_id) {
    return false;
  }

  templates_.erase(it);
  return true;
}

// === SEARCH & FILTER ===

std::vector<MarketplaceTemplate> TemplateMarketplace::search_templates(
  const std::string& query,
  int limit
) const {
  std::vector<MarketplaceTemplate> results;
  std::string lower_query = query;
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

  for (const auto& tmpl : templates_) {
    if (tmpl.status != "approved") continue;

    // Поиск в названии и описании
    std::string lower_name = tmpl.template_data.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    std::string lower_desc = tmpl.template_data.description;
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

    if (lower_name.find(lower_query) != std::string::npos ||
        lower_desc.find(lower_query) != std::string::npos) {
      results.push_back(tmpl);
    }
  }

  // Сортируем по релеватности (содержит в названии = выше)
  std::sort(results.begin(), results.end(), [&](const auto& a, const auto& b) {
    bool a_in_name = a.template_data.name.find(query) != std::string::npos;
    bool b_in_name = b.template_data.name.find(query) != std::string::npos;
    if (a_in_name != b_in_name) return a_in_name;
    return a.download_count > b.download_count;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_by_category(
  const std::string& category,
  int limit
) const {
  std::vector<MarketplaceTemplate> results;

  for (const auto& tmpl : templates_) {
    if (tmpl.status == "approved" && tmpl.template_data.metadata.category == category) {
      results.push_back(tmpl);
    }
  }

  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.download_count > b.download_count;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_by_rating(
  double min_rating,
  double max_rating,
  int limit
) const {
  std::vector<MarketplaceTemplate> results;

  for (const auto& tmpl : templates_) {
    if (tmpl.status != "approved") continue;
    if (tmpl.average_rating >= min_rating && tmpl.average_rating <= max_rating) {
      results.push_back(tmpl);
    }
  }

  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.average_rating > b.average_rating;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_featured_templates(
  int limit
) const {
  std::vector<MarketplaceTemplate> results;

  for (const auto& tmpl : templates_) {
    if (tmpl.status == "approved" && tmpl.featured) {
      results.push_back(tmpl);
    }
  }

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_trending_templates(
  int limit
) const {
  std::vector<MarketplaceTemplate> results = templates_;

  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.download_count > b.download_count;
  });

  std::vector<MarketplaceTemplate> approved_results;
  for (const auto& tmpl : results) {
    if (tmpl.status == "approved") {
      approved_results.push_back(tmpl);
    }
  }

  if (approved_results.size() > limit) {
    approved_results.resize(limit);
  }

  return approved_results;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_recent_templates(
  int limit
) const {
  std::vector<MarketplaceTemplate> results;

  for (const auto& tmpl : templates_) {
    if (tmpl.status == "approved") {
      results.push_back(tmpl);
    }
  }

  // Сортируем по дате публикации (новые в начале)
  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.published_at > b.published_at;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

// === RATINGS & REVIEWS ===

bool TemplateMarketplace::rate_template(
  const std::string& template_id,
  const std::string& user_id,
  int stars,
  const std::string& review
) {
  if (stars < 1 || stars > 5) {
    return false;
  }

  auto* tmpl = find_template_mut(template_id);
  if (tmpl == nullptr) {
    return false;
  }

  // Добавляем рейтинг
  TemplateRating rating;
  rating.template_id = template_id;
  rating.user_id = user_id;
  rating.stars = stars;
  rating.review_text = review;
  rating.rated_at = get_current_timestamp();

  ratings_.push_back(rating);

  // Обновляем среднюю оценку
  tmpl->average_rating = recalculate_average_rating(template_id);
  return true;
}

std::vector<TemplateRating> TemplateMarketplace::get_template_ratings(
  const std::string& template_id
) const {
  std::vector<TemplateRating> results;
  for (const auto& rating : ratings_) {
    if (rating.template_id == template_id) {
      results.push_back(rating);
    }
  }
  return results;
}

double TemplateMarketplace::get_average_rating(const std::string& template_id) const {
  return recalculate_average_rating(template_id);
}

// === INSTALLATION ===

bool TemplateMarketplace::install_template(const std::string& template_id) {
  auto tmpl = find_template(template_id);
  if (tmpl == nullptr) {
    return false;
  }

  // Проверяем не установлен ли уже
  if (is_installed(template_id)) {
    return false;
  }

  installed_templates_.push_back(template_id);

  // Увеличиваем счётчик загрузок
  auto* marketplace_tmpl = find_template_mut(template_id);
  if (marketplace_tmpl) {
    marketplace_tmpl->download_count++;
  }

  return true;
}

std::vector<JSONTemplate> TemplateMarketplace::get_installed_templates(
  const std::string& user_id
) const {
  // Простая реализация - возвращаем все установленные шаблоны
  std::vector<JSONTemplate> results;
  for (const auto& template_id : installed_templates_) {
    if (auto tmpl = find_template(template_id)) {
      results.push_back(tmpl->template_data);
    }
  }
  return results;
}

bool TemplateMarketplace::is_installed(const std::string& template_id) const {
  return std::find(installed_templates_.begin(), installed_templates_.end(), template_id)
    != installed_templates_.end();
}

// === STATISTICS ===

std::optional<MarketplaceTemplate> TemplateMarketplace::get_template_details(
  const std::string& template_id
) const {
  auto tmpl = find_template(template_id);
  if (tmpl == nullptr) {
    return std::nullopt;
  }
  return *tmpl;
}

void TemplateMarketplace::increment_view_count(const std::string& template_id) {
  auto* tmpl = find_template_mut(template_id);
  if (tmpl) {
    tmpl->view_count++;
  }
}

std::vector<std::pair<std::string, int>> TemplateMarketplace::get_top_publishers(
  int limit
) const {
  std::map<std::string, int> publisher_counts;

  for (const auto& tmpl : templates_) {
    publisher_counts[tmpl.published_by]++;
  }

  std::vector<std::pair<std::string, int>> results(
    publisher_counts.begin(), publisher_counts.end()
  );

  std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
    return a.second > b.second;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

// === MODERATION ===

bool TemplateMarketplace::approve_template(const std::string& template_id) {
  auto* tmpl = find_template_mut(template_id);
  if (tmpl == nullptr) {
    return false;
  }
  tmpl->status = "approved";
  return true;
}

bool TemplateMarketplace::reject_template(
  const std::string& template_id,
  const std::string& reason
) {
  auto* tmpl = find_template_mut(template_id);
  if (tmpl == nullptr) {
    return false;
  }
  tmpl->status = "rejected";
  if (!tmpl->reviews.empty()) {
    tmpl->reviews[0] = "Rejection reason: " + reason;
  } else {
    tmpl->reviews.push_back("Rejection reason: " + reason);
  }
  return true;
}

bool TemplateMarketplace::set_featured(const std::string& template_id, bool featured) {
  auto* tmpl = find_template_mut(template_id);
  if (tmpl == nullptr) {
    return false;
  }
  tmpl->featured = featured;
  return true;
}

std::vector<MarketplaceTemplate> TemplateMarketplace::get_pending_templates() const {
  std::vector<MarketplaceTemplate> results;
  for (const auto& tmpl : templates_) {
    if (tmpl.status == "pending") {
      results.push_back(tmpl);
    }
  }
  return results;
}

// === PRIVATE HELPERS ===

MarketplaceTemplate* TemplateMarketplace::find_template_mut(const std::string& id) {
  for (auto& tmpl : templates_) {
    if (tmpl.template_data.id == id) {
      return &tmpl;
    }
  }
  return nullptr;
}

const MarketplaceTemplate* TemplateMarketplace::find_template(const std::string& id) const {
  for (const auto& tmpl : templates_) {
    if (tmpl.template_data.id == id) {
      return &tmpl;
    }
  }
  return nullptr;
}

double TemplateMarketplace::recalculate_average_rating(const std::string& template_id) {
  auto ratings = get_template_ratings(template_id);
  if (ratings.empty()) {
    return 0.0;
  }

  double sum = 0.0;
  for (const auto& rating : ratings) {
    sum += rating.stars;
  }

  return sum / ratings.size();
}

}  // namespace bendcore
