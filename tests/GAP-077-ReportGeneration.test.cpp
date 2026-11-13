#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/analytics/report_generator.hpp"

class ReportGenerationTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::analytics::ReportGenerator> report_gen;

    void SetUp() override {
        report_gen = std::make_unique<bendcore::analytics::ReportGenerator>();
    }
};

TEST_F(ReportGenerationTest, InitializeGenerator) {
    EXPECT_NE(report_gen.get(), nullptr);
}

TEST_F(ReportGenerationTest, GeneratePDFReport) {
    auto report = report_gen->generateReport("project1", "pdf");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateHTMLReport) {
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateCSVReport) {
    auto report = report_gen->generateReport("project1", "csv");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateJSONReport) {
    auto report = report_gen->generateReport("project1", "json");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddDataToReport) {
    report_gen->addDataSection("metrics", {{"total_objects", "100"}});
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddChartToReport) {
    report_gen->addChart("performance", "line");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddImageToReport) {
    report_gen->addImage("screenshot.png");
    auto report = report_gen->generateReport("project1", "pdf");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, SetReportTemplate) {
    report_gen->setTemplate("professional");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, SetReportTitle) {
    report_gen->setTitle("Project Analysis Report");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddReportSection) {
    report_gen->addSection("Executive Summary");
    report_gen->addSectionContent("Executive Summary", "This is a summary");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateScheduledReport) {
    auto scheduled = report_gen->scheduleReport("project1", "weekly", "pdf");
    EXPECT_TRUE(scheduled);
}

TEST_F(ReportGenerationTest, EmailReport) {
    auto emailed = report_gen->emailReport("project1", "pdf", "user@example.com");
    EXPECT_TRUE(emailed);
}

TEST_F(ReportGenerationTest, ExportReportMetadata) {
    auto metadata = report_gen->getReportMetadata("project1");
    EXPECT_FALSE(metadata.empty());
}

TEST_F(ReportGenerationTest, GenerateComparisonReport) {
    auto comp = report_gen->generateComparisonReport("project1", "project2", "html");
    EXPECT_FALSE(comp.empty());
}

TEST_F(ReportGenerationTest, GenerateTrendReport) {
    auto trend = report_gen->generateTrendReport("project1", 12, "html");
    EXPECT_FALSE(trend.empty());
}

TEST_F(ReportGenerationTest, GenerateSummaryReport) {
    auto summary = report_gen->generateSummaryReport("all_projects", "html");
    EXPECT_FALSE(summary.empty());
}

TEST_F(ReportGenerationTest, GenerateDetailedReport) {
    auto detailed = report_gen->generateDetailedReport("project1", "html");
    EXPECT_FALSE(detailed.empty());
}

TEST_F(ReportGenerationTest, AddFilterToReport) {
    report_gen->addFilter("date_range", "2024-01-01", "2024-12-31");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddCustomMetric) {
    report_gen->addCustomMetric("efficiency_score", 85.5);
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, SetReportStyle) {
    report_gen->setStyle("theme", "dark");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, IncludeAppendix) {
    report_gen->addAppendix("Methodology", "This is the methodology");
    auto report = report_gen->generateReport("project1", "pdf");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateExecutiveSummary) {
    auto exec = report_gen->generateExecutiveSummary("project1");
    EXPECT_FALSE(exec.empty());
}

TEST_F(ReportGenerationTest, GenerateDetailedAnalysis) {
    auto analysis = report_gen->generateDetailedAnalysis("project1", "performance");
    EXPECT_FALSE(analysis.empty());
}

TEST_F(ReportGenerationTest, ExportReportAsEmail) {
    auto email_html = report_gen->generateReportForEmail("project1");
    EXPECT_FALSE(email_html.empty());
}

TEST_F(ReportGenerationTest, GenerateInteractiveReport) {
    auto interactive = report_gen->generateInteractiveReport("project1", "html");
    EXPECT_FALSE(interactive.empty());
}

TEST_F(ReportGenerationTest, SaveReportTemplate) {
    report_gen->saveCustomTemplate("my_template", "<html>...</html>");
    auto saved = report_gen->getCustomTemplate("my_template");
    EXPECT_FALSE(saved.empty());
}

TEST_F(ReportGenerationTest, GenerateReportWithWatermark) {
    report_gen->setWatermark("CONFIDENTIAL");
    auto report = report_gen->generateReport("project1", "pdf");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateMultiLanguageReport) {
    auto report = report_gen->generateReportMultiLanguage("project1", {"en", "es", "fr"});
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, SetReportPageOrientation) {
    report_gen->setPageOrientation("landscape");
    auto report = report_gen->generateReport("project1", "pdf");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, AddFootnotes) {
    report_gen->addFootnote("Data source: Internal analytics");
    auto report = report_gen->generateReport("project1", "html");
    EXPECT_FALSE(report.empty());
}

TEST_F(ReportGenerationTest, GenerateReportVersion) {
    auto version = report_gen->getReportGeneratorVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(ReportGenerationTest, AsyncReportGeneration) {
    auto future = report_gen->generateReportAsync("project1", "pdf");
    EXPECT_TRUE(future.valid());
}

TEST_F(ReportGenerationTest, BulkReportGeneration) {
    std::vector<std::string> projects{"proj1", "proj2", "proj3"};
    auto bulk = report_gen->generateBulkReports(projects, "html");
    EXPECT_TRUE(bulk);
}

TEST_F(ReportGenerationTest, ReportCaching) {
    auto report1 = report_gen->generateReport("project1", "html");
    auto report2 = report_gen->generateReport("project1", "html");
    EXPECT_EQ(report1, report2);
}
