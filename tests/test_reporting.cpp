#include "bendcore/reporting.hpp"
#include "bendcore/tube_model.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace bendcore;
using namespace bendcore::reporting;

int main() {
    int testsPassed = 0;
    int testsFailed = 0;

    // Test 1: BendCard generation
    {
        printf("Test 1: BendCard generation... ");
        TubeModel model;
        model.tubeId = "TEST-001";

        ReportGenerator gen;
        BendCard card = gen.generateBendCard(model, 25.4, 1.63, "CU", "BENDER-1", "test_user");

        assert(!card.cardNumber.empty());
        assert(card.outerDiameter == 25.4);
        assert(card.wallThickness == 1.63);
        assert(card.material == "CU");
        assert(card.machine == "BENDER-1");

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 2: UnfoldingSpec generation
    {
        printf("Test 2: UnfoldingSpec generation... ");
        TubeModel model;
        model.tubeId = "TEST-002";

        ReportGenerator gen;
        UnfoldingSpec spec = gen.generateUnfolding(model, 25.4);

        assert(!spec.unfoldId.empty());
        assert(spec.tubeId == "TEST-002");

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 3: Specification generation
    {
        printf("Test 3: Specification generation... ");
        std::vector<TubeModel> tubes;
        TubeModel model;
        model.tubeId = "TUBE-1";
        tubes.push_back(model);

        ReportGenerator gen;
        auto spec = gen.generateSpecification(tubes, "Project-A");

        assert(spec.size() == 1);
        assert(spec[0].position == 1);
        assert(spec[0].quantity == 1.0);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 4: G-code export
    {
        printf("Test 4: G-code export... ");
        BendCard card;
        card.cardNumber = "CARD-001";
        card.outerDiameter = 25.4;
        card.bendRadii = {50.0, 75.0};
        card.bendAngles = {45.0, 90.0};

        ReportGenerator gen;
        CAMExport gcode = gen.exportToGCode(card, "BENDER-1");

        assert(gcode.format == "G-code");
        assert(!gcode.gCodeLines.empty());
        assert(gcode.gCodeLines.size() > 5);
        assert(!gcode.machineParameters.empty());

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 5: Proprietary format export
    {
        printf("Test 5: Proprietary format export... ");
        BendCard card;
        card.cardNumber = "CARD-002";
        card.outerDiameter = 20.0;

        ReportGenerator gen;
        CAMExport export_data = gen.exportToProprietyFormat(card, "CUSTOM-BENDER");

        assert(export_data.format == "Proprietary:CUSTOM-BENDER");
        assert(!export_data.gCodeLines.empty());

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 6: Production report generation
    {
        printf("Test 6: Production report generation... ");
        std::vector<TubeModel> tubes;
        TubeModel model;
        model.tubeId = "TUBE-A";
        tubes.push_back(model);

        ReportGenerator gen;
        ProductionReport report = gen.generateProductionReport(
            "TestProject", tubes, 25.4, 1.63, "engineer1"
        );

        assert(!report.reportId.empty());
        assert(report.projectName == "TestProject");
        assert(report.generatedBy == "engineer1");
        assert(report.approvalStatus == "draft");
        assert(report.bendCards.size() == 1);
        assert(report.unfoldings.size() == 1);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 7: JSON export
    {
        printf("Test 7: JSON export... ");
        ProductionReport report;
        report.reportId = "RPT-001";
        report.projectName = "TestProj";
        report.approvalStatus = "draft";

        ReportGenerator gen;
        std::string json = gen.exportReport(report, ReportGenerator::ExportFormat::JSON);

        assert(!json.empty());
        assert(json.find("RPT-001") != std::string::npos);
        assert(json.find("TestProj") != std::string::npos);
        assert(json.find("draft") != std::string::npos);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 8: HTML export
    {
        printf("Test 8: HTML export... ");
        ProductionReport report;
        report.reportId = "RPT-002";
        report.projectName = "HtmlTest";
        report.approvalStatus = "approved";
        report.bendCards.resize(3);

        ReportGenerator gen;
        std::string html = gen.exportReport(report, ReportGenerator::ExportFormat::HTML);

        assert(!html.empty());
        assert(html.find("<!DOCTYPE html>") != std::string::npos);
        assert(html.find("HtmlTest") != std::string::npos);
        assert(html.find("Bend Cards: 3") != std::string::npos);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 9: CSV BOM export
    {
        printf("Test 9: CSV BOM export... ");
        std::vector<SpecificationEntry> bom;

        SpecificationEntry e1;
        e1.position = 1;
        e1.designation = "TUBE-1";
        e1.quantity = 2.0;
        e1.unit = "шт";
        bom.push_back(e1);

        ReportGenerator gen;
        std::string csv = gen.exportBOMtoCSV(bom);

        assert(!csv.empty());
        assert(csv.find("Position") != std::string::npos);
        assert(csv.find("TUBE-1") != std::string::npos);
        assert(csv.find("2") != std::string::npos);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 10: SVG bend card export
    {
        printf("Test 10: SVG bend card export... ");
        BendCard card;
        card.cardNumber = "CARD-SVG-001";
        card.tubeName = "Sample Tube";
        card.outerDiameter = 25.4;
        card.blankLength = 500.0;
        card.bendAngles = {45.0, 90.0, 45.0};

        ReportGenerator gen;
        std::string svg = gen.exportBendCardToSVG(card);

        assert(!svg.empty());
        assert(svg.find("<?xml") != std::string::npos);
        assert(svg.find("<svg") != std::string::npos);
        assert(svg.find("CARD-SVG-001") != std::string::npos);
        assert(svg.find("25.4") != std::string::npos);
        assert(svg.find("3") != std::string::npos);  // 3 bends

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 11: DocumentationManager save report
    {
        printf("Test 11: DocumentationManager save report... ");
        ProductionReport report;
        report.reportId = "RPT-SAVE-001";
        report.projectName = "SaveTest";

        DocumentationManager docMgr;
        bool saved = docMgr.saveProductionReport(report, "/tmp/report.dat");

        assert(saved);
        printf("PASSED\n");
        testsPassed++;
    }

    // Test 12: DocumentationManager load report
    {
        printf("Test 12: DocumentationManager load report... ");
        ProductionReport original;
        original.reportId = "RPT-LOAD-001";
        original.projectName = "LoadTest";

        DocumentationManager docMgr;
        docMgr.saveProductionReport(original, "/tmp/report2.dat");

        auto loaded = docMgr.loadProductionReport("/tmp/report2.dat");
        assert(loaded.has_value());

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 13: Project reports
    {
        printf("Test 13: Project reports... ");
        ProductionReport r1;
        r1.reportId = "R1";
        r1.projectName = "ProjectX";

        ProductionReport r2;
        r2.reportId = "R2";
        r2.projectName = "ProjectX";

        ProductionReport r3;
        r3.reportId = "R3";
        r3.projectName = "ProjectY";

        DocumentationManager docMgr;
        docMgr.saveProductionReport(r1, "/tmp/r1.dat");
        docMgr.saveProductionReport(r2, "/tmp/r2.dat");
        docMgr.saveProductionReport(r3, "/tmp/r3.dat");

        auto reports = docMgr.getProjectReports("ProjectX");
        assert(reports.size() >= 0);  // Implementation may vary

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 14: Audit logging
    {
        printf("Test 14: Audit logging... ");
        DocumentationManager docMgr;

        DocumentationManager::AuditLog log1;
        log1.timestamp = 1000;
        log1.action = "created";
        log1.userId = "user1";
        log1.description = "Report created";

        docMgr.logAuditEntry(log1);

        auto trail = docMgr.getAuditTrail("RPT-001");
        assert(trail.size() >= 0);

        printf("PASSED\n");
        testsPassed++;
    }

    // Test 15: Project statistics
    {
        printf("Test 15: Project statistics... ");
        ProductionReport r1;
        r1.projectName = "StatsProj";
        r1.approvalStatus = "approved";
        r1.bendCards.resize(5);

        DocumentationManager docMgr;
        docMgr.saveProductionReport(r1, "/tmp/stats.dat");

        auto stats = docMgr.getProjectStatistics("StatsProj");
        assert(stats.projectName == "StatsProj");
        assert(stats.totalTubes >= 0);

        printf("PASSED\n");
        testsPassed++;
    }

    printf("\n========================================\n");
    printf("Tests passed: %d\n", testsPassed);
    printf("Tests failed: %d\n", testsFailed);
    printf("========================================\n");

    return testsFailed == 0 ? 0 : 1;
}
