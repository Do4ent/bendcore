#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/analytics/data_visualization.hpp"

class DataVisualizationTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::analytics::DataVisualization> viz;

    void SetUp() override {
        viz = std::make_unique<bendcore::analytics::DataVisualization>();
    }
};

TEST_F(DataVisualizationTest, InitializeVisualization) {
    EXPECT_NE(viz.get(), nullptr);
}

TEST_F(DataVisualizationTest, CreateLineChart) {
    auto chart = viz->createLineChart("performance_metrics");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, CreateBarChart) {
    auto chart = viz->createBarChart("feature_usage");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, CreatePieChart) {
    auto chart = viz->createPieChart("component_distribution");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, CreateScatterPlot) {
    auto chart = viz->createScatterPlot("performance_vs_complexity");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, CreateHeatmap) {
    auto heatmap = viz->createHeatmap("correlation_matrix");
    EXPECT_FALSE(heatmap.empty());
}

TEST_F(DataVisualizationTest, CreateHistogram) {
    auto histogram = viz->createHistogram("object_count_distribution");
    EXPECT_FALSE(histogram.empty());
}

TEST_F(DataVisualizationTest, AddDataToChart) {
    auto chart = viz->createLineChart("test_chart");
    viz->addDataPoint(chart, 1, 100);
    viz->addDataPoint(chart, 2, 150);
    auto data = viz->getChartData(chart);
    EXPECT_GE(data.size(), 2);
}

TEST_F(DataVisualizationTest, SetChartTitle) {
    auto chart = viz->createLineChart("test_chart");
    viz->setChartTitle(chart, "Performance Over Time");
    auto title = viz->getChartTitle(chart);
    EXPECT_EQ(title, "Performance Over Time");
}

TEST_F(DataVisualizationTest, SetAxisLabels) {
    auto chart = viz->createLineChart("test_chart");
    viz->setAxisLabel(chart, "x", "Time (seconds)");
    viz->setAxisLabel(chart, "y", "Performance (ms)");
    auto x_label = viz->getAxisLabel(chart, "x");
    EXPECT_EQ(x_label, "Time (seconds)");
}

TEST_F(DataVisualizationTest, SetChartColors) {
    auto chart = viz->createLineChart("test_chart");
    viz->setChartColor(chart, 0, "#FF0000");
    auto color = viz->getChartColor(chart, 0);
    EXPECT_EQ(color, "#FF0000");
}

TEST_F(DataVisualizationTest, AddLegend) {
    auto chart = viz->createLineChart("test_chart");
    viz->addLegendEntry(chart, "Series 1");
    viz->addLegendEntry(chart, "Series 2");
    auto legend = viz->getLegendEntries(chart);
    EXPECT_EQ(legend.size(), 2);
}

TEST_F(DataVisualizationTest, SetChartDimensions) {
    auto chart = viz->createLineChart("test_chart");
    viz->setDimensions(chart, 800, 600);
    auto dims = viz->getDimensions(chart);
    EXPECT_EQ(dims.width, 800);
    EXPECT_EQ(dims.height, 600);
}

TEST_F(DataVisualizationTest, EnableGridLines) {
    auto chart = viz->createLineChart("test_chart");
    auto enabled = viz->enableGridLines(chart, true);
    EXPECT_TRUE(enabled);
}

TEST_F(DataVisualizationTest, AddTrendLine) {
    auto chart = viz->createScatterPlot("test_chart");
    auto trend = viz->addTrendLine(chart, "linear");
    EXPECT_TRUE(trend);
}

TEST_F(DataVisualizationTest, ExportChartAsImage) {
    auto chart = viz->createLineChart("test_chart");
    auto image = viz->exportAsImage(chart, "png");
    EXPECT_FALSE(image.empty());
}

TEST_F(DataVisualizationTest, ExportChartAsHTML) {
    auto chart = viz->createLineChart("test_chart");
    auto html = viz->exportAsHTML(chart);
    EXPECT_FALSE(html.empty());
}

