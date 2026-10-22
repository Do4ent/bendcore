#include "bendcore/reporting_ui.hpp"

#include <sstream>
#include <ctime>
#include <algorithm>

namespace bendcore::reporting_ui {

// ========== ReportingUIManager ==========

ReportingUIManager::ReportingUIManager() = default;
ReportingUIManager::~ReportingUIManager() = default;

bool ReportingUIManager::releaseForProduction(
    const ReleaseRequest& request,
    std::string& outReportId
) {
    // Create report from release request
    reporting::ProductionReport report;
    report.reportId = "RPT-" + std::to_string(std::time(nullptr));
    report.projectName = request.tubeId;
    report.generatedTimestamp = std::time(nullptr);
    report.generatedBy = request.approvedBy;
    report.approvalStatus = "released";

    outReportId = report.reportId;

    // Cache it
    reportCache[report.reportId] = report;

    // Create summary
    ReportSummary summary;
    summary.reportId = report.reportId;
    summary.status = "released";
    summary.timestamp = std::time(nullptr);
    summary.approvedBy = request.approvedBy;
    summary.bendCardCount = report.bendCards.size();
    summary.formats = {"gcode", "pdf", "dwg", "svg", "json", "csv"};

    summaryCache[report.reportId] = summary;

    return true;
}

std::vector<ReportingUIManager::ReportSummary> ReportingUIManager::getProductionReports(
    const std::string& filter,
    int limit
) const {
    std::vector<ReportSummary> result;

    int count = 0;
    for (auto it = summaryCache.rbegin();
         it != summaryCache.rend() && count < limit;
         ++it, ++count) {

        if (!filter.empty()) {
            // Simple filter by report ID or status
            if (it->second.reportId.find(filter) == std::string::npos &&
                it->second.status.find(filter) == std::string::npos) {
                --count; // Don't count this one
                continue;
            }
        }

        result.push_back(it->second);
    }

    return result;
}

bool ReportingUIManager::exportReport(
    const ExportRequest& request,
    std::string& outFolder
) {
    auto it = reportCache.find(request.reportId);
    if (it == reportCache.end()) {
        return false;
    }

    const auto& report = it->second;

    // Generate export folder path
    outFolder = generateExportFolder(request.outputPath, request.reportId);

    // Export each format (stub - actual export happens elsewhere)
    for (const auto& format : request.formats) {
        // G-code, PDF, DWG, SVG, JSON, CSV exports would happen here
        // For now, just log the intent
    }

    return true;
}

bool ReportingUIManager::deleteReport(const std::string& reportId) {
    reportCache.erase(reportId);
    summaryCache.erase(reportId);
    return true;
}

std::optional<reporting::ProductionReport> ReportingUIManager::getReportDetails(
    const std::string& reportId
) const {
    auto it = reportCache.find(reportId);
    if (it != reportCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string ReportingUIManager::generateExportFolder(
    const std::string& basePath,
    const std::string& reportId
) const {
    std::ostringstream path;
    path << basePath;
    if (!basePath.empty() && basePath.back() != '/') {
        path << '/';
    }
    path << reportId << '/';
    return path.str();
}

// ========== StandardsUIManager ==========

StandardsUIManager::StandardsUIManager(const standards::StandardsDatabase& db)
    : database(db) {}

StandardsUIManager::~StandardsUIManager() = default;

std::vector<StandardsUIManager::StandardResult> StandardsUIManager::getStandardsForTube(
    const TubeModel& model,
    double od, double wall
) const {
    std::vector<StandardResult> results;

    // Check all 5 standards
    const std::vector<standards::Standard> standards = {
        standards::Standard::DIN,
        standards::Standard::ISO,
        standards::Standard::GOST,
        standards::Standard::ANSI,
        standards::Standard::EN
    };

    standards::StandardsChecker checker(database);

    for (auto standard : standards) {
        auto compliance = checker.checkTubeCompliance(model, od, wall, standard);

        StandardResult result;
        result.standard = standard;
        result.name = compliance.standardName;
        result.status = compliance.overallStatus;
        result.complianceScore = compliance.compliancePercentage();
        result.passedRules = compliance.compliantRules;
        result.failedRules = compliance.nonCompliantRules;

        // Build summary
        std::ostringstream summary;
        if (result.status == standards::ComplianceStatus::Compliant) {
            summary << "[OK] Compliant (" << (int)result.complianceScore << "%)";
        } else if (result.status == standards::ComplianceStatus::PartiallyCompliant) {
            summary << "[!] Partially Compliant (" << (int)result.complianceScore << "%)";
        } else {
            summary << "[FAIL] Non-Compliant (" << (int)result.complianceScore << "%)";
        }

        result.summary = summary.str();
        results.push_back(result);
    }

    return results;
}

standards::Standard StandardsUIManager::getCurrentStandard(const std::string& tubeId) const {
    auto it = tubeStandardMap.find(tubeId);
    if (it != tubeStandardMap.end()) {
        return it->second;
    }
    // Default to EN
    return standards::Standard::EN;
}

void StandardsUIManager::setCurrentStandard(
    const std::string& tubeId,
    standards::Standard standard
) {
    tubeStandardMap[tubeId] = standard;
}

StandardsUIManager::StandardDetails StandardsUIManager::getStandardDetails(
    const TubeModel& model,
    double od, double wall,
    standards::Standard standard
) const {
    StandardDetails details;

    standards::StandardsChecker checker(database);
    auto compliance = checker.checkTubeCompliance(model, od, wall, standard);

    details.standard = standard;
    details.name = compliance.standardName;
    details.overallStatus = compliance.overallStatus;
    details.complianceScore = compliance.compliancePercentage();

    // Add detail items from results
    for (const auto& result : compliance.results) {
        StandardDetails::DetailItem item;
        item.name = result.ruleName;
        item.value = std::to_string(result.actualValue);

        if (result.status == standards::ComplianceStatus::Compliant) {
            item.status = 'Y';  // OK
        } else if (result.status == standards::ComplianceStatus::PartiallyCompliant) {
            item.status = 'W';  // Warning
        } else {
            item.status = 'N';  // Not OK
        }

        item.message = result.message;
        details.details.push_back(item);
    }

    // Add recommendations
    auto recommendations = checker.recommendStandards(od, wall);
    for (const auto& rec : recommendations) {
        details.recommendations.push_back(rec.reason);
    }

    return details;
}

std::string StandardsUIManager::getComplianceReportHTML(
    const TubeModel& model,
    double od, double wall,
    standards::Standard standard
) const {
    standards::StandardsChecker checker(database);
    auto compliance = checker.checkTubeCompliance(model, od, wall, standard);

    standards::ComplianceReportGenerator generator;
    return generator.generateHTMLReport(compliance);
}

// ========== CollaborationUIStubs ==========

CollaborationUIStubs::CollaborationUIStubs() = default;
CollaborationUIStubs::~CollaborationUIStubs() = default;

std::vector<CollaborationUIStubs::CommentSummary> CollaborationUIStubs::getComments(
    const std::string& tubeId
) const {
    // Stub: returns empty
    // Full implementation in Phase 26B/C
    return {};
}

std::vector<CollaborationUIStubs::VersionSummary> CollaborationUIStubs::getVersions(
    const std::string& tubeId
) const {
    // Stub: returns empty
    // Full implementation in Phase 26B/C
    return {};
}

std::vector<CollaborationUIStubs::ChangeSummary> CollaborationUIStubs::getChanges(
    const std::string& tubeId
) const {
    // Stub: returns empty
    // Full implementation in Phase 26B/C
    return {};
}

// ========== ProductionUIManager ==========

ProductionUIManager::ProductionUIManager(const standards::StandardsDatabase& db)
    : standardsUI(db), database(db) {}

ProductionUIManager::~ProductionUIManager() = default;

bool ProductionUIManager::executeReleaseWorkflow(
    const ReleaseWorkflow& workflow,
    std::string& outReportId,
    std::vector<std::string>& outBlockers
) {
    outBlockers.clear();

    // Check standards compliance
    auto standards = standardsUI.getStandardsForTube(
        workflow.tube, workflow.od, workflow.wall
    );

    for (const auto& std : standards) {
        if (std.status == standards::ComplianceStatus::NonCompliant) {
            outBlockers.push_back("Non-compliant with " + std.name);
        }
    }

    // If blockers exist, don't release
    if (!outBlockers.empty()) {
        return false;
    }

    // Release for production
    ReportingUIManager::ReleaseRequest request;
    request.approvedBy = workflow.approvedBy;
    request.comments = workflow.comments;
    request.releaseTimestamp = std::time(nullptr);

    return reportingUI.releaseForProduction(request, outReportId);
}

}  // namespace bendcore::reporting_ui
