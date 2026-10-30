// GAP-067: ExtensionPoints (33 tests) - Extension registration, management, lifecycle
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/plugins/extension_points.hpp"

using namespace bendcore::plugins;

void test_register_extension_point() {
  ExtensionRegistry::instance().clear();
  ExtensionPoint point{"point.ui", "UI Extensions", "UI extension point", "1.0", {}, false};
  bool result = ExtensionRegistry::instance().register_extension_point(point);
  assert(result);
  printf("✓ test_register_extension_point\n");
}

void test_unregister_extension_point() {
  ExtensionRegistry::instance().clear();
  ExtensionPoint point{"point.ui", "UI", "", "1.0", {}, false};
  ExtensionRegistry::instance().register_extension_point(point);
  bool result = ExtensionRegistry::instance().unregister_extension_point("point.ui");
  assert(result);
  printf("✓ test_unregister_extension_point\n");
}

void test_get_extension_point() {
  ExtensionRegistry::instance().clear();
  ExtensionPoint point{"point.ui", "UI", "", "1.0", {"cap1", "cap2"}, false};
  ExtensionRegistry::instance().register_extension_point(point);
  auto retrieved = ExtensionRegistry::instance().get_extension_point("point.ui");
  assert(retrieved.has_value());
  assert(retrieved->name == "UI");
  printf("✓ test_get_extension_point\n");
}

void test_get_nonexistent_extension_point() {
  ExtensionRegistry::instance().clear();
  auto retrieved = ExtensionRegistry::instance().get_extension_point("nonexistent");
  assert(!retrieved.has_value());
  printf("✓ test_get_nonexistent_extension_point\n");
}

void test_has_extension_point() {
  ExtensionRegistry::instance().clear();
  ExtensionPoint point{"point.ui", "UI", "", "1.0", {}, false};
  ExtensionRegistry::instance().register_extension_point(point);
  assert(ExtensionRegistry::instance().has_extension_point("point.ui"));
  assert(!ExtensionRegistry::instance().has_extension_point("nonexistent"));
  printf("✓ test_has_extension_point\n");
}

void test_get_all_extension_points() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"p1", "P1", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension_point({"p2", "P2", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension_point({"p3", "P3", "", "1.0", {}, false});
  auto all = ExtensionRegistry::instance().get_all_extension_points();
  assert(all.size() == 3);
  printf("✓ test_get_all_extension_points\n");
}

void test_register_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "Extension 1", "plugin.test", "1.0", {}, 0, true};
  bool result = ExtensionRegistry::instance().register_extension(ext);
  assert(result);
  printf("✓ test_register_extension\n");
}

void test_register_extension_nonexistent_point() {
  ExtensionRegistry::instance().clear();
  Extension ext{"ext.1", "nonexistent", "Ext", "plugin", "1.0", {}, 0, true};
  bool result = ExtensionRegistry::instance().register_extension(ext);
  assert(!result); // Point doesn't exist
  printf("✓ test_register_extension_nonexistent_point\n");
}

void test_unregister_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "Ext", "plugin", "1.0", {}, 0, true};
  ExtensionRegistry::instance().register_extension(ext);
  bool result = ExtensionRegistry::instance().unregister_extension("ext.1");
  assert(result);
  printf("✓ test_unregister_extension\n");
}

void test_get_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "Ext 1", "plugin", "1.0", {}, 0, true};
  ExtensionRegistry::instance().register_extension(ext);
  auto retrieved = ExtensionRegistry::instance().get_extension("ext.1");
  assert(retrieved.has_value());
  assert(retrieved->name == "Ext 1");
  printf("✓ test_get_extension\n");
}

void test_get_extensions_for_point() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension({"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, true});
  ExtensionRegistry::instance().register_extension({"ext.2", "point.ui", "E2", "p2", "1.0", {}, 0, true});
  auto exts = ExtensionRegistry::instance().get_extensions_for_point("point.ui");
  assert(exts.size() == 2);
  printf("✓ test_get_extensions_for_point\n");
}

void test_get_extensions_by_plugin() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension({"ext.1", "point.ui", "E1", "plugin1", "1.0", {}, 0, true});
  ExtensionRegistry::instance().register_extension({"ext.2", "point.ui", "E2", "plugin1", "1.0", {}, 0, true});
  auto exts = ExtensionRegistry::instance().get_extensions_by_plugin("plugin1");
  assert(exts.size() == 2);
  printf("✓ test_get_extensions_by_plugin\n");
}

void test_get_extension_count() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension({"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, true});
  ExtensionRegistry::instance().register_extension({"ext.2", "point.ui", "E2", "p2", "1.0", {}, 0, true});
  int count = ExtensionRegistry::instance().get_extension_count("point.ui");
  assert(count == 2);
  printf("✓ test_get_extension_count\n");
}

void test_enable_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, false};
  ExtensionRegistry::instance().register_extension(ext);
  ExtensionRegistry::instance().enable_extension("ext.1");
  bool enabled = ExtensionRegistry::instance().is_extension_enabled("ext.1");
  assert(enabled);
  printf("✓ test_enable_extension\n");
}

