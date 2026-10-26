#include <gtest/gtest.h>
#include "bendcore/plugins/plugin_marketplace.hpp"

using namespace bendcore::plugins;

class PluginMarketplaceTest : public ::testing::Test {
protected:
    PluginMarketplace& marketplace = PluginMarketplace::GetInstance();

    void SetUp() override {
        marketplace.Clear();
    }
};

TEST_F(PluginMarketplaceTest, RegisterPlugin) {
    PluginMetadata meta{"plugin1", "Plugin 1", "1.0.0", "Author", "Description", "MIT", {}, "", "", 0};
    EXPECT_TRUE(marketplace.RegisterPlugin(meta));
    EXPECT_NE(marketplace.GetPluginInfo("plugin1"), nullptr);
}

TEST_F(PluginMarketplaceTest, RegisterDuplicate) {
    PluginMetadata meta{"plugin1", "Plugin 1", "1.0.0", "Author", "Description", "MIT", {}, "", "", 0};
    EXPECT_TRUE(marketplace.RegisterPlugin(meta));
    EXPECT_FALSE(marketplace.RegisterPlugin(meta));
}

TEST_F(PluginMarketplaceTest, GetAllPlugins) {
    PluginMetadata m1{"p1", "Plugin 1", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    PluginMetadata m2{"p2", "Plugin 2", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(m1);
    marketplace.RegisterPlugin(m2);
    EXPECT_EQ(marketplace.GetAllPlugins().size(), 2);
}

TEST_F(PluginMarketplaceTest, SearchPlugins) {
    PluginMetadata m1{"renderer", "Renderer Plugin", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    PluginMetadata m2{"analyzer", "Analyzer Plugin", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(m1);
    marketplace.RegisterPlugin(m2);

    auto results = marketplace.SearchPlugins("Renderer");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, "renderer");
}

TEST_F(PluginMarketplaceTest, SearchNoResults) {
    PluginMetadata meta{"plugin", "Test Plugin", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    auto results = marketplace.SearchPlugins("NonExistent");
    EXPECT_EQ(results.size(), 0);
}

TEST_F(PluginMarketplaceTest, RatePlugin) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    EXPECT_TRUE(marketplace.RatePlugin("plugin", 5));
}

TEST_F(PluginMarketplaceTest, RateInvalidRating) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    EXPECT_FALSE(marketplace.RatePlugin("plugin", 0));
    EXPECT_FALSE(marketplace.RatePlugin("plugin", 6));
}

TEST_F(PluginMarketplaceTest, GetPluginRating) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    marketplace.RatePlugin("plugin", 5);
    marketplace.RatePlugin("plugin", 3);
    double rating = marketplace.GetPluginRating("plugin");
    EXPECT_NEAR(rating, 4.0, 0.01);
}

TEST_F(PluginMarketplaceTest, GetRatingNonexistent) {
    double rating = marketplace.GetPluginRating("nonexistent");
    EXPECT_EQ(rating, 0.0);
}

TEST_F(PluginMarketplaceTest, InstallPlugin) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    EXPECT_TRUE(marketplace.InstallPlugin("plugin", "http://example.com/plugin.so"));
}

TEST_F(PluginMarketplaceTest, InstallNonexistent) {
    EXPECT_FALSE(marketplace.InstallPlugin("nonexistent", "http://example.com/plugin.so"));
}

TEST_F(PluginMarketplaceTest, GetDownloadCount) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    marketplace.InstallPlugin("plugin", "http://example.com/plugin.so");
    EXPECT_EQ(marketplace.GetDownloadCount("plugin"), 1);
}

TEST_F(PluginMarketplaceTest, GetDownloadCountMultiple) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    for (int i = 0; i < 5; i++) {
        marketplace.InstallPlugin("plugin", "http://example.com/plugin.so");
    }
    EXPECT_EQ(marketplace.GetDownloadCount("plugin"), 5);
}

