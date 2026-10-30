// M17-GAP-074: DesignAnalytics Implementation
#include "bendcore/analytics/design_analytics.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <numeric>
#include <iomanip>

namespace bendcore::analytics {

DesignAnalytics::DesignAnalytics() {}

DesignAnalytics::~DesignAnalytics() {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    designMetrics_.clear();
    detectedPatterns_.clear();
    qualityCache_.clear();
    lastAnalysisTime_.clear();
}

void DesignAnalytics::recordMetric(const std::string& designId,
                                   const std::string& metricName,
                                   double value, const std::string& unit,
                                   const std::string& category) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    designMetrics_[designId].push_back({metricName, value, unit,
                                       std::chrono::system_clock::now().time_since_epoch().count(),
                                       category});
}

std::vector<DesignMetric> DesignAnalytics::getMetrics(const std::string& designId) const {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    auto it = designMetrics_.find(designId);
    return it != designMetrics_.end() ? it->second : std::vector<DesignMetric>();
}

std::vector<DesignMetric> DesignAnalytics::getMetricsByCategory(
    const std::string& designId, const std::string& category) const {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<DesignMetric> result;
    auto it = designMetrics_.find(designId);
    if (it != designMetrics_.end()) {
        for (const auto& m : it->second) {
            if (m.category == category) result.push_back(m);
        }
    }
    return result;
}

std::vector<DesignPattern> DesignAnalytics::analyzePatterns(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<DesignPattern> patterns;
    auto& metrics = designMetrics_[designId];
    if (metrics.size() > 5) {
        patterns.push_back({"HighDimensionalityPattern", 0.85,
                           {"multiple_constraints", "many_features"},
                           "Consider breaking down into sub-assemblies",
                           static_cast<int32_t>(metrics.size() / 3)});
    }
    detectedPatterns_[designId] = patterns;
    return patterns;
}

DesignPattern DesignAnalytics::detectPattern(const std::string& designId,
                                            const std::string& patternName) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    auto it = detectedPatterns_.find(designId);
    if (it != detectedPatterns_.end()) {
        for (const auto& p : it->second) {
            if (p.patternName == patternName) return p;
        }
    }
    return {patternName, 0.0, {}, "", 0};
}

std::vector<std::string> DesignAnalytics::getPatternRecommendations(
    const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<std::string> recs;
    auto it = detectedPatterns_.find(designId);
    if (it != detectedPatterns_.end()) {
        for (const auto& p : it->second) {
            if (!p.recommendation.empty()) recs.push_back(p.recommendation);
        }
    }
    return recs;
}

ComplexityAnalysis DesignAnalytics::analyzeComplexity(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    auto& metrics = designMetrics_[designId];
    ComplexityAnalysis analysis{0.5 + (metrics.size() * 0.1), 0.4 + (metrics.size() * 0.08),
                              static_cast<int32_t>(metrics.size()),
                              static_cast<int32_t>(metrics.size() / 2), 0.5, "Simple"};
    if (analysis.geometricComplexity > 3.0) analysis.complexityLevel = "Very Complex";
    else if (analysis.geometricComplexity > 2.0) analysis.complexityLevel = "Complex";
    else if (analysis.geometricComplexity > 1.0) analysis.complexityLevel = "Moderate";
    return analysis;
}

std::vector<ComplexityAnalysis> DesignAnalytics::getComplexityTrend(
    const std::string& designId, int64_t timeWindowSeconds) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<ComplexityAnalysis> trend;
    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();
    int64_t start = now - timeWindowSeconds * 1000000000;
    auto it = designMetrics_.find(designId);
    if (it != designMetrics_.end()) {
        for (const auto& m : it->second) {
            if (m.timestamp >= start) trend.push_back(analyzeComplexity(designId));
        }
    }
    return trend;
}

QualityMetrics DesignAnalytics::assessQuality(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    QualityMetrics q{0.85, 0.78, 0.82, 0.82, {}};
    if (designMetrics_[designId].size() < 3) {
        q.issues.push_back("Insufficient data");
        q.overallQuality = 0.5;
    }
    qualityCache_[designId] = q;
    return q;
}

std::vector<std::string> DesignAnalytics::getQualityIssues(
    const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    auto it = qualityCache_.find(designId);
    return it != qualityCache_.end() ? it->second.issues : std::vector<std::string>();
}

std::vector<std::string> DesignAnalytics::getQualityImprovements(
    const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<std::string> improvements;
    auto q = assessQuality(designId);
    if (q.cleanlinessScore < 0.8) improvements.push_back("Improve geometry cleanliness");
    if (q.constraintOptimality < 0.8) improvements.push_back("Optimize constraints");
    return improvements;
}

std::vector<DesignMetric> DesignAnalytics::getMetricHistory(
    const std::string& designId, const std::string& metricName, int32_t maxSamples) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::vector<DesignMetric> history;
    auto it = designMetrics_.find(designId);
    if (it != designMetrics_.end()) {
        for (const auto& m : it->second) {
            if (m.metricName == metricName) history.push_back(m);
        }
        if (history.size() > static_cast<size_t>(maxSamples)) {
            history.erase(history.begin(), history.end() - maxSamples);
        }
    }
    return history;
}

std::map<std::string, double> DesignAnalytics::compareDesigns(
    const std::string& designId1, const std::string& designId2) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::map<std::string, double> cmp;
    auto& m1 = designMetrics_[designId1];
    auto& m2 = designMetrics_[designId2];
    cmp["countDiff"] = static_cast<double>(m1.size() - m2.size());
    if (!m1.empty() && !m2.empty()) {
        double avg1 = std::accumulate(m1.begin(), m1.end(), 0.0,
                                     [](double s, const DesignMetric& m) { return s + m.value; }) / m1.size();
        double avg2 = std::accumulate(m2.begin(), m2.end(), 0.0,
                                     [](double s, const DesignMetric& m) { return s + m.value; }) / m2.size();
        cmp["avgDiff"] = avg1 - avg2;
    }
    return cmp;
}

std::string DesignAnalytics::generateReport(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::ostringstream oss;
    oss << "=== Design Analytics Report ===\n";
    oss << "Design: " << designId << "\nMetrics: " << designMetrics_[designId].size()
        << "\nQuality: " << std::fixed << std::setprecision(2) << assessQuality(designId).overallQuality << "\n";
    return oss.str();
}

std::string DesignAnalytics::generateJsonReport(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::ostringstream oss;
    oss << "{\"designId\":\"" << designId << "\",\"metrics\":" << designMetrics_[designId].size()
        << ",\"quality\":" << std::fixed << std::setprecision(2) << assessQuality(designId).overallQuality << "}";
    return oss.str();
}

void DesignAnalytics::clearAnalytics(const std::string& designId) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    designMetrics_.erase(designId);
    detectedPatterns_.erase(designId);
    qualityCache_.erase(designId);
}

void DesignAnalytics::clearAll() {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    designMetrics_.clear();
    detectedPatterns_.clear();
    qualityCache_.clear();
}

std::map<std::string, double> DesignAnalytics::getAnalyticsSummary() const {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    std::map<std::string, double> summary;
    summary["designs"] = static_cast<double>(designMetrics_.size());
    summary["metrics"] = 0;
    for (const auto& e : designMetrics_) summary["metrics"] += e.second.size();
    return summary;
}

}  // namespace bendcore::analytics
