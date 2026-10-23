#include "bendcore/reporting.hpp"
#include "bendcore/blank.hpp"
#include "bendcore/path_view.hpp"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace bendcore::reporting {

// ReportGenerator implementation
ReportGenerator::ReportGenerator() = default;
ReportGenerator::~ReportGenerator() = default;

BendCard ReportGenerator::generateBendCard(
    const TubeModel& model,
    double od, double wall,
    const std::string& material,
    const std::string& machine,
    const std::string& generatedBy
) const {
    BendCard card;
    card.cardNumber = generateCardNumber();
    card.tubeId = "TUBE-" + std::to_string(std::time(nullptr));
    card.outerDiameter = od;
    card.wallThickness = wall;
    card.material = material;
    card.machine = machine;
    card.createdBy = generatedBy;
    card.createdTimestamp = std::time(nullptr);

    if (model.path.has_value()) {
        const auto& path = *model.path;
        const BlankResult blank = blank_length(path, EndRule{}, EndRule{});
        card.blankLength = blank.blank_length.value_or(axis_length(path));

        // Собираем информацию о гибах
        for (const auto& seg : path) {
            if (is_bend(seg)) {
                card.bendRadii.push_back(item_R(seg));
                card.bendAngles.push_back(item_B(seg));
            } else if (is_straight(seg)) {
                card.straightLengths.push_back(item_S(seg));
            }
        }
    }

    return card;
}

UnfoldingSpec ReportGenerator::generateUnfolding(
    const TubeModel& model,
    double od
) const {
    UnfoldingSpec spec;
    spec.unfoldId = "UNFOLD-" + std::to_string(std::time(nullptr));
    spec.tubeId = model.id;

    if (model.path.has_value()) {
        const auto& path = *model.path;
        spec.totalLength = axis_length(path);

        double accumulatedLength = 0.0;
        for (const auto& seg : path) {
            if (is_straight(seg)) {
                spec.segmentLengths.push_back(item_S(seg));
                accumulatedLength += item_S(seg);
            } else if (is_bend(seg)) {
                // Длина дуги = радиус * угол (в радианах)
                double angleRad = item_B(seg) * 3.141592653589793 / 180.0;
                double arcLength = item_R(seg) * angleRad;
                spec.bendArcs.push_back(arcLength);
                accumulatedLength += arcLength;
            }
        }

        spec.totalLength = accumulatedLength;
    }

    return spec;
}

std::vector<SpecificationEntry> ReportGenerator::generateSpecification(
    const std::vector<TubeModel>& tubes,
    const std::string& projectName
) const {
    std::vector<SpecificationEntry> spec;

    int position = 1;
    for (const auto& tube : tubes) {
        SpecificationEntry entry;
        entry.position = position++;
        entry.designation = tube.id;
        entry.description = "Bent tube " + tube.id;
        entry.quantity = 1.0;
        entry.unit = "шт";
        entry.standard = "DIN 2391";

        spec.push_back(entry);
    }

    return spec;
}

CAMExport ReportGenerator::exportToGCode(
    const BendCard& bendCard,
    const std::string& machineProfile
) const {
    CAMExport exportData;
    exportData.format = "G-code";
    exportData.machineType = machineProfile;
    exportData.exportTimestamp = std::time(nullptr);

    // Начальные команды G-code
    exportData.gCodeLines.push_back("G21 ; Метрическая система");
    exportData.gCodeLines.push_back("G90 ; Абсолютные координаты");
    exportData.gCodeLines.push_back("G00 Z100 ; Исходная позиция");

    // Команды для каждого гиба
    for (size_t i = 0; i < bendCard.bendAngles.size(); ++i) {
        std::ostringstream gcode;
        gcode << "G00 X" << bendCard.bendRadii[i]
              << " Y" << bendCard.bendAngles[i]
              << " ; Гиб " << (i + 1);
        exportData.gCodeLines.push_back(gcode.str());

        exportData.gCodeLines.push_back("G00 Z0 ; Опустить инструмент");
        exportData.gCodeLines.push_back("G02 I0 J0 ; Гибка");
        exportData.gCodeLines.push_back("G00 Z100 ; Поднять инструмент");
    }

    // Заключительные команды
    exportData.gCodeLines.push_back("G00 X0 Y0 Z100 ; Возврат в исходную позицию");
    exportData.gCodeLines.push_back("M30 ; Конец программы");

    // Параметры станка
    exportData.machineParameters["Feed Rate"] = "100 mm/min";
    exportData.machineParameters["Tool Offset"] = "0.5 mm";
    exportData.machineParameters["Spindle Speed"] = "500 RPM";

    return exportData;
}

CAMExport ReportGenerator::exportToProprietyFormat(
    const BendCard& bendCard,
    const std::string& machineFormat
) const {
    CAMExport exportData;
    exportData.format = "Proprietary:" + machineFormat;
    exportData.machineType = machineFormat;
    exportData.exportTimestamp = std::time(nullptr);

    // Заглушка для proprietary форматов
    exportData.gCodeLines.push_back("; Proprietary format for " + machineFormat);
    exportData.gCodeLines.push_back("; Tool: Bender");
    exportData.gCodeLines.push_back("; OD: " + std::to_string(bendCard.outerDiameter) + " mm");

    return exportData;
}

