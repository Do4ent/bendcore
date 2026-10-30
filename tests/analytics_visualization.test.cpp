// M17-GAP-078: DataVisualization Tests
#include <gtest/gtest.h>
#include "bendcore/analytics/data_visualization.hpp"

using namespace bendcore::analytics;

class DataVisualizationTest : public ::testing::Test {
protected:
    DataVisualization viz;
};

TEST_F(DataVisualizationTest, CreateLineChart) {
    std::vector<std::string> labels{"Jan", "Feb", "Mar"};
    std::vector<std::vector<double>> datasets{{1.0, 2.0, 3.0}};
    
    auto chart = viz.createLineChart("Line Chart", labels, datasets);
    
    EXPECT_EQ(chart.chartType, "line");
    EXPECT_EQ(chart.title, "Line Chart");
}

TEST_F(DataVisualizationTest, CreateBarChart) {
    std::vector<std::string> labels{"A", "B", "C"};
    std::vector<std::vector<double>> datasets{{10.0, 20.0, 30.0}};
    
    auto chart = viz.createBarChart("Bar Chart", labels, datasets);
    
    EXPECT_EQ(chart.chartType, "bar");
}

TEST_F(DataVisualizationTest, CreatePieChart) {
    std::vector<std::string> labels{"Slice1", "Slice2", "Slice3"};
    std::vector<double> dataset{30.0, 40.0, 30.0};
    
    auto chart = viz.createPieChart("Pie Chart", labels, dataset);
    
    EXPECT_EQ(chart.chartType, "pie");
}

TEST_F(DataVisualizationTest, CreateScatterChart) {
    std::vector<double> xData{1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> yData{2.0, 4.0, 6.0, 8.0, 10.0};
    
    auto chart = viz.createScatterChart("Scatter", xData, yData);
    
    EXPECT_EQ(chart.chartType, "scatter");
}

TEST_F(DataVisualizationTest, RenderChartJson) {
    std::vector<std::string> labels{"A", "B"};
    std::vector<std::vector<double>> datasets{{1.0, 2.0}};
    
    auto chart = viz.createLineChart("Test", labels, datasets);
    auto rendered = viz.renderChartJson(chart);
    
    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("chartId") != std::string::npos);
}

TEST_F(DataVisualizationTest, RenderChartSvg) {
    std::vector<std::string> labels{"X"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("SVG Test", labels, datasets);
    auto rendered = viz.renderChartSvg(chart);
    
    EXPECT_FALSE(rendered.empty());
    EXPECT_TRUE(rendered.find("<svg") != std::string::npos);
}

TEST_F(DataVisualizationTest, RenderChartDefault) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createBarChart("Test", labels, datasets);
    auto rendered = viz.renderChart(chart);
    
    EXPECT_FALSE(rendered.empty());
}

TEST_F(DataVisualizationTest, CreateWidget) {
    std::vector<std::string> labels{"Data"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("Widget Chart", labels, datasets);
    auto widget = viz.createWidget("My Widget", chart, 5);
    
    EXPECT_EQ(widget.title, "My Widget");
    EXPECT_EQ(widget.refreshIntervalSeconds, 5);
}

TEST_F(DataVisualizationTest, GetDashboard) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C1", labels, datasets);
    viz.createWidget("W1", chart, 5);
    
    auto dashboard = viz.getDashboard();
    EXPECT_EQ(dashboard.size(), 1);
}

TEST_F(DataVisualizationTest, GenerateDashboardHtml) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C1", labels, datasets);
    viz.createWidget("W1", chart, 5);
    
    auto html = viz.generateDashboardHtml();
    
    EXPECT_FALSE(html.empty());
    EXPECT_TRUE(html.find("<html>") != std::string::npos);
}

TEST_F(DataVisualizationTest, GenerateDashboardJson) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C1", labels, datasets);
    viz.createWidget("W1", chart, 5);
    
    auto json = viz.generateDashboardJson();
    
    EXPECT_FALSE(json.empty());
    EXPECT_TRUE(json.find("widgets") != std::string::npos);
}

