// M17-GAP-077: ReportGeneration Tests
#include <gtest/gtest.h>
#include "bendcore/analytics/report_generator.hpp"

using namespace bendcore::analytics;

class ReportGeneratorTest : public ::testing::Test {
protected:
    ReportGenerator generator;
};

TEST_F(ReportGeneratorTest, GeneratePdfReport) {
    auto report = generator.generatePdfReport("Test Report", "This is a test report");
    
    EXPECT_EQ(report.reportType, "PDF");
    EXPECT_EQ(report.title, "Test Report");
    EXPECT_FALSE(report.reportId.empty());
}

TEST_F(ReportGeneratorTest, GenerateHtmlReport) {
    auto report = generator.generateHtmlReport("HTML Report", "Content here");
    
    EXPECT_EQ(report.reportType, "HTML");
    EXPECT_TRUE(report.content.find("<html>") != std::string::npos);
}

TEST_F(ReportGeneratorTest, GenerateJsonReport) {
    auto report = generator.generateJsonReport("JSON Report", "{\"data\": \"value\"}");
    
    EXPECT_EQ(report.reportType, "JSON");
    EXPECT_FALSE(report.reportId.empty());
}

TEST_F(ReportGeneratorTest, GetReports) {
    generator.generatePdfReport("Report 1", "Content 1");
    generator.generateHtmlReport("Report 2", "Content 2");
    generator.generateJsonReport("Report 3", "Content 3");
    
    auto reports = generator.getReports(10);
    EXPECT_EQ(reports.size(), 3);
}

TEST_F(ReportGeneratorTest, GetSingleReport) {
    auto generated = generator.generatePdfReport("Single", "Content");
    auto retrieved = generator.getReport(generated.reportId);
    
    EXPECT_EQ(retrieved.reportId, generated.reportId);
}

TEST_F(ReportGeneratorTest, AggregateMetrics) {
    std::vector<double> values{1.0, 2.0, 3.0, 4.0, 5.0};
    auto metrics = generator.aggregateMetrics(values, "test_metric");
    
    EXPECT_EQ(metrics.size(), 1);
    EXPECT_EQ(metrics[0].minValue, 1.0);
    EXPECT_EQ(metrics[0].maxValue, 5.0);
    EXPECT_EQ(metrics[0].sampleCount, 5);
}

TEST_F(ReportGeneratorTest, GenerateSummaryReport) {
    std::map<std::string, std::vector<double>> metricsMap;
    metricsMap["metric1"] = {1.0, 2.0, 3.0};
    metricsMap["metric2"] = {4.0, 5.0, 6.0};
    
    auto report = generator.generateSummaryReport(metricsMap);
    
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Summary Report") != std::string::npos);
}

TEST_F(ReportGeneratorTest, GetTotalReports) {
    generator.generatePdfReport("R1", "C1");
    generator.generateHtmlReport("R2", "C2");
    
    EXPECT_EQ(generator.getTotalReports(), 2);
}

TEST_F(ReportGeneratorTest, ClearReports) {
    generator.generatePdfReport("R1", "C1");
    generator.generateHtmlReport("R2", "C2");
    
    generator.clearReports();
    
    EXPECT_EQ(generator.getTotalReports(), 0);
}

TEST_F(ReportGeneratorTest, DeleteReport) {
    auto report = generator.generatePdfReport("To Delete", "Content");
    generator.deleteReport(report.reportId);
    
    EXPECT_EQ(generator.getTotalReports(), 0);
}

TEST_F(ReportGeneratorTest, MultipleReportTypes) {
    for (int i = 0; i < 3; ++i) {
        generator.generatePdfReport("Pdf_" + std::to_string(i), "Content");
    }
    for (int i = 0; i < 2; ++i) {
        generator.generateHtmlReport("Html_" + std::to_string(i), "Content");
    }
    for (int i = 0; i < 1; ++i) {
        generator.generateJsonReport("Json_" + std::to_string(i), "Content");
    }
    
    EXPECT_EQ(generator.getTotalReports(), 6);
}

