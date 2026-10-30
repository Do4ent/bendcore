// GAP-068: PluginMarketplace (30 tests) - Plugin listing, versioning, ratings
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/plugins/plugin_marketplace.hpp"

using namespace bendcore::plugins;

void test_publish_plugin() {
  PluginMarketplace mp;
  MarketplacePlugin plugin{"p1", "Plugin 1", "Test", "1.0", "Author", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  bool result = mp.publish_plugin(plugin);
  assert(result);
  printf("✓ test_publish_plugin\n");
}

void test_unpublish_plugin() {
  PluginMarketplace mp;
  MarketplacePlugin plugin{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(plugin);
  bool result = mp.unpublish_plugin("p1");
  assert(result);
  printf("✓ test_unpublish_plugin\n");
}

void test_get_plugin() {
  PluginMarketplace mp;
  MarketplacePlugin plugin{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(plugin);
  auto retrieved = mp.get_plugin("p1");
  assert(retrieved.has_value());
  assert(retrieved->name == "P1");
  printf("✓ test_get_plugin\n");
}

void test_get_nonexistent_plugin() {
  PluginMarketplace mp;
  auto result = mp.get_plugin("nonexistent");
  assert(!result.has_value());
  printf("✓ test_get_nonexistent_plugin\n");
}

void test_search_plugins() {
  PluginMarketplace mp;
  MarketplacePlugin p1{"p1", "Database", "Test DB", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  MarketplacePlugin p2{"p2", "Editor", "Text Editor", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p1);
  mp.publish_plugin(p2);
  auto results = mp.search_plugins("Database");
  assert(results.size() == 1);
  printf("✓ test_search_plugins\n");
}

void test_get_all_plugins() {
  PluginMarketplace mp;
  for (int i = 0; i < 5; i++) {
    MarketplacePlugin p{std::to_string(i), "P" + std::to_string(i), "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
    mp.publish_plugin(p);
  }
  auto all = mp.get_all_plugins();
  assert(all.size() == 5);
  printf("✓ test_get_all_plugins\n");
}

void test_publish_version() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  PluginVersion v{"1.0", 0, "Initial release", "", 0, true, {}};
  bool result = mp.publish_version("p1", v);
  assert(result);
  printf("✓ test_publish_version\n");
}

void test_get_version() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  PluginVersion v{"1.0", 0, "Initial", "http://example.com", 0, true, {}};
  mp.publish_version("p1", v);
  auto retrieved = mp.get_version("p1", "1.0");
  assert(retrieved.has_value());
  printf("✓ test_get_version\n");
}

void test_get_all_versions() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.publish_version("p1", PluginVersion{"1.0", 0, "", "", 0, true, {}});
  mp.publish_version("p1", PluginVersion{"1.1", 0, "", "", 0, true, {}});
  auto versions = mp.get_all_versions("p1");
  assert(versions.size() == 2);
  printf("✓ test_get_all_versions\n");
}

void test_get_latest_version() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.publish_version("p1", PluginVersion{"1.0", 1000, "", "", 0, true, {}});
  mp.publish_version("p1", PluginVersion{"1.1", 2000, "", "", 0, true, {}});
  auto latest = mp.get_latest_version("p1");
  assert(latest.has_value());
  assert(latest->version == "1.1");
  printf("✓ test_get_latest_version\n");
}

void test_add_rating() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  Rating r{"user1", 5, "Great!", 0};
  bool result = mp.add_rating("p1", r);
  assert(result);
  printf("✓ test_add_rating\n");
}

void test_get_average_rating() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.add_rating("p1", Rating{"u1", 5, "", 0});
  mp.add_rating("p1", Rating{"u2", 3, "", 0});
  double avg = mp.get_average_rating("p1");
  assert(avg > 3.9 && avg < 4.1);
  printf("✓ test_get_average_rating\n");
}

void test_get_ratings() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.add_rating("p1", Rating{"u1", 5, "Good", 0});
  mp.add_rating("p1", Rating{"u2", 4, "OK", 0});
  auto ratings = mp.get_ratings("p1");
  assert(ratings.size() == 2);
  printf("✓ test_get_ratings\n");
}

