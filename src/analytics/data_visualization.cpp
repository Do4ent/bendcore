#include "bendcore/analytics/data_visualization.hpp"
#include <algorithm>
#include <sstream>

namespace bendcore::analytics {

DataVisualization::DataVisualization() {}
DataVisualization::~DataVisualization() {
    std::lock_guard<std::mutex> l(m_);
    charts_.clear();
    widgets_.clear();
}
ChartData DataVisualization::createLineChart(const std::string& title, const std::vector<std::string>& labels,
                                            const std::vector<std::vector<double>>& datasets) {
    std::lock_guard<std::mutex> l(m_);
    ChartData c{"line_" + std::to_string(chartCounter_++), "line", title, labels, datasets};
    charts_.push_back(c);
    return c;
}
ChartData DataVisualization::createBarChart(const std::string& title, const std::vector<std::string>& labels,
                                           const std::vector<std::vector<double>>& datasets) {
    std::lock_guard<std::mutex> l(m_);
    ChartData c{"bar_" + std::to_string(chartCounter_++), "bar", title, labels, datasets};
    charts_.push_back(c);
    return c;
}
ChartData DataVisualization::createPieChart(const std::string& title, const std::vector<std::string>& labels,
                                           const std::vector<double>& dataset) {
    std::lock_guard<std::mutex> l(m_);
    std::vector<std::vector<double>> ds;
    ds.push_back(dataset);
    ChartData c{"pie_" + std::to_string(chartCounter_++), "pie", title, labels, ds};
    charts_.push_back(c);
    return c;
}
ChartData DataVisualization::createScatterChart(const std::string& title, const std::vector<double>& x,
                                               const std::vector<double>& y) {
    std::lock_guard<std::mutex> l(m_);
    std::vector<std::vector<double>> ds;
    ds.push_back(x);
    ds.push_back(y);
    ChartData c{"scatter_" + std::to_string(chartCounter_++), "scatter", title, {}, ds};
    charts_.push_back(c);
    return c;
}
std::string DataVisualization::renderChartSvg(const ChartData& c) const {
    return "<svg><text>" + c.title + "</text></svg>";
}
std::string DataVisualization::renderChartJson(const ChartData& c) const {
    std::ostringstream oss;
    oss << "{\"id\":\"" << c.chartId << "\",\"type\":\"" << c.chartType << "\",\"title\":\"" << c.title << "\"}";
    return oss.str();
}
DashboardWidget DataVisualization::createWidget(const std::string& title, const ChartData& c, int32_t interval) {
    std::lock_guard<std::mutex> l(m_);
    DashboardWidget w{"widget_" + std::to_string(widgetCounter_++), c.chartType, title, c, interval};
    widgets_.push_back(w);
    return w;
}
std::vector<DashboardWidget> DataVisualization::getDashboard() const {
    std::lock_guard<std::mutex> l(m_);
    return widgets_;
}
std::string DataVisualization::generateDashboardHtml() const {
    std::lock_guard<std::mutex> l(m_);
    std::ostringstream oss;
    oss << "<html><body>";
    for (const auto& w : widgets_) oss << "<div>" << w.title << "</div>";
    oss << "</body></html>";
    return oss.str();
}
std::string DataVisualization::generateDashboardJson() const {
    std::lock_guard<std::mutex> l(m_);
    return "{\"widgets\":" + std::to_string(widgets_.size()) + ",\"charts\":" + std::to_string(charts_.size()) + "}";
}
void DataVisualization::addWidget(const DashboardWidget& w) {
    std::lock_guard<std::mutex> l(m_);
    widgets_.push_back(w);
}
void DataVisualization::removeWidget(const std::string& id) {
    std::lock_guard<std::mutex> l(m_);
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
                          [&id](const DashboardWidget& w) { return w.widgetId == id; });
    if (it != widgets_.end()) widgets_.erase(it);
}
void DataVisualization::clearDashboard() {
    std::lock_guard<std::mutex> l(m_);
    widgets_.clear();
    charts_.clear();
}
int32_t DataVisualization::getChartCount() const {
    std::lock_guard<std::mutex> l(m_);
    return charts_.size();
}
int32_t DataVisualization::getWidgetCount() const {
    std::lock_guard<std::mutex> l(m_);
    return widgets_.size();
}

}
