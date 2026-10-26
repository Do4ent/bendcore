#include <gtest/gtest.h>
#include "bendcore/plugins/extension_points.hpp"

using namespace bendcore::plugins;

class ExtensionPointRegistryTest : public ::testing::Test {
protected:
    ExtensionPointRegistry& registry = ExtensionPointRegistry::GetInstance();

    void SetUp() override {
        registry.Clear();
    }
};

TEST_F(ExtensionPointRegistryTest, RegisterExtensionPoint) {
    ExtensionPoint point{
        .id = "test.point",
        .name = "Test Point",
        .description = "Test extension point",
        .category = "testing",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };

    EXPECT_TRUE(registry.RegisterExtensionPoint(point));
    EXPECT_TRUE(registry.HasExtensionPoint("test.point"));
}

TEST_F(ExtensionPointRegistryTest, RegisterDuplicateExtensionPoint) {
    ExtensionPoint point{
        .id = "duplicate.point",
        .name = "Duplicate",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };

    EXPECT_TRUE(registry.RegisterExtensionPoint(point));
    EXPECT_FALSE(registry.RegisterExtensionPoint(point));
}

TEST_F(ExtensionPointRegistryTest, UnregisterExtensionPoint) {
    ExtensionPoint point{
        .id = "remove.point",
        .name = "Remove",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };

    registry.RegisterExtensionPoint(point);
    EXPECT_TRUE(registry.UnregisterExtensionPoint("remove.point"));
    EXPECT_FALSE(registry.HasExtensionPoint("remove.point"));
}

TEST_F(ExtensionPointRegistryTest, GetExtensionPoint) {
    ExtensionPoint point{
        .id = "get.point",
        .name = "Get Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };

    registry.RegisterExtensionPoint(point);
    const auto* retrieved = registry.GetExtensionPoint("get.point");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "get.point");
    EXPECT_EQ(retrieved->name, "Get Point");
}

TEST_F(ExtensionPointRegistryTest, GetNonexistentExtensionPoint) {
    const auto* point = registry.GetExtensionPoint("nonexistent");
    EXPECT_EQ(point, nullptr);
}

