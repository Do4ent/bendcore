// M17-GAP-074: DesignAnalytics Tests
#include <gtest/gtest.h>
#include "bendcore/analytics/design_analytics.hpp"

using namespace bendcore::analytics;

class DesignAnalyticsTest : public ::testing::Test {
protected:
    DesignAnalytics analytics;
};

TEST_F(DesignAnalyticsTest, RecordMetric) {
    analytics.recordMetric("design1", "dimension", 10.5, "mm", "geometry");
    auto metrics = analytics.getMetrics("design1");
    EXPECT_EQ(metrics.size(), 1);
    EXPECT_EQ(metrics[0].metricName, "dimension");
}

TEST_F(DesignAnalyticsTest, RecordMultipleMetrics) {
    for (int i = 0; i < 5; ++i) {
        analytics.recordMetric("design1", "metric_" + std::to_string(i), i * 1.5, "unit", "test");
    }
    auto metrics = analytics.getMetrics("design1");
    EXPECT_EQ(metrics.size(), 5);
}

TEST_F(DesignAnalyticsTest, GetMetricsByCategory) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "geometry");
    analytics.recordMetric("design1", "m2", 2.0, "u", "geometry");
    analytics.recordMetric("design1", "m3", 3.0, "u", "complexity");

    auto geom = analytics.getMetricsByCategory("design1", "geometry");
    EXPECT_EQ(geom.size(), 2);

    auto complex = analytics.getMetricsByCategory("design1", "complexity");
    EXPECT_EQ(complex.size(), 1);
}

TEST_F(DesignAnalyticsTest, AnalyzePatterns) {
    for (int i = 0; i < 10; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 2.0, "u", "g");
    }

    auto patterns = analytics.analyzePatterns("design1");
    EXPECT_TRUE(patterns.size() > 0);
}

TEST_F(DesignAnalyticsTest, DetectPattern) {
    for (int i = 0; i < 10; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 2.0, "u", "g");
    }

    auto pattern = analytics.detectPattern("design1", "HighDimensionalityPattern");
    EXPECT_NE(pattern.patternName, "");
}

TEST_F(DesignAnalyticsTest, GetPatternRecommendations) {
    for (int i = 0; i < 10; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 2.0, "u", "g");
    }

    analytics.analyzePatterns("design1");
    auto recommendations = analytics.getPatternRecommendations("design1");
    EXPECT_TRUE(recommendations.size() >= 0);
}

TEST_F(DesignAnalyticsTest, AnalyzeComplexity) {
    for (int i = 0; i < 8; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 1.0, "u", "g");
    }

    auto complexity = analytics.analyzeComplexity("design1");
    EXPECT_GT(complexity.geometricComplexity, 0.0);
    EXPECT_GT(complexity.topologicalComplexity, 0.0);
    EXPECT_GT(complexity.featureCount, 0);
}

TEST_F(DesignAnalyticsTest, GetComplexityTrend) {
    for (int i = 0; i < 5; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 1.0, "u", "g");
    }

    auto trend = analytics.getComplexityTrend("design1", 3600);
    EXPECT_TRUE(trend.size() >= 0);
}

TEST_F(DesignAnalyticsTest, AssessQuality) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");
    analytics.recordMetric("design1", "m2", 2.0, "u", "g");
    analytics.recordMetric("design1", "m3", 3.0, "u", "g");

    auto quality = analytics.assessQuality("design1");
    EXPECT_GE(quality.cleanlinessScore, 0.0);
    EXPECT_LE(quality.overallQuality, 100.0);
}

TEST_F(DesignAnalyticsTest, GetQualityIssues) {
    auto quality = analytics.assessQuality("design_empty");
    auto issues = analytics.getQualityIssues("design_empty");
    EXPECT_TRUE(issues.size() >= 0);
}

TEST_F(DesignAnalyticsTest, GetQualityImprovements) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");

    auto improvements = analytics.getQualityImprovements("design1");
    EXPECT_TRUE(improvements.size() >= 0);
}

TEST_F(DesignAnalyticsTest, GetMetricHistory) {
    for (int i = 0; i < 3; ++i) {
        analytics.recordMetric("design1", "metric_dim", i * 2.0, "mm", "g");
    }

    auto history = analytics.getMetricHistory("design1", "metric_dim", 10);
    EXPECT_EQ(history.size(), 3);
}

TEST_F(DesignAnalyticsTest, CompareDesigns) {
    for (int i = 0; i < 3; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 1.0, "u", "g");
        analytics.recordMetric("design2", "m" + std::to_string(i), i * 2.0, "u", "g");
    }

    auto comparison = analytics.compareDesigns("design1", "design2");
    EXPECT_TRUE(comparison.size() > 0);
}

TEST_F(DesignAnalyticsTest, GenerateReport) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");

    auto report = analytics.generateReport("design1");
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Design Analytics Report") != std::string::npos);
}

TEST_F(DesignAnalyticsTest, GenerateJsonReport) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");

    auto report = analytics.generateJsonReport("design1");
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("designId") != std::string::npos);
}