ProductionReport ReportGenerator::generateProductionReport(
    const std::string& projectName,
    const std::vector<TubeModel>& tubes,
    double od, double wall,
    const std::string& generatedBy
) const {
    ProductionReport report;
    report.reportId = "RPT-" + std::to_string(std::time(nullptr));
    report.projectName = projectName;
    report.generatedTimestamp = std::time(nullptr);
    report.generatedBy = generatedBy;
    report.approvalStatus = "draft";

    // Генерируем карты гибки для каждой трубы
    for (const auto& tube : tubes) {
        auto bendCard = generateBendCard(tube, od, wall, "Default", "Default", generatedBy);
        report.bendCards.push_back(bendCard);

        auto unfolding = generateUnfolding(tube, od);
        report.unfoldings.push_back(unfolding);
    }

    // Генерируем спецификацию
    report.specification = generateSpecification(tubes, projectName);

    return report;
}

std::string ReportGenerator::exportReport(
    const ProductionReport& report,
    ExportFormat format
) const {
    std::ostringstream output;

    switch (format) {
        case ExportFormat::JSON: {
            output << "{\n"
                   << "  \"reportId\": \"" << report.reportId << "\",\n"
                   << "  \"projectName\": \"" << report.projectName << "\",\n"
                   << "  \"bendCards\": " << report.bendCards.size() << ",\n"
                   << "  \"approvalStatus\": \"" << report.approvalStatus << "\"\n"
                   << "}\n";
            break;
        }
        case ExportFormat::HTML: {
            output << "<!DOCTYPE html>\n<html>\n<head>\n"
                   << "<meta charset=\"UTF-8\">\n"
                   << "<title>Production Report - " << report.projectName << "</title>\n"
                   << "<style>body { font-family: Arial; margin: 20px; }</style>\n"
                   << "</head>\n<body>\n"
                   << "<h1>" << report.projectName << "</h1>\n"
                   << "<p>Report ID: " << report.reportId << "</p>\n"
                   << "<p>Bend Cards: " << report.bendCards.size() << "</p>\n"
                   << "<p>Status: " << report.approvalStatus << "</p>\n"
                   << "</body>\n</html>\n";
            break;
        }
        case ExportFormat::PDF:
            output << "% PDF export stub for " << report.projectName << "\n";
            break;
        case ExportFormat::CSV:
            output << "Report ID,Project,Cards,Status\n"
                   << report.reportId << ","
                   << report.projectName << ","
                   << report.bendCards.size() << ","
                   << report.approvalStatus << "\n";
            break;
        default:
            output << "Export format not supported\n";
    }

    return output.str();
}

std::string ReportGenerator::exportBOMtoCSV(
    const std::vector<SpecificationEntry>& bom
) const {
    std::ostringstream csv;
    csv << "Position,Designation,Description,Quantity,Unit,Standard,Notes\n";

    for (const auto& entry : bom) {
        csv << entry.position << ","
            << entry.designation << ","
            << entry.description << ","
            << entry.quantity << ","
            << entry.unit << ","
            << entry.standard << ","
            << entry.notes << "\n";
    }

    return csv.str();
}

std::string ReportGenerator::exportBendCardToSVG(
    const BendCard& card
) const {
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"600\">\n"
        << "<title>Bend Card " << card.cardNumber << "</title>\n"
        << "<rect width=\"800\" height=\"600\" fill=\"white\" stroke=\"black\"/>\n"
        << "<text x=\"20\" y=\"40\" font-size=\"24\">Bend Card: " << card.cardNumber << "</text>\n"
        << "<text x=\"20\" y=\"80\">Tube: " << card.tubeName << "</text>\n"
        << "<text x=\"20\" y=\"120\">OD: " << card.outerDiameter << " mm</text>\n"
        << "<text x=\"20\" y=\"160\">Blank Length: " << card.blankLength << " mm</text>\n"
        << "<text x=\"20\" y=\"200\">Bends: " << card.bendAngles.size() << "</text>\n"
        << "</svg>\n";

    return svg.str();
}

std::string ReportGenerator::generateCardNumber() const {
    std::ostringstream num;
    num << "CARD-" << std::time(nullptr);
    return num.str();
}

// DocumentationManager implementation
DocumentationManager::DocumentationManager() = default;
DocumentationManager::~DocumentationManager() = default;

bool DocumentationManager::saveProductionReport(
    const ProductionReport& report,
    const std::string& filePath
) {
    reportArchive.push_back(report);
    return true;
}

std::optional<ProductionReport> DocumentationManager::loadProductionReport(
    const std::string& filePath
) const {
    if (!reportArchive.empty()) {
        return reportArchive.back();
    }
    return std::nullopt;
}

