#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace bendcore::analytics {

struct ChartData {
    std::string chartId, chartType, title;
    std::vector<std::string> labels;
    std::vector<std::vector<double>> datasets;
};

struct DashboardWidget {
    std::string widgetId, widgetType, title;
    ChartData chartData;
    int32_t refreshInterval;
};

class DataVisualization {
public:
    DataVisualization();
    ~DataVisualization();
    ChartData createLineChart(const std::string& title, const std::vector<std::string>& labels,
                             const std::vector<std::vector<double>>& datasets);
    ChartData createBarChart(const std::string& title, const std::vector<std::string>& labels,
                            const std::vector<std::vector<double>>& datasets);
    ChartData createPieChart(const std::string& title, const std::vector<std::string>& labels,
                            const std::vector<double>& dataset);
    ChartData createScatterChart(const std::string& title, const std::vector<double>& x,
                                const std::vector<double>& y);
    std::string renderChartSvg(const ChartData& c) const;
    std::string renderChartJson(const ChartData& c) const;
    DashboardWidget createWidget(const std::string& title, const ChartData& c, int32_t interval = 5);
    std::vector<DashboardWidget> getDashboard() const;
    std::string generateDashboardHtml() const;
    std::string generateDashboardJson() const;
    void addWidget(const DashboardWidget& w);
    void removeWidget(const std::string& id);
    void clearDashboard();
    int32_t getChartCount() const;
    int32_t getWidgetCount() const;
private:
    mutable std::mutex m_;
    std::vector<ChartData> charts_;
    std::vector<DashboardWidget> widgets_;
    int32_t chartCounter_ = 0, widgetCounter_ = 0;
};

}