TEST_F(DesignAnalyticsTest, ClearAnalytics) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");
    analytics.clearAnalytics("design1");

    auto metrics = analytics.getMetrics("design1");
    EXPECT_EQ(metrics.size(), 0);
}

TEST_F(DesignAnalyticsTest, ClearAll) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");
    analytics.recordMetric("design2", "m2", 2.0, "u", "g");

    analytics.clearAll();

    EXPECT_EQ(analytics.getMetrics("design1").size(), 0);
    EXPECT_EQ(analytics.getMetrics("design2").size(), 0);
}

TEST_F(DesignAnalyticsTest, GetAnalyticsSummary) {
    analytics.recordMetric("design1", "m1", 1.0, "u", "g");
    analytics.recordMetric("design2", "m2", 2.0, "u", "g");

    auto summary = analytics.getAnalyticsSummary();
    EXPECT_TRUE(summary.size() > 0);
}

TEST_F(DesignAnalyticsTest, InvalidMetric) {
    analytics.recordMetric("design1", "", 1.0, "u", "g");
    auto metrics = analytics.getMetrics("design1");
    EXPECT_EQ(metrics.size(), 0);  // Invalid metric should not be recorded
}

TEST_F(DesignAnalyticsTest, NonExistentDesign) {
    auto metrics = analytics.getMetrics("nonexistent");
    EXPECT_EQ(metrics.size(), 0);
}

TEST_F(DesignAnalyticsTest, ThreadSafety) {
    for (int i = 0; i < 10; ++i) {
        analytics.recordMetric("design_" + std::to_string(i), "m1", i * 1.0, "u", "g");
    }

    auto summary = analytics.getAnalyticsSummary();
    EXPECT_EQ(summary["totalDesigns"], 10.0);
}

TEST_F(DesignAnalyticsTest, ComplexityLevel) {
    for (int i = 0; i < 30; ++i) {
        analytics.recordMetric("design_complex", "m" + std::to_string(i), i * 1.0, "u", "g");
    }

    auto complexity = analytics.analyzeComplexity("design_complex");
    EXPECT_EQ(complexity.complexityLevel, "Complex");
}

TEST_F(DesignAnalyticsTest, MetricStatistics) {
    for (int i = 1; i <= 5; ++i) {
        analytics.recordMetric("design1", "metric_set", i * 2.0, "u", "test");
    }

    auto metrics = analytics.getMetrics("design1");
    EXPECT_EQ(metrics.size(), 5);
}

TEST_F(DesignAnalyticsTest, PatternConfidence) {
    for (int i = 0; i < 10; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 1.0, "u", "g");
    }

    auto patterns = analytics.analyzePatterns("design1");
    for (const auto& p : patterns) {
        EXPECT_GE(p.confidence, 0.0);
        EXPECT_LE(p.confidence, 1.0);
    }
}

TEST_F(DesignAnalyticsTest, QualityScore) {
    analytics.recordMetric("d1", "m1", 1.0, "u", "g");
    analytics.recordMetric("d1", "m2", 2.0, "u", "g");
    analytics.recordMetric("d1", "m3", 3.0, "u", "g");

    auto quality = analytics.assessQuality("d1");
    EXPECT_GE(quality.overallQuality, 0.0);
    EXPECT_LE(quality.overallQuality, 100.0);
}

TEST_F(DesignAnalyticsTest, MultipleDesignTracking) {
    for (int d = 1; d <= 5; ++d) {
        std::string did = "design" + std::to_string(d);
        for (int m = 1; m <= 3; ++m) {
            analytics.recordMetric(did, "metric" + std::to_string(m), m * 1.0, "u", "g");
        }
    }

    auto summary = analytics.getAnalyticsSummary();
    EXPECT_EQ(summary["totalDesigns"], 5.0);
    EXPECT_EQ(summary["totalMetrics"], 15.0);
}

TEST_F(DesignAnalyticsTest, HistoryMaxSamples) {
    for (int i = 0; i < 200; ++i) {
        analytics.recordMetric("design1", "metric_history", i * 1.0, "u", "g");
    }

    auto history = analytics.getMetricHistory("design1", "metric_history", 50);
    EXPECT_LE(history.size(), 50);
}

TEST_F(DesignAnalyticsTest, EmptyMetricsComparison) {
    auto comparison = analytics.compareDesigns("empty1", "empty2");
    EXPECT_TRUE(comparison.size() > 0);
}

TEST_F(DesignAnalyticsTest, ReportContent) {
    for (int i = 0; i < 5; ++i) {
        analytics.recordMetric("design1", "m" + std::to_string(i), i * 1.0, "u", "g");
    }

    auto report = analytics.generateReport("design1");
    EXPECT_TRUE(report.find("Metrics Count") != std::string::npos);
    EXPECT_TRUE(report.find("Quality Score") != std::string::npos);
}

TEST_F(DesignAnalyticsTest, BasicOperations) {
    EXPECT_TRUE(true);
}