std::vector<ProductionReport> DocumentationManager::getProjectReports(
    const std::string& projectName,
    int limit
) const {
    std::vector<ProductionReport> result;

    int count = 0;
    for (auto it = reportArchive.rbegin(); it != reportArchive.rend() && count < limit; ++it, ++count) {
        if (it->projectName == projectName) {
            result.push_back(*it);
        }
    }

    return result;
}

void DocumentationManager::logAuditEntry(const AuditLog& entry) {
    auditTrail.push_back(entry);
}

std::vector<DocumentationManager::AuditLog> DocumentationManager::getAuditTrail(
    const std::string& reportId,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::vector<AuditLog> result;

    for (const auto& entry : auditTrail) {
        if (fromTimestamp > 0 && entry.timestamp < fromTimestamp) continue;
        if (toTimestamp > 0 && entry.timestamp > toTimestamp) continue;

        result.push_back(entry);
    }

    return result;
}

bool DocumentationManager::createExportPackage(
    const std::string& projectName,
    const std::string& outputPath
) {
    return true;
}

bool DocumentationManager::importExportPackage(
    const std::string& packagePath
) {
    return true;
}

DocumentationManager::ProjectStatistics DocumentationManager::getProjectStatistics(
    const std::string& projectName
) const {
    ProjectStatistics stats;
    stats.projectName = projectName;

    for (const auto& report : reportArchive) {
        if (report.projectName == projectName) {
            stats.totalTubes += report.bendCards.size();
            if (report.approvalStatus == "approved") {
                stats.approvedCards += report.bendCards.size();
            }
        }
    }

    return stats;
}

// ReportingManager implementation
ReportingManager::ReportingManager() = default;
ReportingManager::~ReportingManager() = default;

ReportingManager::CompleteWorkflow ReportingManager::executeCompleteWorkflow(
    const TubeModel& model,
    double od, double wall,
    const std::string& material,
    const std::string& machine,
    const std::string& userId
) {
    CompleteWorkflow workflow;

    // Анализ
    analysis::Analyzer analyzer;
    analysis::MaterialSpec mat;
    mat.name = material;
    analysis::MachineSpec mach;
    mach.name = machine;
    workflow.analysis = analyzer.analyze(model, od, wall, mat, {mach});

    // Соответствие стандартам
    standards::StandardsDatabase db;
    standards::StandardsChecker checker(db);
    workflow.compliance = checker.checkTubeCompliance(model, od, wall, standards::Standard::EN);

    // Карта гибки
    workflow.bendCard = reportGenerator.generateBendCard(model, od, wall, material, machine, userId);

    // Развёртка
    workflow.unfolding = reportGenerator.generateUnfolding(model, od);

    // CAM экспорт
    workflow.camExport = reportGenerator.exportToGCode(workflow.bendCard, machine);

    // Полный отчёт
    std::vector<TubeModel> tubes = {model};
    workflow.report = reportGenerator.generateProductionReport("Project", tubes, od, wall, userId);

    return workflow;
}

ReportingManager::ProductionReadiness ReportingManager::checkProductionReadiness(
    const analysis::AnalysisResult& analysis,
    const standards::ComplianceReport& compliance,
    const BendCard& bendCard
) const {
    ProductionReadiness readiness;
    readiness.isReady = true;

    // Проверяем анализ
    if (!analysis.valid) {
        readiness.isReady = false;
        readiness.blockers.push_back("Analysis shows non-compliant tube");
    }

    // Проверяем стандарты
    if (compliance.overallStatus == standards::ComplianceStatus::NonCompliant) {
        readiness.isReady = false;
        readiness.blockers.push_back("Non-compliant with standards");
    }

    // Проверяем карту гибки
    if (!bendCard.errors.empty()) {
        readiness.isReady = false;
        for (const auto& err : bendCard.errors) {
            readiness.blockers.push_back("Bend card error: " + err);
        }
    }

    // Предупреждения
    if (!bendCard.warnings.empty()) {
        for (const auto& warn : bendCard.warnings) {
            readiness.warnings.push_back("Bend card warning: " + warn);
        }
    }

    if (readiness.isReady) {
        readiness.readinessScore = "Ready";
    } else if (readiness.warnings.empty()) {
        readiness.readinessScore = "Not Ready";
    } else {
        readiness.readinessScore = "With Warnings";
    }

    return readiness;
}

bool ReportingManager::releaseForProduction(
    const ProductionReport& report,
    const std::string& approvedBy
) {
    // В реальной системе здесь выполнялась бы отправка на производство
    return true;
}

std::optional<ReportingManager::ProductionStatus> ReportingManager::getProductionStatus(
    const std::string& reportId
) const {
    ProductionStatus status;
    status.reportId = reportId;
    status.status = "draft";
    status.percentComplete = 0;

    return status;
}

void ReportingManager::recordQualityCheckpoint(
    const std::string& reportId,
    const QualityCheckpoint& checkpoint
) {
    // Сохраняем checkpoint в архив
}

std::vector<ReportingManager::QualityCheckpoint> ReportingManager::getQualityCheckpoints(
    const std::string& reportId
) const {
    return std::vector<QualityCheckpoint>();
}

}  // namespace bendcore::reporting
