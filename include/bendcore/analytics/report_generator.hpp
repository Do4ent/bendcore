#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace bendcore::analytics {

struct ReportData {
    std::string reportId, reportType, title, content;
    int64_t generatedAt;
};

class ReportGenerator {
public:
    ReportGenerator();
    ~ReportGenerator();
    ReportData generatePdfReport(const std::string& title, const std::string& content);
    ReportData generateHtmlReport(const std::string& title, const std::string& content);
    ReportData generateJsonReport(const std::string& title, const std::string& content);
    std::vector<ReportData> getReports(int32_t max = 50) const;
    ReportData getReport(const std::string& id) const;
    std::vector<std::pair<std::string, std::pair<double, double>>> aggregateMetrics(
        const std::vector<double>& values, const std::string& name);
    std::string generateSummaryReport(const std::map<std::string, std::vector<double>>& metrics) const;
    int32_t getTotalReports() const;
    void clearReports();
    void deleteReport(const std::string& id);
private:
    mutable std::mutex m_;
    std::vector<ReportData> reports_;
    int32_t counter_ = 0;
};

}
