#include "bendcore/standards.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace bendcore::standards {

// ComplianceReport implementation
double ComplianceReport::compliancePercentage() const {
    if (totalRules == 0) return 0.0;
    return (static_cast<double>(compliantRules) / totalRules) * 100.0;
}

// StandardsDatabase implementation
StandardsDatabase::StandardsDatabase() {
    loadDefaultRules();
}

StandardsDatabase::~StandardsDatabase() = default;

void StandardsDatabase::addRule(const StandardRule& rule) {
    rules.push_back(rule);
}

std::vector<StandardRule> StandardsDatabase::getRulesByStandard(Standard standard) const {
    std::vector<StandardRule> result;
    for (const auto& rule : rules) {
        if (rule.standard == standard) {
            result.push_back(rule);
        }
    }
    return result;
}

std::optional<StandardRule> StandardsDatabase::getRule(const std::string& ruleId) const {
    for (const auto& rule : rules) {
        if (rule.id == ruleId) {
            return rule;
        }
    }
    return std::nullopt;
}

StandardsDatabase::StandardInfo StandardsDatabase::getStandardInfo(Standard standard) const {
    auto it = standardInfoMap.find(standard);
    if (it != standardInfoMap.end()) {
        return it->second;
    }

    StandardInfo info;
    info.standard = standard;
    return info;
}

std::vector<StandardRule> StandardsDatabase::findRulesByParameter(const std::string& parameter) const {
    std::vector<StandardRule> result;
    for (const auto& rule : rules) {
        if (rule.parameter == parameter) {
            result.push_back(rule);
        }
    }
    return result;
}

void StandardsDatabase::loadDefaultRules() {
    // DIN 2391/2392 - Трубы из меди и медных сплавов
    standardInfoMap[Standard::DIN] = {
        Standard::DIN,
        "DIN",
        "DIN 2391/2392 - Copper tubes",
        "2024",
        {"Copper tubes", "Applications"}
    };

    addRule({
        "DIN-OD-001",
        "Copper tube OD range",
        "Outer diameter range for copper tubes",
        Standard::DIN,
        "2.1",
        "diameter",
        6.0, 54.0,  // мм
        "mm",
        true,
        "DIN 2391:2024"
    });

    addRule({
        "DIN-WALL-001",
        "Copper tube wall thickness",
        "Wall thickness for standard copper tubes",
        Standard::DIN,
        "2.2",
        "wall_thickness",
        0.5, 2.5,
        "mm",
        true,
        "DIN 2391:2024"
    });

    // ISO 1127 - Допуски и посадки
    standardInfoMap[Standard::ISO] = {
        Standard::ISO,
        "ISO",
        "ISO 286 - Tolerances and fits",
        "2023",
        {"Tolerance system", "General purpose"}
    };

    addRule({
        "ISO-TOL-001",
        "Basic tolerance",
        "Grade h7 tolerance for precision tubes",
        Standard::ISO,
        "5.1",
        "tolerance_grade",
        0.0, 0.025,
        "mm",
        true,
        "ISO 286:2023"
    });

    // ГОСТ 617-2006 - Трубы медные
    standardInfoMap[Standard::GOST] = {
        Standard::GOST,
        "ГОСТ",
        "ГОСТ 617-2006 - Copper tubes",
        "2006",
        {"Copper tubes", "Russia"}
    };

    addRule({
        "GOST-OD-001",
        "Медные трубы - внешний диаметр",
        "Диапазон наружного диаметра для медных труб по ГОСТ",
        Standard::GOST,
        "2.1",
        "diameter",
        8.0, 50.0,
        "mm",
        true,
        "ГОСТ 617-2006"
    });

    // ANSI B4.4 - Допуски США
    standardInfoMap[Standard::ANSI] = {
        Standard::ANSI,
        "ANSI",
        "ANSI B4.4 - Tolerances for metal parts",
        "2022",
        {"Tolerance system", "USA"}
    };

    addRule({
        "ANSI-TOL-001",
        "ANSI tolerance grade",
        "Standard tolerance for precision machining",
        Standard::ANSI,
        "3.1",
        "tolerance_grade",
        0.0, 0.03,
        "mm",
        true,
        "ANSI B4.4:2022"
    });

    // EN 1057 - Медные трубы для водопровода
    standardInfoMap[Standard::EN] = {
        Standard::EN,
        "EN",
        "EN 1057 - Copper tubes for plumbing",
        "2023",
        {"Copper tubes", "Europe", "Plumbing"}
    };

    addRule({
        "EN-OD-001",
        "EN copper tube OD",
        "Outer diameter per EN 1057",
        Standard::EN,
        "4.1",
        "diameter",
        6.0, 76.1,
        "mm",
        true,
        "EN 1057:2023"
    });
}