TEST_F(DataVisualizationTest, ExportChartAsJSON) {
    auto chart = viz->createLineChart("test_chart");
    auto json = viz->exportAsJSON(chart);
    EXPECT_FALSE(json.empty());
}

TEST_F(DataVisualizationTest, CreateDashboard) {
    auto dashboard = viz->createDashboard("main_dashboard");
    EXPECT_FALSE(dashboard.empty());
}

TEST_F(DataVisualizationTest, AddChartToDashboard) {
    auto dashboard = viz->createDashboard("main_dashboard");
    auto chart = viz->createLineChart("perf_chart");
    auto added = viz->addChartToDashboard(dashboard, chart);
    EXPECT_TRUE(added);
}

TEST_F(DataVisualizationTest, CreateKPI) {
    auto kpi = viz->createKPI("efficiency_score", 85.5);
    EXPECT_FALSE(kpi.empty());
}

TEST_F(DataVisualizationTest, AddKPIToDashboard) {
    auto dashboard = viz->createDashboard("main_dashboard");
    auto kpi = viz->createKPI("efficiency", 85.5);
    auto added = viz->addKPIToDashboard(dashboard, kpi);
    EXPECT_TRUE(added);
}

TEST_F(DataVisualizationTest, CreateSparkline) {
    auto sparkline = viz->createSparkline("quick_metric", {1, 2, 3, 4, 5});
    EXPECT_FALSE(sparkline.empty());
}

TEST_F(DataVisualizationTest, InteractiveChart) {
    auto chart = viz->createLineChart("interactive_chart");
    auto interactive = viz->enableInteractivity(chart, true);
    EXPECT_TRUE(interactive);
}

TEST_F(DataVisualizationTest, ChartTooltips) {
    auto chart = viz->createLineChart("tooltip_chart");
    auto tooltips = viz->enableTooltips(chart, true);
    EXPECT_TRUE(tooltips);
}

TEST_F(DataVisualizationTest, ChartZoom) {
    auto chart = viz->createLineChart("zoomable_chart");
    auto zoom = viz->enableZoom(chart, true);
    EXPECT_TRUE(zoom);
}

TEST_F(DataVisualizationTest, CreateComparativeChart) {
    auto chart = viz->createComparativeChart("project1_vs_project2");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, CreateTimeSeriesChart) {
    auto chart = viz->createTimeSeriesChart("performance_timeline");
    EXPECT_FALSE(chart.empty());
}

TEST_F(DataVisualizationTest, VisualizationVersion) {
    auto version = viz->getVisualizationVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(DataVisualizationTest, AsyncChartGeneration) {
    auto future = viz->createLineChartAsync("async_chart");
    EXPECT_TRUE(future.valid());
}

TEST_F(DataVisualizationTest, BulkChartGeneration) {
    std::vector<std::string> chart_types{"line", "bar", "pie"};
    auto charts = viz->createBulkCharts(chart_types);
    EXPECT_EQ(charts.size(), 3);
}

TEST_F(DataVisualizationTest, ChartCaching) {
    auto chart1 = viz->createLineChart("cached_chart");
    auto chart2 = viz->createLineChart("cached_chart");
    EXPECT_EQ(chart1, chart2);
}

TEST_F(DataVisualizationTest, AnimatedChart) {
    auto chart = viz->createLineChart("animated_chart");
    auto animated = viz->enableAnimation(chart, true);
    EXPECT_TRUE(animated);
}

TEST_F(DataVisualizationTest, ExportDashboard) {
    auto dashboard = viz->createDashboard("export_dash");
    auto exported = viz->exportDashboard(dashboard, "html");
    EXPECT_FALSE(exported.empty());
}

TEST_F(DataVisualizationTest, DarkModeChart) {
    auto chart = viz->createLineChart("dark_chart");
    auto dark = viz->enableDarkMode(chart, true);
    EXPECT_TRUE(dark);
}