TEST_F(ExtensionPointRegistryTest, GetAllExtensionPoints) {
    ExtensionPoint p1{
        .id = "point1",
        .name = "Point 1",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    ExtensionPoint p2{
        .id = "point2",
        .name = "Point 2",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };

    registry.RegisterExtensionPoint(p1);
    registry.RegisterExtensionPoint(p2);

    auto points = registry.GetAllExtensionPoints();
    EXPECT_EQ(points.size(), 2);
}

TEST_F(ExtensionPointRegistryTest, RegisterExtension) {
    ExtensionPoint point{
        .id = "ext.point",
        .name = "Extension Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext{
        .id = "test.ext",
        .plugin_id = "plugin.test",
        .extension_point = "ext.point",
        .name = "Test Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    EXPECT_TRUE(registry.RegisterExtension(ext));
    EXPECT_TRUE(registry.HasExtension("test.ext"));
}

TEST_F(ExtensionPointRegistryTest, RegisterExtensionToNonexistentPoint) {
    Extension ext{
        .id = "test.ext",
        .plugin_id = "plugin.test",
        .extension_point = "nonexistent.point",
        .name = "Test Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    EXPECT_FALSE(registry.RegisterExtension(ext));
}

TEST_F(ExtensionPointRegistryTest, UnregisterExtension) {
    ExtensionPoint point{
        .id = "ext.point",
        .name = "Extension Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext{
        .id = "test.ext",
        .plugin_id = "plugin.test",
        .extension_point = "ext.point",
        .name = "Test Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext);
    EXPECT_TRUE(registry.UnregisterExtension("test.ext"));
    EXPECT_FALSE(registry.HasExtension("test.ext"));
}

TEST_F(ExtensionPointRegistryTest, GetExtensionsForPoint) {
    ExtensionPoint point{
        .id = "ext.point",
        .name = "Extension Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext1{
        .id = "ext1",
        .plugin_id = "plugin1",
        .extension_point = "ext.point",
        .name = "Extension 1",
        .callback = [](const auto&) { return true; },
        .priority = 10,
        .enabled = true
    };
    Extension ext2{
        .id = "ext2",
        .plugin_id = "plugin2",
        .extension_point = "ext.point",
        .name = "Extension 2",
        .callback = [](const auto&) { return true; },
        .priority = 5,
        .enabled = true
    };

    registry.RegisterExtension(ext1);
    registry.RegisterExtension(ext2);

    auto exts = registry.GetExtensionsForPoint("ext.point");
    EXPECT_EQ(exts.size(), 2);
    // Should be sorted by priority
    EXPECT_EQ(exts[0].priority, 5);
    EXPECT_EQ(exts[1].priority, 10);
}

TEST_F(ExtensionPointRegistryTest, GetExtensionsByPlugin) {
    ExtensionPoint point{
        .id = "ext.point",
        .name = "Extension Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext1{
        .id = "ext1",
        .plugin_id = "plugin1",
        .extension_point = "ext.point",
        .name = "Extension 1",
        .callback = [](const auto&) { return true; },
        .priority = 10,
        .enabled = true
    };
    Extension ext2{
        .id = "ext2",
        .plugin_id = "plugin1",
        .extension_point = "ext.point",
        .name = "Extension 2",
        .callback = [](const auto&) { return true; },
        .priority = 20,
        .enabled = true
    };
    Extension ext3{
        .id = "ext3",
        .plugin_id = "plugin2",
        .extension_point = "ext.point",
        .name = "Extension 3",
        .callback = [](const auto&) { return true; },
        .priority = 30,
        .enabled = true
    };

    registry.RegisterExtension(ext1);
    registry.RegisterExtension(ext2);
    registry.RegisterExtension(ext3);

    auto exts = registry.GetExtensionsByPlugin("plugin1");
    EXPECT_EQ(exts.size(), 2);
}

TEST_F(ExtensionPointRegistryTest, ExecuteExtensions) {
    ExtensionPoint point{
        .id = "exec.point",
        .name = "Execute Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    int call_count = 0;
    Extension ext{
        .id = "exec.ext",
        .plugin_id = "plugin",
        .extension_point = "exec.point",
        .name = "Execute Extension",
        .callback = [&call_count](const auto&) {
            call_count++;
            return true;
        },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext);

    std::map<std::string, std::any> params;
    EXPECT_TRUE(registry.ExecuteExtensions("exec.point", params));
    EXPECT_EQ(call_count, 1);
}

TEST_F(ExtensionPointRegistryTest, ExecuteDisabledExtension) {
    ExtensionPoint point{
        .id = "disabled.point",
        .name = "Disabled Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    int call_count = 0;
    Extension ext{
        .id = "disabled.ext",
        .plugin_id = "plugin",
        .extension_point = "disabled.point",
        .name = "Disabled Extension",
        .callback = [&call_count](const auto&) {
            call_count++;
            return true;
        },
        .priority = 100,
        .enabled = false
    };

    registry.RegisterExtension(ext);

    std::map<std::string, std::any> params;
    registry.ExecuteExtensions("disabled.point", params);
    EXPECT_EQ(call_count, 0);
}

TEST_F(ExtensionPointRegistryTest, SetExtensionEnabled) {
    ExtensionPoint point{
        .id = "enable.point",
        .name = "Enable Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext{
        .id = "enable.ext",
        .plugin_id = "plugin",
        .extension_point = "enable.point",
        .name = "Enable Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext);
    EXPECT_TRUE(registry.SetExtensionEnabled("enable.ext", false));

    const auto* retrieved = registry.GetExtension("enable.ext");
    EXPECT_FALSE(retrieved->enabled);
}

TEST_F(ExtensionPointRegistryTest, GetExtension) {
    ExtensionPoint point{
        .id = "get.point",
        .name = "Get Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext{
        .id = "get.ext",
        .plugin_id = "plugin",
        .extension_point = "get.point",
        .name = "Get Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext);

    const auto* retrieved = registry.GetExtension("get.ext");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "get.ext");
}

TEST_F(ExtensionPointRegistryTest, RemovePluginExtensions) {
    ExtensionPoint point{
        .id = "remove.point",
        .name = "Remove Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    Extension ext1{
        .id = "ext1",
        .plugin_id = "plugin.remove",
        .extension_point = "remove.point",
        .name = "Extension 1",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };
    Extension ext2{
        .id = "ext2",
        .plugin_id = "plugin.remove",
        .extension_point = "remove.point",
        .name = "Extension 2",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext1);
    registry.RegisterExtension(ext2);

    EXPECT_TRUE(registry.RemovePluginExtensions("plugin.remove"));
    EXPECT_EQ(registry.GetExtensionCount(), 0);
}

TEST_F(ExtensionPointRegistryTest, GetExtensionCount) {
    ExtensionPoint point{
        .id = "count.point",
        .name = "Count Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);

    EXPECT_EQ(registry.GetExtensionCount(), 0);

    Extension ext{
        .id = "count.ext",
        .plugin_id = "plugin",
        .extension_point = "count.point",
        .name = "Count Extension",
        .callback = [](const auto&) { return true; },
        .priority = 100,
        .enabled = true
    };

    registry.RegisterExtension(ext);
    EXPECT_EQ(registry.GetExtensionCount(), 1);
}

TEST_F(ExtensionPointRegistryTest, GetExtensionPointCount) {
    EXPECT_EQ(registry.GetExtensionPointCount(), 0);

    ExtensionPoint point{
        .id = "count.point",
        .name = "Count Point",
        .description = "Test",
        .category = "test",
        .priority = 0,
        .required_params = {},
        .optional_params = {},
        .version = "1.0.0",
        .is_active = true
    };
    registry.RegisterExtensionPoint(point);
    EXPECT_EQ(registry.GetExtensionPointCount(), 1);
}

// ExtensionManager tests
class ExtensionManagerTest : public ::testing::Test {
protected:
    ExtensionManager manager;
};

TEST_F(ExtensionManagerTest, BuiltInExtensionPoints) {
    auto points = manager.GetAvailableExtensionPoints();
    EXPECT_GT(points.size(), 0);
    EXPECT_TRUE(std::find(points.begin(), points.end(),
                         BuiltInExtensionPoints::ON_DOCUMENT_OPEN) != points.end());
}

TEST_F(ExtensionManagerTest, RegisterPluginExtension) {
    bool result = manager.RegisterPluginExtension(
        "test.plugin",
        BuiltInExtensionPoints::ON_DOCUMENT_OPEN,
        "my_extension",
        [](const auto&) { return true; }
    );
    EXPECT_TRUE(result);
}

TEST_F(ExtensionManagerTest, GetExtensionCountForPoint) {
    manager.RegisterPluginExtension(
        "plugin1",
        BuiltInExtensionPoints::ON_DOCUMENT_OPEN,
        "ext1",
        [](const auto&) { return true; }
    );
    manager.RegisterPluginExtension(
        "plugin2",
        BuiltInExtensionPoints::ON_DOCUMENT_OPEN,
        "ext2",
        [](const auto&) { return true; }
    );

    int count = manager.GetExtensionCountForPoint(BuiltInExtensionPoints::ON_DOCUMENT_OPEN);
    EXPECT_EQ(count, 2);
}

TEST_F(ExtensionManagerTest, ExecuteExtensions) {
    int call_count = 0;
    manager.RegisterPluginExtension(
        "test.plugin",
        BuiltInExtensionPoints::ON_DOCUMENT_OPEN,
        "test_ext",
        [&call_count](const auto&) {
            call_count++;
            return true;
        }
    );

    std::map<std::string, std::any> params;
    EXPECT_TRUE(manager.ExecuteExtensions(BuiltInExtensionPoints::ON_DOCUMENT_OPEN, params));
    EXPECT_EQ(call_count, 1);
}

TEST_F(ExtensionManagerTest, GetExtensionSummary) {
    auto summary = manager.GetExtensionSummary();
    EXPECT_GT(summary.size(), 0);
}