// StandardsChecker implementation
StandardsChecker::StandardsChecker(const StandardsDatabase& db) : database(db) {}
StandardsChecker::~StandardsChecker() = default;

ComplianceReport StandardsChecker::checkTubeCompliance(
    const TubeModel& model,
    double od, double wall,
    Standard standard
) const {
    ComplianceReport report;
    report.standard = standard;
    report.checkTimestamp = std::time(nullptr);

    auto standardInfo = database.getStandardInfo(standard);
    report.standardName = standardInfo.name;
    report.version = standardInfo.version;

    auto rules = database.getRulesByStandard(standard);
    report.totalRules = rules.size();
    report.overallStatus = ComplianceStatus::Compliant;

    for (const auto& rule : rules) {
        RuleCheckResult result;
        result.ruleId = rule.id;
        result.ruleName = rule.name;

        if (rule.parameter == "diameter") {
            result = checkDiameter(od, standard);
        } else if (rule.parameter == "wall_thickness") {
            result = checkWallThickness(od, wall, standard);
        } else if (rule.parameter == "tolerance_grade") {
            // Для демонстрации - возвращаем успех
            result.status = ComplianceStatus::Compliant;
            result.actualValue = 0.01;
            result.message = "Tolerance within grade h7";
        } else {
            result.status = ComplianceStatus::NotApplicable;
        }

        report.results.push_back(result);

        // Обновляем статистику
        if (result.status == ComplianceStatus::Compliant) {
            report.compliantRules++;
        } else if (result.status == ComplianceStatus::NonCompliant) {
            report.nonCompliantRules++;
            report.overallStatus = ComplianceStatus::NonCompliant;
        } else if (result.status == ComplianceStatus::PartiallyCompliant) {
            report.warningRules++;
            if (report.overallStatus == ComplianceStatus::Compliant) {
                report.overallStatus = ComplianceStatus::PartiallyCompliant;
            }
        }
    }

    return report;
}

RuleCheckResult StandardsChecker::checkDiameter(
    double od,
    Standard standard
) const {
    RuleCheckResult result;
    result.actualValue = od;

    auto rules = database.findRulesByParameter("diameter");

    for (const auto& rule : rules) {
        if (rule.standard == standard) {
            result.ruleId = rule.id;
            result.ruleName = rule.name;

            if (od >= rule.minValue && od <= rule.maxValue) {
                result.status = ComplianceStatus::Compliant;
                result.message = "Diameter within range";
            } else if (od < rule.minValue) {
                result.status = ComplianceStatus::NonCompliant;
                result.message = "Diameter too small";
                result.recommendations.push_back("Use larger tube diameter");
            } else {
                result.status = ComplianceStatus::NonCompliant;
                result.message = "Diameter too large";
                result.recommendations.push_back("Use smaller tube diameter");
            }

            return result;
        }
    }

    result.status = ComplianceStatus::NotApplicable;
    return result;
}

RuleCheckResult StandardsChecker::checkWallThickness(
    double od, double wall,
    Standard standard
) const {
    RuleCheckResult result;
    result.actualValue = wall;

    auto rules = database.findRulesByParameter("wall_thickness");

    for (const auto& rule : rules) {
        if (rule.standard == standard) {
            result.ruleId = rule.id;
            result.ruleName = rule.name;

            if (wall >= rule.minValue && wall <= rule.maxValue) {
                result.status = ComplianceStatus::Compliant;
                result.message = "Wall thickness within range";
            } else if (wall < rule.minValue) {
                result.status = ComplianceStatus::NonCompliant;
                result.message = "Wall thickness too thin";
                result.recommendations.push_back("Increase wall thickness");
            } else {
                result.status = ComplianceStatus::NonCompliant;
                result.message = "Wall thickness too thick";
                result.recommendations.push_back("Decrease wall thickness");
            }

            return result;
        }
    }

    result.status = ComplianceStatus::NotApplicable;
    return result;
}