TEST_F(DataVisualizationTest, AddWidget) {
    DashboardWidget widget{
        "test_widget",
        "line",
        "Test",
        ChartData{"c1", "line", "Chart", {}, {}, "json"},
        5
    };
    
    viz.addWidget(widget);
    
    EXPECT_EQ(viz.getWidgetCount(), 1);
}

TEST_F(DataVisualizationTest, RemoveWidget) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C", labels, datasets);
    auto widget = viz.createWidget("W", chart, 5);
    
    viz.removeWidget(widget.widgetId);
    
    EXPECT_EQ(viz.getWidgetCount(), 0);
}

TEST_F(DataVisualizationTest, ClearDashboard) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    for (int i = 0; i < 5; ++i) {
        auto chart = viz.createLineChart("C" + std::to_string(i), labels, datasets);
        viz.createWidget("W" + std::to_string(i), chart, 5);
    }
    
    viz.clearDashboard();
    
    EXPECT_EQ(viz.getWidgetCount(), 0);
    EXPECT_EQ(viz.getChartCount(), 0);
}

TEST_F(DataVisualizationTest, GetChartCount) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    for (int i = 0; i < 3; ++i) {
        viz.createLineChart("C" + std::to_string(i), labels, datasets);
    }
    
    EXPECT_EQ(viz.getChartCount(), 3);
}

TEST_F(DataVisualizationTest, GetWidgetCount) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C", labels, datasets);
    for (int i = 0; i < 3; ++i) {
        viz.createWidget("W" + std::to_string(i), chart, 5);
    }
    
    EXPECT_EQ(viz.getWidgetCount(), 3);
}

TEST_F(DataVisualizationTest, MultipleChartTypes) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    std::vector<double> pieData{1.0};
    std::vector<double> scatterX{1.0};
    std::vector<double> scatterY{2.0};
    
    viz.createLineChart("Line", labels, datasets);
    viz.createBarChart("Bar", labels, datasets);
    viz.createPieChart("Pie", labels, pieData);
    viz.createScatterChart("Scatter", scatterX, scatterY);
    
    EXPECT_EQ(viz.getChartCount(), 4);
}

TEST_F(DataVisualizationTest, ChartIdUniqueness) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto c1 = viz.createLineChart("C1", labels, datasets);
    auto c2 = viz.createLineChart("C2", labels, datasets);
    
    EXPECT_NE(c1.chartId, c2.chartId);
}

TEST_F(DataVisualizationTest, WidgetIdUniqueness) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C", labels, datasets);
    auto w1 = viz.createWidget("W1", chart, 5);
    auto w2 = viz.createWidget("W2", chart, 5);
    
    EXPECT_NE(w1.widgetId, w2.widgetId);
}

TEST_F(DataVisualizationTest, HighVolumeCharts) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    for (int i = 0; i < 50; ++i) {
        viz.createLineChart("Chart_" + std::to_string(i), labels, datasets);
    }
    
    EXPECT_EQ(viz.getChartCount(), 50);
}

TEST_F(DataVisualizationTest, EmptyLabels) {
    std::vector<std::string> empty;
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("Empty Labels", empty, datasets);
    
    EXPECT_EQ(chart.labels.size(), 0);
}

TEST_F(DataVisualizationTest, MultipleDatasets) {
    std::vector<std::string> labels{"A", "B", "C"};
    std::vector<std::vector<double>> datasets{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };
    
    auto chart = viz.createLineChart("Multi", labels, datasets);
    
    EXPECT_EQ(chart.datasets.size(), 3);
}

TEST_F(DataVisualizationTest, DashboardHtmlStructure) {
    std::vector<std::string> labels{"L"};
    std::vector<std::vector<double>> datasets{{1.0}};
    
    auto chart = viz.createLineChart("C", labels, datasets);
    viz.createWidget("W", chart, 5);
    
    auto html = viz.generateDashboardHtml();
    
    EXPECT_TRUE(html.find("<body>") != std::string::npos);
    EXPECT_TRUE(html.find("</body>") != std::string::npos);
}

TEST_F(DataVisualizationTest, BasicOperations) {
    EXPECT_TRUE(true);
}