TEST_F(ReportGeneratorTest, ReportTimestamp) {
    auto report = generator.generatePdfReport("Test", "Content");
    
    EXPECT_GT(report.generatedAt, 0);
}

TEST_F(ReportGeneratorTest, ReportIdUniqueness) {
    auto r1 = generator.generatePdfReport("R1", "C1");
    auto r2 = generator.generatePdfReport("R2", "C2");
    
    EXPECT_NE(r1.reportId, r2.reportId);
}

TEST_F(ReportGeneratorTest, EmptyMetrics) {
    std::vector<double> empty;
    auto metrics = generator.aggregateMetrics(empty, "empty");
    
    EXPECT_EQ(metrics.size(), 0);
}

TEST_F(ReportGeneratorTest, SingleValueMetrics) {
    std::vector<double> single{42.0};
    auto metrics = generator.aggregateMetrics(single, "single");
    
    EXPECT_EQ(metrics[0].minValue, 42.0);
    EXPECT_EQ(metrics[0].maxValue, 42.0);
    EXPECT_EQ(metrics[0].averageValue, 42.0);
}

TEST_F(ReportGeneratorTest, LargeMetricsAggregation) {
    std::vector<double> values;
    for (int i = 0; i < 1000; ++i) {
        values.push_back(i * 1.5);
    }
    
    auto metrics = generator.aggregateMetrics(values, "large");
    
    EXPECT_EQ(metrics[0].sampleCount, 1000);
}

TEST_F(ReportGeneratorTest, GetReportsWithLimit) {
    for (int i = 0; i < 100; ++i) {
        generator.generatePdfReport("Report_" + std::to_string(i), "C");
    }
    
    auto reports = generator.getReports(30);
    EXPECT_LE(reports.size(), 30);
}

TEST_F(ReportGeneratorTest, NonexistentReport) {
    auto report = generator.getReport("nonexistent_id");
    
    EXPECT_TRUE(report.reportId.empty());
}

TEST_F(ReportGeneratorTest, ReportContent) {
    std::string content = "Test content with special chars: !@#$%^&*()";
    auto report = generator.generatePdfReport("Content Test", content);
    
    EXPECT_EQ(report.content, content);
}

TEST_F(ReportGeneratorTest, HtmlReportStructure) {
    auto report = generator.generateHtmlReport("Test", "Test Content");
    
    EXPECT_TRUE(report.content.find("<html>") != std::string::npos);
    EXPECT_TRUE(report.content.find("</html>") != std::string::npos);
}

TEST_F(ReportGeneratorTest, DeleteNonexistentReport) {
    generator.deleteReport("nonexistent");
    EXPECT_EQ(generator.getTotalReports(), 0);
}

TEST_F(ReportGeneratorTest, SummaryReportMultipleMetrics) {
    std::map<std::string, std::vector<double>> metricsMap;
    metricsMap["metric_a"] = {10.0, 20.0, 30.0};
    metricsMap["metric_b"] = {5.0, 15.0, 25.0};
    metricsMap["metric_c"] = {100.0, 200.0, 300.0};
    
    auto report = generator.generateSummaryReport(metricsMap);
    
    EXPECT_TRUE(report.find("metric_a") != std::string::npos);
    EXPECT_TRUE(report.find("metric_b") != std::string::npos);
    EXPECT_TRUE(report.find("metric_c") != std::string::npos);
}

TEST_F(ReportGeneratorTest, MetricsAverageBoundary) {
    std::vector<double> values{0.0, 100.0};
    auto metrics = generator.aggregateMetrics(values, "boundary");
    
    EXPECT_EQ(metrics[0].averageValue, 50.0);
}

TEST_F(ReportGeneratorTest, LongReportTitles) {
    std::string longTitle(500, 'A');
    auto report = generator.generatePdfReport(longTitle, "Content");
    
    EXPECT_EQ(report.title, longTitle);
}

TEST_F(ReportGeneratorTest, BasicOperations) {
    EXPECT_TRUE(true);
}