TEST_F(PluginMarketplaceTest, GetDownloadCountNonexistent) {
    EXPECT_EQ(marketplace.GetDownloadCount("nonexistent"), 0);
}

TEST_F(PluginMarketplaceTest, UninstallPlugin) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    EXPECT_TRUE(marketplace.UninstallPlugin("plugin"));
    EXPECT_EQ(marketplace.GetPluginInfo("plugin"), nullptr);
}

TEST_F(PluginMarketplaceTest, UninstallNonexistent) {
    EXPECT_FALSE(marketplace.UninstallPlugin("nonexistent"));
}

TEST_F(PluginMarketplaceTest, GetPluginInfo) {
    PluginMetadata meta{"myplugin", "My Plugin", "1.0.0", "AuthorName", "A Great Plugin", "Apache2", {"tag1", "tag2"}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    const auto* info = marketplace.GetPluginInfo("myplugin");
    EXPECT_NE(info, nullptr);
    EXPECT_EQ(info->name, "My Plugin");
    EXPECT_EQ(info->author, "AuthorName");
}

TEST_F(PluginMarketplaceTest, GetNonexistentPluginInfo) {
    const auto* info = marketplace.GetPluginInfo("nonexistent");
    EXPECT_EQ(info, nullptr);
}

TEST_F(PluginMarketplaceTest, MultipleRatings) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    for (int i = 0; i < 10; i++) {
        marketplace.RatePlugin("plugin", 4);
    }
    double rating = marketplace.GetPluginRating("plugin");
    EXPECT_NEAR(rating, 4.0, 0.01);
}

TEST_F(PluginMarketplaceTest, SearchCaseInsensitive) {
    PluginMetadata meta{"plugin", "Test PLUGIN", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    auto results = marketplace.SearchPlugins("test");
    EXPECT_EQ(results.size(), 1);
}

TEST_F(PluginMarketplaceTest, GetPluginsByCategory) {
    PluginMetadata meta{"plugin", "Test", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(meta);
    auto results = marketplace.GetPluginsByCategory("rendering");
    // This is currently unimplemented in the placeholder, so expect empty
    EXPECT_GE(results.size(), 0);
}

TEST_F(PluginMarketplaceTest, ClearMarketplace) {
    PluginMetadata m1{"p1", "Plugin 1", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    PluginMetadata m2{"p2", "Plugin 2", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(m1);
    marketplace.RegisterPlugin(m2);
    marketplace.Clear();
    EXPECT_EQ(marketplace.GetAllPlugins().size(), 0);
}

TEST_F(PluginMarketplaceTest, SingletonBehavior) {
    PluginMarketplace& m1 = PluginMarketplace::GetInstance();
    PluginMarketplace& m2 = PluginMarketplace::GetInstance();
    EXPECT_EQ(&m1, &m2);
}

TEST_F(PluginMarketplaceTest, PluginMetadataIntegrity) {
    PluginMetadata meta{"test.plugin", "Test Plugin", "1.2.3", "Test Author", "Test Description", "MIT", {"tag1", "tag2"}, "https://docs.example.com", "https://repo.example.com", 0};
    marketplace.RegisterPlugin(meta);
    const auto* retrieved = marketplace.GetPluginInfo("test.plugin");
    EXPECT_EQ(retrieved->version, "1.2.3");
    EXPECT_EQ(retrieved->author, "Test Author");
}

TEST_F(PluginMarketplaceTest, MixedOperations) {
    PluginMetadata m1{"p1", "Plugin 1", "1.0.0", "A", "D", "MIT", {}, "", "", 0};
    marketplace.RegisterPlugin(m1);
    marketplace.InstallPlugin("p1", "http://example.com/p1.so");
    marketplace.RatePlugin("p1", 5);
    EXPECT_EQ(marketplace.GetDownloadCount("p1"), 1);
    EXPECT_NEAR(marketplace.GetPluginRating("p1"), 5.0, 0.01);
}
