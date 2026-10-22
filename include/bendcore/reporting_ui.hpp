// UI Logic для Production Reporting, Standards, Collaboration
// Phase 26 Stage A: UI Integration
#pragma once

#include "bendcore/reporting.hpp"
#include "bendcore/standards.hpp"
#include "bendcore/tube_model.hpp"

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace bendcore::reporting_ui {

// ========== Production Reporting UI ==========

class ReportingUIManager {
public:
    explicit ReportingUIManager();
    ~ReportingUIManager();

    // Release for Production workflow
    struct ReleaseRequest {
        std::string tubeId;
        std::string approvedBy;
        std::string comments;
        int64_t releaseTimestamp = 0;
    };

    bool releaseForProduction(
        const ReleaseRequest& request,
        std::string& outReportId
    );

    // Production Reports panel
    struct ReportSummary {
        std::string reportId;
        std::string status;              // draft, approved, released
        int64_t timestamp = 0;
        std::string approvedBy;
        int bendCardCount = 0;
        std::vector<std::string> formats; // exported formats
    };

    std::vector<ReportSummary> getProductionReports(
        const std::string& filter = "",
        int limit = 50
    ) const;

    // Export functionality
    struct ExportRequest {
        std::string reportId;
        std::vector<std::string> formats; // "gcode", "pdf", "dwg", "svg", "json", "csv"
        std::string outputPath;
        bool createSubfolder = true;
    };

    bool exportReport(const ExportRequest& request, std::string& outFolder);

    // Report management
    bool deleteReport(const std::string& reportId);

    std::optional<reporting::ProductionReport> getReportDetails(const std::string& reportId) const;

private:
    std::map<std::string, reporting::ProductionReport> reportCache;
    std::map<std::string, ReportSummary> summaryCache;

    std::string generateExportFolder(
        const std::string& basePath,
        const std::string& reportId
    ) const;
};

// ========== Standards Compliance UI ==========

class StandardsUIManager {
public:
    explicit StandardsUIManager(const standards::StandardsDatabase& db);
    ~StandardsUIManager();

    // Get all standards for a tube
    struct StandardResult {
        standards::Standard standard;
        std::string name;
        standards::ComplianceStatus status;
        double complianceScore = 0.0; // 0-100%
        int passedRules = 0;
        int failedRules = 0;
        std::string summary;
    };

    std::vector<StandardResult> getStandardsForTube(
        const TubeModel& model,
        double od, double wall
    ) const;

    // Current standard management
    standards::Standard getCurrentStandard(const std::string& tubeId) const;

    void setCurrentStandard(
        const std::string& tubeId,
        standards::Standard standard
    );

    // Standard details for UI display
    struct StandardDetails {
        standards::Standard standard;
        std::string name;
        standards::ComplianceStatus overallStatus;
        double complianceScore = 0.0;

        // Detail tree
        struct DetailItem {
            std::string name;       // "Diameter", "Wall", "Material", "Tolerance"
            std::string value;
            char status = '?';      // '✓', '⚠', '✗'
            std::string message;
        };

        std::vector<DetailItem> details;
        std::vector<std::string> recommendations;
    };

    StandardDetails getStandardDetails(
        const TubeModel& model,
        double od, double wall,
        standards::Standard standard
    ) const;

    // Compliance report HTML for viewing
    std::string getComplianceReportHTML(
        const TubeModel& model,
        double od, double wall,
        standards::Standard standard
    ) const;

private:
    const standards::StandardsDatabase& database;
    std::map<std::string, standards::Standard> tubeStandardMap;
};

// ========== Collaboration UI (Stubs) ==========

class CollaborationUIStubs {
public:
    explicit CollaborationUIStubs();
    ~CollaborationUIStubs();

    // Stub: Comments
    struct CommentSummary {
        std::string commentId;
        std::string author;
        std::string text;
        int64_t timestamp = 0;
        int replyCount = 0;
        bool resolved = false;
    };

    std::vector<CommentSummary> getComments(const std::string& tubeId) const;

    // Stub: Versions
    struct VersionSummary {
        std::string versionNumber; // e.g., "1.0"
        std::string tag;           // "stable", "release"
        int64_t timestamp = 0;
        std::string author;
    };

    std::vector<VersionSummary> getVersions(const std::string& tubeId) const;

    // Stub: Changes
    struct ChangeSummary {
        std::string changeType;    // "geometry", "material", "machine", etc.
        std::string description;
        int64_t timestamp = 0;
        std::string author;
    };

    std::vector<ChangeSummary> getChanges(const std::string& tubeId) const;

private:
    // Stubs return empty for now
    // Full implementation: Phase 26B/C
};

// ========== Integrated UI Manager ==========

class ProductionUIManager {
public:
    explicit ProductionUIManager(const standards::StandardsDatabase& db);
    ~ProductionUIManager();

    // Main components
    ReportingUIManager reportingUI;
    StandardsUIManager standardsUI;
    CollaborationUIStubs collaborationUI;

    // Unified workflow: Release with all checks
    struct ReleaseWorkflow {
        TubeModel tube;
        double od = 0.0;
        double wall = 0.0;
        std::string approvedBy;
        std::string comments;
    };

    bool executeReleaseWorkflow(
        const ReleaseWorkflow& workflow,
        std::string& outReportId,
        std::vector<std::string>& outBlockers
    );

private:
    const standards::StandardsDatabase& database;
};

}  // namespace bendcore::reporting_ui
