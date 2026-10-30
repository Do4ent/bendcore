#include "bendcore/analytics/report_generator.hpp"
#include <algorithm>
#include <sstream>
#include <numeric>
#include <cmath>
#include <iomanip>

namespace bendcore::analytics {

ReportGenerator::ReportGenerator() {}
ReportGenerator::~ReportGenerator() {
    std::lock_guard<std::mutex> l(m_);
    reports_.clear();
}
ReportData ReportGenerator::generatePdfReport(const std::string& title, const std::string& content) {
    std::lock_guard<std::mutex> l(m_);
    ReportData r{"pdf_" + std::to_string(counter_++), "PDF", title, content,
                 std::chrono::system_clock::now().time_since_epoch().count()};
    reports_.push_back(r);
    return r;
}
ReportData ReportGenerator::generateHtmlReport(const std::string& title, const std::string& content) {
    std::lock_guard<std::mutex> l(m_);
    std::string html = "<html><head><title>" + title + "</title></head><body>" + content + "</body></html>";
    ReportData r{"html_" + std::to_string(counter_++), "HTML", title, html,
                 std::chrono::system_clock::now().time_since_epoch().count()};
    reports_.push_back(r);
    return r;
}
ReportData ReportGenerator::generateJsonReport(const std::string& title, const std::string& content) {
    std::lock_guard<std::mutex> l(m_);
    ReportData r{"json_" + std::to_string(counter_++), "JSON", title, content,
                 std::chrono::system_clock::now().time_since_epoch().count()};
    reports_.push_back(r);
    return r;
}
std::vector<ReportData> ReportGenerator::getReports(int32_t max) const {
    std::lock_guard<std::mutex> l(m_);
    std::vector<ReportData> result;
    size_t start = reports_.size() > static_cast<size_t>(max) ? reports_.size() - max : 0;
    for (size_t i = start; i < reports_.size(); ++i) result.push_back(reports_[i]);
    return result;
}
ReportData ReportGenerator::getReport(const std::string& id) const {
    std::lock_guard<std::mutex> l(m_);
    for (const auto& r : reports_) {
        if (r.reportId == id) return r;
    }
    return {"", "", "", "", 0};
}
std::vector<std::pair<std::string, std::pair<double, double>>> ReportGenerator::aggregateMetrics(
    const std::vector<double>& values, const std::string& name) {
    std::vector<std::pair<std::string, std::pair<double, double>>> result;
    if (values.empty()) return result;
    double min = *std::min_element(values.begin(), values.end());
    double max = *std::max_element(values.begin(), values.end());
    result.push_back({name, {min, max}});
    return result;
}
std::string ReportGenerator::generateSummaryReport(
    const std::map<std::string, std::vector<double>>& metrics) const {
    std::ostringstream oss;
    oss << "=== Summary ===\nMetrics: " << metrics.size() << "\n";
    return oss.str();
}
int32_t ReportGenerator::getTotalReports() const {
    std::lock_guard<std::mutex> l(m_);
    return reports_.size();
}
void ReportGenerator::clearReports() {
    std::lock_guard<std::mutex> l(m_);
    reports_.clear();
}
void ReportGenerator::deleteReport(const std::string& id) {
    std::lock_guard<std::mutex> l(m_);
    auto it = std::find_if(reports_.begin(), reports_.end(),
                          [&id](const ReportData& r) { return r.reportId == id; });
    if (it != reports_.end()) reports_.erase(it);
}

}
