#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/analytics/design_analytics.hpp"

class DesignAnalyticsTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::analytics::DesignAnalytics> analytics;

    void SetUp() override {
        analytics = std::make_unique<bendcore::analytics::DesignAnalytics>();
    }
};

TEST_F(DesignAnalyticsTest, InitializeAnalytics) {
    EXPECT_NE(analytics.get(), nullptr);
}

TEST_F(DesignAnalyticsTest, TrackProjectMetrics) {
    analytics->trackProjectMetrics("project1", "active");
    auto metrics = analytics->getProjectMetrics("project1");
    EXPECT_NE(metrics.project_id, "");
}

TEST_F(DesignAnalyticsTest, CollectDesignComplexity) {
    auto complexity = analytics->calculateDesignComplexity("complex_design");
    EXPECT_GT(complexity, 0.0f);
}

TEST_F(DesignAnalyticsTest, TrackObjectCount) {
    analytics->trackObjectCount("design1", 150);
    auto count = analytics->getObjectCount("design1");
    EXPECT_EQ(count, 150);
}

TEST_F(DesignAnalyticsTest, TrackLayerUsage) {
    analytics->trackLayerUsage("design1", "layer1", 50);
    auto usage = analytics->getLayerUsage("design1", "layer1");
    EXPECT_EQ(usage, 50);
}

TEST_F(DesignAnalyticsTest, TrackComponentReuse) {
    analytics->trackComponentUse("component1", "design1");
    analytics->trackComponentUse("component1", "design2");
    auto reuse_count = analytics->getComponentReuseCount("component1");
    EXPECT_EQ(reuse_count, 2);
}

TEST_F(DesignAnalyticsTest, CalculateDesignMetrics) {
    analytics->trackProjectMetrics("metrics_proj", "active");
    auto metrics = analytics->calculateMetrics("metrics_proj");
    EXPECT_GT(metrics.total_elements, 0);
}

TEST_F(DesignAnalyticsTest, TrackUserActivity) {
    analytics->trackUserActivity("user1", "design1", "edit");
    auto activity = analytics->getUserActivity("user1");
    EXPECT_GT(activity.size(), 0);
}

TEST_F(DesignAnalyticsTest, TrackDesignChanges) {
    analytics->trackDesignChange("design1", "element_added");
    analytics->trackDesignChange("design1", "property_changed");
    auto changes = analytics->getDesignChanges("design1");
    EXPECT_GE(changes.size(), 2);
}

TEST_F(DesignAnalyticsTest, IdentifyHotspots) {
    analytics->trackObjectCount("design1", 500);
    auto hotspots = analytics->identifyPerformanceHotspots("design1");
    EXPECT_GT(hotspots.size(), 0);
}