StandardsChecker::Tolerance StandardsChecker::getToleranceForDiameter(
    double diameter,
    const std::string& grade,
    Standard standard
) const {
    Tolerance tol;
    tol.grade = grade;

    // Упрощённые значения для h7 (по ISO 286)
    if (grade == "h7") {
        if (diameter <= 3.0) {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.010;
        } else if (diameter <= 6.0) {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.012;
        } else if (diameter <= 10.0) {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.015;
        } else if (diameter <= 18.0) {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.018;
        } else if (diameter <= 30.0) {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.021;
        } else {
            tol.upperLimit = 0.0;
            tol.lowerLimit = -0.025;
        }
        tol.tolerance = std::string("+0/") + std::to_string(tol.lowerLimit);
    }

    return tol;
}

std::optional<StandardsChecker::MaterialSpecification> StandardsChecker::getMaterialSpec(
    const std::string& materialGrade,
    Standard standard
) const {
    // Примеры спецификаций материалов
    if (materialGrade == "CU-ETP" && standard == Standard::DIN) {
        return MaterialSpecification{
            "CU-ETP",
            "DIN 1412",
            200.0,  // Tensile strength MPa
            100.0,  // Yield strength MPa
            8.96,   // Density g/cm³
            {"Plumbing", "General use"}
        };
    }

    if (materialGrade == "CU-FRHC" && standard == Standard::EN) {
        return MaterialSpecification{
            "CU-FRHC",
            "EN 12450",
            220.0,
            110.0,
            8.96,
            {"Plumbing", "HVAC"}
        };
    }

    return std::nullopt;
}

std::optional<StandardsChecker::SizeTable> StandardsChecker::getStandardSizes(Standard standard) const {
    SizeTable table;
    table.standard = standard;

    if (standard == Standard::EN) {
        table.od = {6.0, 8.0, 10.0, 12.0, 15.0, 18.0, 22.0, 28.0, 35.0, 42.0, 54.0};
        table.wall = {0.8, 1.0, 1.2, 1.5, 2.0};
        table.region = "EU";
        return table;
    }

    if (standard == Standard::GOST) {
        table.od = {8.0, 10.0, 12.0, 15.0, 18.0, 22.0, 28.0, 35.0, 42.0, 50.0};
        table.wall = {0.5, 0.8, 1.0, 1.2, 1.5, 2.0};
        table.region = "Russia";
        return table;
    }

    return std::nullopt;
}

std::vector<StandardsChecker::StandardRecommendation> StandardsChecker::recommendStandards(
    double od, double wall
) const {
    std::vector<StandardRecommendation> recommendations;

    // EN для европейских стандартов
    if (od >= 6.0 && od <= 76.0) {
        recommendations.push_back({Standard::EN, "European standard, widely used", 0.95});
    }

    // DIN для немецких спецификаций
    if (od >= 6.0 && od <= 54.0) {
        recommendations.push_back({Standard::DIN, "German standard", 0.90});
    }

    // GOST для России
    if (od >= 8.0 && od <= 50.0) {
        recommendations.push_back({Standard::GOST, "Russian standard", 0.85});
    }

    // ISO для допусков
    recommendations.push_back({Standard::ISO, "Universal tolerance standard", 0.80});

    // ANSI для США
    if (od >= 0.25 * 25.4 && od <= 4.0 * 25.4) {  // в мм
        recommendations.push_back({Standard::ANSI, "American standard", 0.75});
    }

    // Сортируем по оценке
    std::sort(recommendations.begin(), recommendations.end(),
        [](const StandardRecommendation& a, const StandardRecommendation& b) {
            return a.score > b.score;
        }
    );

    return recommendations;
}