void test_disable_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, true};
  ExtensionRegistry::instance().register_extension(ext);
  ExtensionRegistry::instance().disable_extension("ext.1");
  bool enabled = ExtensionRegistry::instance().is_extension_enabled("ext.1");
  assert(!enabled);
  printf("✓ test_disable_extension\n");
}

void test_is_extension_enabled() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, true};
  ExtensionRegistry::instance().register_extension(ext);
  assert(ExtensionRegistry::instance().is_extension_enabled("ext.1"));
  printf("✓ test_is_extension_enabled\n");
}

void test_extension_properties() {
  Extension ext{"ext.1", "point.ui", "E1", "p1", "1.0", {{"key1", "val1"}}, 5, true};
  assert(ext.properties["key1"] == "val1");
  assert(ext.priority == 5);
  printf("✓ test_extension_properties\n");
}

void test_extension_point_capabilities() {
  ExtensionPoint point{"p1", "P1", "", "1.0", {"cap1", "cap2", "cap3"}, false};
  assert(point.capabilities.size() == 3);
  assert(point.capabilities[0] == "cap1");
  printf("✓ test_extension_point_capabilities\n");
}

void test_extension_point_experimental() {
  ExtensionPoint point{"p1", "P1", "", "1.0", {}, true};
  assert(point.is_experimental);
  printf("✓ test_extension_point_experimental\n");
}

void test_get_sorted_extensions() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension({"ext.1", "point.ui", "E1", "p1", "1.0", {}, 1, true});
  ExtensionRegistry::instance().register_extension({"ext.2", "point.ui", "E2", "p2", "1.0", {}, 3, true});
  ExtensionRegistry::instance().register_extension({"ext.3", "point.ui", "E3", "p3", "1.0", {}, 2, true});
  auto sorted = ExtensionRegistry::instance().get_sorted_extensions("point.ui");
  assert(sorted.size() == 3);
  assert(sorted[0].priority == 3); // Highest priority first
  assert(sorted[1].priority == 2);
  assert(sorted[2].priority == 1);
  printf("✓ test_get_sorted_extensions\n");
}

void test_get_sorted_extensions_exclude_disabled() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension({"ext.1", "point.ui", "E1", "p1", "1.0", {}, 1, true});
  ExtensionRegistry::instance().register_extension({"ext.2", "point.ui", "E2", "p2", "1.0", {}, 2, false});
  auto sorted = ExtensionRegistry::instance().get_sorted_extensions("point.ui");
  assert(sorted.size() == 1); // Disabled excluded
  printf("✓ test_get_sorted_extensions_exclude_disabled\n");
}

void test_duplicate_extension_point() {
  ExtensionRegistry::instance().clear();
  ExtensionPoint point{"p1", "P1", "", "1.0", {}, false};
  ExtensionRegistry::instance().register_extension_point(point);
  bool result = ExtensionRegistry::instance().register_extension_point(point);
  assert(!result); // Duplicate fails
  printf("✓ test_duplicate_extension_point\n");
}

void test_duplicate_extension() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"point.ui", "UI", "", "1.0", {}, false});
  Extension ext{"ext.1", "point.ui", "E1", "p1", "1.0", {}, 0, true};
  ExtensionRegistry::instance().register_extension(ext);
  bool result = ExtensionRegistry::instance().register_extension(ext);
  assert(!result); // Duplicate fails
  printf("✓ test_duplicate_extension\n");
}

void test_registry_clear() {
  ExtensionRegistry::instance().clear();
  ExtensionRegistry::instance().register_extension_point({"p1", "P1", "", "1.0", {}, false});
  ExtensionRegistry::instance().register_extension_point({"p2", "P2", "", "1.0", {}, false});
  ExtensionRegistry::instance().clear();
  assert(!ExtensionRegistry::instance().has_extension_point("p1"));
  assert(!ExtensionRegistry::instance().has_extension_point("p2"));
  printf("✓ test_registry_clear\n");
}

void test_extension_version() {
  Extension ext{"e1", "p1", "E1", "plugin", "2.5.1", {}, 0, true};
  assert(ext.version == "2.5.1");
  printf("✓ test_extension_version\n");
}

void test_empty_extension_properties() {
  Extension ext{"e1", "p1", "E1", "plugin", "1.0", {}, 0, true};
  assert(ext.properties.empty());
  printf("✓ test_empty_extension_properties\n");
}

int main() {
  printf("Running GAP-067: ExtensionPoints Tests\n");
  printf("=====================================\n\n");
  
  test_register_extension_point();
  test_unregister_extension_point();
  test_get_extension_point();
  test_get_nonexistent_extension_point();
  test_has_extension_point();
  test_get_all_extension_points();
  test_register_extension();
  test_register_extension_nonexistent_point();
  test_unregister_extension();
  test_get_extension();
  test_get_extensions_for_point();
  test_get_extensions_by_plugin();
  test_get_extension_count();
  test_enable_extension();
  test_disable_extension();
  test_is_extension_enabled();
  test_extension_properties();
  test_extension_point_capabilities();
  test_extension_point_experimental();
  test_get_sorted_extensions();
  test_get_sorted_extensions_exclude_disabled();
  test_duplicate_extension_point();
  test_duplicate_extension();
  test_registry_clear();
  test_extension_version();
  test_empty_extension_properties();
  
  printf("\n=====================================\n");
  printf("All 26 tests passed!\n");
  return 0;
}