TEST_F(DesignAnalyticsTest, CalculateProjectHealth) {
    analytics->trackProjectMetrics("health_proj", "active");
    auto health = analytics->calculateProjectHealth("health_proj");
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(DesignAnalyticsTest, TrackMemoryUsage) {
    analytics->trackMemoryUsage("design1", 512000);
    auto memory = analytics->getMemoryUsage("design1");
    EXPECT_EQ(memory, 512000);
}

TEST_F(DesignAnalyticsTest, TrackRenderingPerformance) {
    analytics->trackRenderTime("design1", 25);
    auto render_time = analytics->getAverageRenderTime("design1");
    EXPECT_GT(render_time, 0);
}

TEST_F(DesignAnalyticsTest, GenerateDesignReport) {
    analytics->trackProjectMetrics("report_proj", "active");
    auto report = analytics->generateDesignReport("report_proj");
    EXPECT_FALSE(report.empty());
}

TEST_F(DesignAnalyticsTest, CompareDesigns) {
    analytics->trackObjectCount("design1", 100);
    analytics->trackObjectCount("design2", 150);
    auto comparison = analytics->compareDesigns("design1", "design2");
    EXPECT_NE(comparison.design1_metrics.object_count, comparison.design2_metrics.object_count);
}

TEST_F(DesignAnalyticsTest, TrackFeatureUsage) {
    analytics->trackFeatureUsage("feature_bend", 450);
    auto usage = analytics->getFeatureUsageStatistics();
    EXPECT_GT(usage.size(), 0);
}

TEST_F(DesignAnalyticsTest, CalculateProjectTrend) {
    analytics->trackProjectMetrics("trend_proj1", "active");
    analytics->trackProjectMetrics("trend_proj2", "active");
    auto trend = analytics->calculateTrend("bend_operations");
    EXPECT_NE(trend.direction, "");
}

TEST_F(DesignAnalyticsTest, GetTopDesigns) {
    analytics->trackObjectCount("design1", 500);
    analytics->trackObjectCount("design2", 1000);
    analytics->trackObjectCount("design3", 250);
    auto top_designs = analytics->getTopDesignsByComplexity(2);
    EXPECT_EQ(top_designs.size(), 2);
}

TEST_F(DesignAnalyticsTest, TrackErrorFrequency) {
    analytics->trackError("design1", "invalid_geometry");
    analytics->trackError("design1", "calculation_failed");
    auto errors = analytics->getErrorFrequency("design1");
    EXPECT_GE(errors.size(), 2);
}

TEST_F(DesignAnalyticsTest, CalculateProductivity) {
    analytics->trackUserActivity("user1", "design1", "create");
    analytics->trackUserActivity("user1", "design1", "edit");
    auto productivity = analytics->getProductivityMetrics("user1");
    EXPECT_GT(productivity.operations_per_hour, 0.0f);
}

TEST_F(DesignAnalyticsTest, TrackCollaborationMetrics) {
    analytics->trackUserActivity("user1", "design1", "edit");
    analytics->trackUserActivity("user2", "design1", "review");
    auto collab = analytics->getCollaborationMetrics("design1");
    EXPECT_GT(collab.collaborator_count, 0);
}

TEST_F(DesignAnalyticsTest, GenerateTrendAnalysis) {
    for (int i = 0; i < 5; i++) {
        analytics->trackProjectMetrics("proj_" + std::to_string(i), "active");
    }
    auto analysis = analytics->generateTrendAnalysis();
    EXPECT_FALSE(analysis.empty());
}

TEST_F(DesignAnalyticsTest, IdentifyBottlenecks) {
    analytics->trackRenderTime("design1", 200);
    analytics->trackMemoryUsage("design1", 1000000);
    auto bottlenecks = analytics->identifyBottlenecks("design1");
    EXPECT_GT(bottlenecks.size(), 0);
}

TEST_F(DesignAnalyticsTest, CalculateQualityScore) {
    analytics->trackProjectMetrics("quality_proj", "active");
    auto score = analytics->calculateQualityScore("quality_proj");
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(DesignAnalyticsTest, ExportAnalytics) {
    analytics->trackProjectMetrics("export_proj", "active");
    auto exported = analytics->exportAnalytics("export_proj", "csv");
    EXPECT_FALSE(exported.empty());
}

TEST_F(DesignAnalyticsTest, TrackElementTypes) {
    analytics->trackElementType("design1", "tube", 50);
    analytics->trackElementType("design1", "bend", 30);
    auto types = analytics->getElementTypeDistribution("design1");
    EXPECT_GE(types.size(), 2);
}

TEST_F(DesignAnalyticsTest, CalculateOperationDistribution) {
    analytics->trackDesignChange("design1", "bend_operation");
    analytics->trackDesignChange("design1", "straighten_operation");
    auto distribution = analytics->getOperationDistribution("design1");
    EXPECT_GT(distribution.size(), 0);
}

TEST_F(DesignAnalyticsTest, GetAnalyticsSummary) {
    analytics->trackProjectMetrics("summary_proj", "active");
    auto summary = analytics->getAnalyticsSummary();
    EXPECT_FALSE(summary.empty());
}

TEST_F(DesignAnalyticsTest, TrackVersionHistory) {
    analytics->trackVersionChange("design1", 1, "initial");
    analytics->trackVersionChange("design1", 2, "updated");
    auto versions = analytics->getVersionHistory("design1");
    EXPECT_EQ(versions.size(), 2);
}

TEST_F(DesignAnalyticsTest, CalculateReusableComponents) {
    analytics->trackComponentUse("comp1", "design1");
    analytics->trackComponentUse("comp1", "design2");
    analytics->trackComponentUse("comp1", "design3");
    auto reusable = analytics->getHighlyReusableComponents(1);
    EXPECT_GT(reusable.size(), 0);
}

TEST_F(DesignAnalyticsTest, AnalyticsVersion) {
    auto version = analytics->getAnalyticsVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(DesignAnalyticsTest, AsyncAnalyticsComputation) {
    auto future = analytics->computeAnalyticsAsync("design1");
    EXPECT_TRUE(future.valid());
}

TEST_F(DesignAnalyticsTest, DesignSimilarity) {
    analytics->trackObjectCount("design1", 100);
    analytics->trackObjectCount("design2", 105);
    auto similarity = analytics->calculateSimilarity("design1", "design2");
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}