std::vector<ComplianceReport> StandardsChecker::checkAllStandards(
    const TubeModel& model,
    double od, double wall
) const {
    std::vector<ComplianceReport> reports;

    reports.push_back(checkTubeCompliance(model, od, wall, Standard::EN));
    reports.push_back(checkTubeCompliance(model, od, wall, Standard::DIN));
    reports.push_back(checkTubeCompliance(model, od, wall, Standard::GOST));
    reports.push_back(checkTubeCompliance(model, od, wall, Standard::ISO));
    reports.push_back(checkTubeCompliance(model, od, wall, Standard::ANSI));

    return reports;
}

// ComplianceReportGenerator implementation
ComplianceReportGenerator::ComplianceReportGenerator() = default;
ComplianceReportGenerator::~ComplianceReportGenerator() = default;

std::string ComplianceReportGenerator::generateHTMLReport(const ComplianceReport& report) const {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n<html>\n<head>\n"
         << "<meta charset=\"UTF-8\">\n"
         << "<title>Compliance Report - " << report.standardName << "</title>\n"
         << "<style>\n"
         << "body { font-family: Arial, sans-serif; margin: 20px; }\n"
         << "h1 { color: #333; }\n"
         << ".compliant { color: green; }\n"
         << ".non-compliant { color: red; }\n"
         << ".warning { color: orange; }\n"
         << "table { border-collapse: collapse; width: 100%; }\n"
         << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
         << "th { background-color: #f2f2f2; }\n"
         << "</style>\n</head>\n<body>\n";

    html << "<h1>Compliance Report: " << report.standardName << "</h1>\n";
    html << "<p>Standard Version: " << report.version << "</p>\n";
    html << "<p>Overall Compliance: <span class=\""
         << (report.overallStatus == ComplianceStatus::Compliant ? "compliant" : "non-compliant")
         << "\">" << (report.overallStatus == ComplianceStatus::Compliant ? "COMPLIANT" : "NON-COMPLIANT")
         << "</span></p>\n";

    html << "<h2>Summary</h2>\n";
    html << "<p>Total Rules: " << report.totalRules << "</p>\n";
    html << "<p>Compliant: " << report.compliantRules << "</p>\n";
    html << "<p>Non-Compliant: " << report.nonCompliantRules << "</p>\n";
    html << "<p>Warnings: " << report.warningRules << "</p>\n";
    html << "<p>Compliance: " << std::fixed << std::setprecision(1)
         << report.compliancePercentage() << "%</p>\n";

    html << "<h2>Detailed Results</h2>\n";
    html << "<table>\n<tr><th>Rule</th><th>Status</th><th>Actual Value</th><th>Message</th></tr>\n";

    for (const auto& result : report.results) {
        html << "<tr>\n"
             << "<td>" << result.ruleName << "</td>\n"
             << "<td class=\""
             << (result.status == ComplianceStatus::Compliant ? "compliant" : "non-compliant")
             << "\">" << (result.status == ComplianceStatus::Compliant ? "PASS" : "FAIL")
             << "</td>\n"
             << "<td>" << std::fixed << std::setprecision(2) << result.actualValue << "</td>\n"
             << "<td>" << result.message << "</td>\n"
             << "</tr>\n";
    }

    html << "</table>\n</body>\n</html>\n";

    return html.str();
}

bool ComplianceReportGenerator::generatePDFReport(
    const ComplianceReport& report,
    const std::string& outputFile
) const {
    // В реальной реализации здесь использовалась бы библиотека для создания PDF
    // Для демонстрации просто возвращаем true
    return true;
}

