// M17-GAP-074: DesignAnalytics - Metrics collection and pattern analysis
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <mutex>
#include <chrono>

namespace bendcore::analytics {

struct DesignMetric {
    std::string metricName;
    double value;
    std::string unit;
    int64_t timestamp;
    std::string category;
};

struct DesignPattern {
    std::string patternName;
    double confidence;
    std::vector<std::string> features;
    std::string recommendation;
    int32_t occurrenceCount;
};

struct ComplexityAnalysis {
    double geometricComplexity;
    double topologicalComplexity;
    int32_t featureCount;
    int32_t constraintCount;
    double avgFeatureInterdependency;
    std::string complexityLevel;
};

struct QualityMetrics {
    double cleanlinessScore;
    double constraintOptimality;
    double featureOrganization;
    double overallQuality;
    std::vector<std::string> issues;
};

class DesignAnalytics {
public:
    DesignAnalytics();
    ~DesignAnalytics();

    void recordMetric(const std::string& designId, const std::string& metricName,
                     double value, const std::string& unit, const std::string& category);
    std::vector<DesignMetric> getMetrics(const std::string& designId) const;
    std::vector<DesignMetric> getMetricsByCategory(const std::string& designId,
                                                    const std::string& category) const;

    std::vector<DesignPattern> analyzePatterns(const std::string& designId);
    DesignPattern detectPattern(const std::string& designId,
                               const std::string& patternName);
    std::vector<std::string> getPatternRecommendations(const std::string& designId);

    ComplexityAnalysis analyzeComplexity(const std::string& designId);
    std::vector<ComplexityAnalysis> getComplexityTrend(
        const std::string& designId, int64_t timeWindowSeconds);

    QualityMetrics assessQuality(const std::string& designId);
    std::vector<std::string> getQualityIssues(const std::string& designId);
    std::vector<std::string> getQualityImprovements(const std::string& designId);

    std::vector<DesignMetric> getMetricHistory(const std::string& designId,
                                               const std::string& metricName,
                                               int32_t maxSamples = 100);
    std::map<std::string, double> compareDesigns(const std::string& designId1,
                                                 const std::string& designId2);

    std::string generateReport(const std::string& designId);
    std::string generateJsonReport(const std::string& designId);

    void clearAnalytics(const std::string& designId);
    void clearAll();

    std::map<std::string, double> getAnalyticsSummary() const;

private:
    mutable std::mutex analyticsMutex_;
    std::map<std::string, std::vector<DesignMetric>> designMetrics_;
    std::map<std::string, std::vector<DesignPattern>> detectedPatterns_;
    std::map<std::string, QualityMetrics> qualityCache_;
    std::map<std::string, int64_t> lastAnalysisTime_;
};

}  // namespace bendcore::analytics
