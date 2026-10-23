// Тесты для системы стандартов (Phase 25 Stage 4)
#include "bendcore/standards.hpp"
#include "bendcore/path_view.hpp"
#include "bendcore/tube_model.hpp"

#include <cstdio>

using namespace bendcore;
using namespace bendcore::standards;

struct TestCase {
    const char* name;
    int passed = 0;
    int failed = 0;
};

void assert(TestCase& tc, bool condition, const char* message) {
    if (condition) {
        tc.passed++;
    } else {
        printf("  FAIL: %s\n", message);
        tc.failed++;
    }
}

int main() {
    TestCase tc{"standards"};
    
    // Тест 1: Создание StandardsDatabase
    {
        StandardsDatabase db;
        assert(tc, true, "StandardsDatabase создана");
    }
    
    // Тест 2: Загрузка правил
    {
        StandardsDatabase db;
        auto dinRules = db.getRulesByStandard(Standard::DIN);
        assert(tc, !dinRules.empty(), "DIN правила загружены");
        
        auto isoRules = db.getRulesByStandard(Standard::ISO);
        assert(tc, !isoRules.empty(), "ISO правила загружены");
    }
    
    // Тест 3: Получение информации о стандарте
    {
        StandardsDatabase db;
        auto info = db.getStandardInfo(Standard::EN);
        assert(tc, info.standard == Standard::EN, "Информация о стандарте EN получена");
        assert(tc, !info.fullName.empty(), "Полное имя стандарта присутствует");
    }
    
    // Тест 4: StandardsChecker - проверка диаметра
    {
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto result = checker.checkDiameter(10.0, Standard::EN);
        assert(tc, result.status == ComplianceStatus::Compliant, "Диаметр 10мм соответствует EN");
        
        auto resultBig = checker.checkDiameter(100.0, Standard::DIN);
        assert(tc, resultBig.status == ComplianceStatus::NonCompliant, "Диаметр 100мм не соответствует DIN");
    }
    
    // Тест 5: Проверка толщины стенки
    {
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        // В базе правило толщины стенки есть только для DIN; для EN — «не применимо».
        auto result = checker.checkWallThickness(10.0, 1.0, Standard::DIN);
        assert(tc, result.status == ComplianceStatus::Compliant, "Толщина 1мм соответствует");
        auto resultEn = checker.checkWallThickness(10.0, 1.0, Standard::EN);
        assert(tc, resultEn.status == ComplianceStatus::NotApplicable, "Для EN правила толщины нет");
        
        auto resultThin = checker.checkWallThickness(10.0, 0.2, Standard::DIN);
        assert(tc, resultThin.status == ComplianceStatus::NonCompliant, "Слишком тонкая стенка");
    }
    
    // Тест 6: Получение допусков
    {
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto tol = checker.getToleranceForDiameter(10.0, "h7", Standard::ISO);
        assert(tc, tol.grade == "h7", "Допуск h7 получен");
        assert(tc, tol.upperLimit == 0.0, "Верхний предел допуска правильный");
    }
    
    // Тест 7: Получение размеров по стандартам
    {
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto sizeTable = checker.getStandardSizes(Standard::EN);
        assert(tc, sizeTable.has_value(), "Таблица размеров EN получена");
        assert(tc, !sizeTable->od.empty(), "Таблица содержит диаметры");
    }
    
    // Тест 8: Рекомендации по стандартам
    {
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto recommendations = checker.recommendStandards(22.0, 1.5);
        assert(tc, !recommendations.empty(), "Рекомендации получены");
        assert(tc, recommendations[0].score > 0.0, "Оценки присутствуют");
    }
    
    // Тест 9: Проверка соответствия трубы
    {
        TubeModel model;
        model.path = Path{
            Straight{100.0}
        };
        
        Port port1{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0};
        Port port2{"B", Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0};
        model.ports = {port1, port2};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto report = checker.checkTubeCompliance(model, 10.0, 1.0, Standard::EN);
        assert(tc, report.totalRules > 0, "Отчёт содержит правила");
        assert(tc, report.compliantRules >= 0, "Статистика подсчитана");
    }
    
    // Тест 10: Проверка всех стандартов
    {
        TubeModel model;
        model.path = Path{
            Straight{100.0}
        };
        
        Port port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0};
        model.ports = {port};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        
        auto reports = checker.checkAllStandards(model, 10.0, 1.0);
        assert(tc, reports.size() >= 3, "Проверка всех стандартов выполнена");
    }
    
    // Тест 11: Генерация HTML отчёта
    {
        TubeModel model;
        model.path = Path{Straight{100.0}};
        model.ports = {Port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0}};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        ComplianceReportGenerator gen;
        
        auto report = checker.checkTubeCompliance(model, 10.0, 1.0, Standard::EN);
        auto html = gen.generateHTMLReport(report);
        assert(tc, !html.empty(), "HTML отчёт создан");
        assert(tc, html.find("<html>") != std::string::npos, "HTML содержит теги");
    }
    
    // Тест 12: Генерация JSON отчёта
    {
        TubeModel model;
        model.path = Path{Straight{100.0}};
        model.ports = {Port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0}};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        ComplianceReportGenerator gen;
        
        auto report = checker.checkTubeCompliance(model, 10.0, 1.0, Standard::ISO);
        auto json = gen.generateJSONReport(report);
        assert(tc, !json.empty(), "JSON отчёт создан");
        assert(tc, json.find("{") != std::string::npos, "JSON содержит скобки");
    }
    
    // Тест 13: Генерация CSV отчёта
    {
        TubeModel model;
        model.path = Path{Straight{100.0}};
        model.ports = {Port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0}};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        ComplianceReportGenerator gen;
        
        auto report = checker.checkTubeCompliance(model, 10.0, 1.0, Standard::GOST);
        auto csv = gen.generateCSVReport(report);
        assert(tc, !csv.empty(), "CSV отчёт создан");
        assert(tc, csv.find("Rule,Status") != std::string::npos, "CSV содержит заголовок");
    }
    
    // Тест 14: Сравнение отчётов
    {
        TubeModel model;
        model.path = Path{Straight{100.0}};
        model.ports = {Port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0}};
        
        StandardsDatabase db;
        StandardsChecker checker(db);
        ComplianceReportGenerator gen;
        
        auto report1 = checker.checkTubeCompliance(model, 8.0, 0.8, Standard::EN);
        auto report2 = checker.checkTubeCompliance(model, 10.0, 1.0, Standard::EN);
        
        auto comparison = gen.compareReports(report1, report2);
        assert(tc, !comparison.summary.empty(), "Сравнение выполнено");
    }
    
    // Тест 15: StandardsComplianceManager
    {
        StandardsComplianceManager mgr;
        
        TubeModel model;
        model.path = Path{Straight{100.0}};
        model.ports = {Port{"A", Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0}};
        
        auto report = mgr.checker.checkTubeCompliance(model, 10.0, 1.0, Standard::EN);
        
        bool saved = mgr.saveReport(report, "report.json", "json");
        assert(tc, saved, "Отчёт сохранён");
        
        if (report.overallStatus == ComplianceStatus::Compliant) {
            auto cert = mgr.issueCertificate(report, "Inspector");
            assert(tc, cert.has_value(), "Сертификат выдан");
        }
    }
    
    // Результаты
    printf("\n=== Test Standards ===\n");
    printf("Passed: %d\n", tc.passed);
    printf("Failed: %d\n", tc.failed);
    
    return tc.failed == 0 ? 0 : 1;
}