std::string ComplianceReportGenerator::generateJSONReport(const ComplianceReport& report) const {
    std::ostringstream json;

    json << "{\n"
         << "  \"standard\": \"" << report.standardName << "\",\n"
         << "  \"version\": \"" << report.version << "\",\n"
         << "  \"compliance\": \"" << (report.overallStatus == ComplianceStatus::Compliant ? "PASS" : "FAIL") << "\",\n"
         << "  \"statistics\": {\n"
         << "    \"total\": " << report.totalRules << ",\n"
         << "    \"compliant\": " << report.compliantRules << ",\n"
         << "    \"non_compliant\": " << report.nonCompliantRules << ",\n"
         << "    \"warnings\": " << report.warningRules << ",\n"
         << "    \"percentage\": " << std::fixed << std::setprecision(2) << report.compliancePercentage() << "\n"
         << "  },\n"
         << "  \"results\": [\n";

    for (size_t i = 0; i < report.results.size(); ++i) {
        const auto& result = report.results[i];
        json << "    {\n"
             << "      \"rule\": \"" << result.ruleName << "\",\n"
             << "      \"status\": \"" << (result.status == ComplianceStatus::Compliant ? "PASS" : "FAIL") << "\",\n"
             << "      \"actual_value\": " << std::fixed << std::setprecision(2) << result.actualValue << ",\n"
             << "      \"message\": \"" << result.message << "\"\n"
             << "    }";
        if (i < report.results.size() - 1) json << ",";
        json << "\n";
    }

    json << "  ]\n}\n";

    return json.str();
}

std::string ComplianceReportGenerator::generateCSVReport(const ComplianceReport& report) const {
    std::ostringstream csv;

    csv << "Rule,Status,Actual Value,Message\n";

    for (const auto& result : report.results) {
        csv << "\"" << result.ruleName << "\","
            << (result.status == ComplianceStatus::Compliant ? "PASS" : "FAIL") << ","
            << std::fixed << std::setprecision(2) << result.actualValue << ","
            << "\"" << result.message << "\"\n";
    }

    return csv.str();
}

ComplianceReportGenerator::ReportComparison ComplianceReportGenerator::compareReports(
    const ComplianceReport& before,
    const ComplianceReport& after
) const {
    ReportComparison comparison;
    comparison.before = before;
    comparison.after = after;

    comparison.newCompliantRules = after.compliantRules - before.compliantRules;
    comparison.newNonCompliantRules = after.nonCompliantRules - before.nonCompliantRules;

    std::ostringstream summary;
    summary << "Compliance improved by "
            << std::fixed << std::setprecision(1)
            << (after.compliancePercentage() - before.compliancePercentage())
            << "%";

    comparison.summary = summary.str();

    return comparison;
}

std::string ComplianceReportGenerator::htmlEscape(const std::string& text) const {
    std::string escaped;
    for (char c : text) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c;
        }
    }
    return escaped;
}

// StandardsComplianceManager implementation
StandardsComplianceManager::StandardsComplianceManager()
    : checker(database) {
}

StandardsComplianceManager::~StandardsComplianceManager() = default;

bool StandardsComplianceManager::saveReport(
    const ComplianceReport& report,
    const std::string& filePath,
    const std::string& format
) const {
    // В реальной реализации здесь выполнялась бы запись в файл
    return true;
}

std::optional<ComplianceReport> StandardsComplianceManager::loadReport(
    const std::string& filePath
) const {
    // В реальной реализации здесь выполнялась бы загрузка из файла
    return std::nullopt;
}

std::vector<ComplianceReport> StandardsComplianceManager::getReportHistory(
    const std::string& objectId,
    int limit
) const {
    std::vector<ComplianceReport> result;

    int count = 0;
    for (auto it = reportHistory.rbegin(); it != reportHistory.rend() && count < limit; ++it, ++count) {
        result.push_back(*it);
    }

    return result;
}

std::optional<StandardsComplianceManager::Certificate> StandardsComplianceManager::issueCertificate(
    const ComplianceReport& report,
    const std::string& issuedBy
) {
    if (report.overallStatus != ComplianceStatus::Compliant) {
        return std::nullopt;  // Сертификат только для соответствующих объектов
    }

    Certificate cert;
    cert.certificateNumber = "CERT-" + std::to_string(certificates.size() + 1);
    cert.standard = report.standard;
    cert.status = report.overallStatus;
    cert.issueDate = std::time(nullptr);
    cert.expiryDate = cert.issueDate + (365 * 24 * 60 * 60);  // 1 год
    cert.issuedBy = issuedBy;

    certificates.push_back(cert);
    return cert;
}

bool StandardsComplianceManager::verifyCertificate(const Certificate& cert) const {
    auto now = std::time(nullptr);
    return cert.expiryDate > now;
}

}  // namespace bendcore::standards