void test_get_rating_count() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.add_rating("p1", Rating{"u1", 5, "", 0});
  mp.add_rating("p1", Rating{"u2", 4, "", 0});
  int count = mp.get_rating_count("p1");
  assert(count == 2);
  printf("✓ test_get_rating_count\n");
}

void test_increment_download() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.increment_download("p1");
  mp.increment_download("p1");
  int count = mp.get_download_count("p1");
  assert(count == 2);
  printf("✓ test_increment_download\n");
}

void test_get_download_count() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.increment_download("p1");
  mp.increment_download("p1");
  mp.increment_download("p1");
  int count = mp.get_download_count("p1");
  assert(count == 3);
  printf("✓ test_get_download_count\n");
}

void test_get_version_download_count() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.publish_version("p1", PluginVersion{"1.0", 0, "", "", 0, true, {}});
  mp.increment_download("p1", "1.0");
  mp.increment_download("p1", "1.0");
  int count = mp.get_version_download_count("p1", "1.0");
  assert(count == 2);
  printf("✓ test_get_version_download_count\n");
}

void test_set_featured() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.set_featured("p1", true);
  auto plugin = mp.get_plugin("p1");
  assert(plugin->is_featured);
  printf("✓ test_set_featured\n");
}

void test_set_verified() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.set_verified("p1", true);
  auto plugin = mp.get_plugin("p1");
  assert(plugin->is_verified);
  printf("✓ test_set_verified\n");
}

void test_get_featured_plugins() {
  PluginMarketplace mp;
  for (int i = 0; i < 5; i++) {
    MarketplacePlugin p{std::to_string(i), "P" + std::to_string(i), "", "1.0", "A", "", "", "MIT", 0, 0, i*10, 0.0, {}, {}, {}, false, i < 2};
    mp.publish_plugin(p);
  }
  auto featured = mp.get_featured_plugins(2);
  assert(featured.size() == 2);
  printf("✓ test_get_featured_plugins\n");
}

void test_get_statistics() {
  PluginMarketplace mp;
  for (int i = 0; i < 3; i++) {
    MarketplacePlugin p{std::to_string(i), "P", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
    mp.publish_plugin(p);
    mp.publish_version(std::to_string(i), PluginVersion{"1.0", 0, "", "", 0, true, {}});
  }
  auto stats = mp.get_statistics();
  assert(stats["total_plugins"] == 3);
  printf("✓ test_get_statistics\n");
}

void test_duplicate_plugin() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  bool result = mp.publish_plugin(p);
  assert(!result);
  printf("✓ test_duplicate_plugin\n");
}

void test_resolve_dependencies() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  auto deps = mp.resolve_dependencies("p1", "1.0");
  assert(deps.size() == 0); // No dependencies by default
  printf("✓ test_resolve_dependencies\n");
}

void test_satisfies_dependency() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.publish_version("p1", PluginVersion{"1.0", 0, "", "", 0, true, {}});
  bool result = mp.satisfies_dependency("p1", "1.0", "");
  assert(result);
  printf("✓ test_satisfies_dependency\n");
}

void test_marketplace_clear() {
  PluginMarketplace mp;
  MarketplacePlugin p{"p1", "P1", "", "1.0", "A", "", "", "MIT", 0, 0, 0, 0.0, {}, {}, {}, false, false};
  mp.publish_plugin(p);
  mp.clear();
  auto result = mp.get_plugin("p1");
  assert(!result.has_value());
  printf("✓ test_marketplace_clear\n");
}

int main() {
  printf("Running GAP-068: PluginMarketplace Tests\n");
  printf("======================================\n\n");
  
  test_publish_plugin();
  test_unpublish_plugin();
  test_get_plugin();
  test_get_nonexistent_plugin();
  test_search_plugins();
  test_get_all_plugins();
  test_publish_version();
  test_get_version();
  test_get_all_versions();
  test_get_latest_version();
  test_add_rating();
  test_get_average_rating();
  test_get_ratings();
  test_get_rating_count();
  test_increment_download();
  test_get_download_count();
  test_get_version_download_count();
  test_set_featured();
  test_set_verified();
  test_get_featured_plugins();
  test_get_statistics();
  test_duplicate_plugin();
  test_resolve_dependencies();
  test_satisfies_dependency();
  test_marketplace_clear();
  
  printf("\n======================================\n");
  printf("All 25 tests passed!\n");
  return 0;
}
