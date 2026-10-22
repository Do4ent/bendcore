// Система проверки соответствия стандартам
// Phase 25 Stage 4: Standards Integration
#pragma once

#include "bendcore/tube_model.hpp"
#include "bendcore/vec3.hpp"

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace bendcore::standards {

// Поддерживаемые стандарты
enum class Standard {
    DIN,    // DIN 2391, DIN 2392 (трубы)
    ISO,    // ISO 1127, ISO 286 (допуски)
    GOST,   // ГОСТ 617-2006, ГОСТ 9940 (трубы)
    ANSI,   // ANSI B4.4 (допуски и посадки)
    EN      // EN 1057, EN 10255 (медные и стальные трубы)
};

// Статус соответствия
enum class ComplianceStatus {
    Compliant,      // Соответствует стандарту
    NonCompliant,   // Не соответствует
    PartiallyCompliant,  // Частично соответствует (с предупреждениями)
    NotApplicable   // Стандарт неприменим
};

// Правило стандарта
struct StandardRule {
    std::string id;                    // Уникальный ID правила
    std::string name;                  // Имя правила
    std::string description;           // Описание
    Standard standard = Standard::ISO; // Стандарт
    std::string clause;                // Пункт стандарта (например, "2.1.3")
    std::string parameter;             // Проверяемый параметр
    double minValue = 0.0;             // Минимальное значение
    double maxValue = 0.0;             // Максимальное значение
    std::string unit;                  // Единица измерения
    bool isMandatory = true;           // Обязательное ли правило
    std::string reference;             // Ссылка на документ
};

// Результат проверки правила
struct RuleCheckResult {
    std::string ruleId;
    std::string ruleName;
    ComplianceStatus status = ComplianceStatus::NotApplicable;
    double actualValue = 0.0;
    std::string message;
    std::vector<std::string> recommendations;
};

// Отчёт о соответствии стандарту
struct ComplianceReport {
    Standard standard = Standard::ISO;
    std::string standardName;
    std::string version;               // Версия стандарта
    ComplianceStatus overallStatus = ComplianceStatus::NotApplicable;
    std::vector<RuleCheckResult> results;
    int64_t checkTimestamp = 0;
    std::string checkedBy;
    std::string notes;

    // Статистика
    int totalRules = 0;
    int compliantRules = 0;
    int nonCompliantRules = 0;
    int warningRules = 0;

    double compliancePercentage() const;
};

// База правил стандартов
class StandardsDatabase {
public:
    explicit StandardsDatabase();
    ~StandardsDatabase();

    // Добавление правила
    void addRule(const StandardRule& rule);

    // Получение правил по стандарту
    std::vector<StandardRule> getRulesByStandard(Standard standard) const;

    // Получение конкретного правила
    std::optional<StandardRule> getRule(const std::string& ruleId) const;

    // Получение параметров стандарта
    struct StandardInfo {
        Standard standard;
        std::string name;
        std::string fullName;
        std::string version;
        std::vector<std::string> scope;  // Область применения
        int totalRules = 0;
    };

    StandardInfo getStandardInfo(Standard standard) const;

    // Поиск правил по параметру
    std::vector<StandardRule> findRulesByParameter(const std::string& parameter) const;

    // Загрузка базовых правил
    void loadDefaultRules();

private:
    std::vector<StandardRule> rules;
    std::map<Standard, StandardInfo> standardInfoMap;
};

// Проверяющий стандарты
class StandardsChecker {
public:
    explicit StandardsChecker(const StandardsDatabase& db);
    ~StandardsChecker();

    // Проверка соответствия трубы стандарту
    ComplianceReport checkTubeCompliance(
        const TubeModel& model,
        double od, double wall,
        Standard standard
    ) const;

    // Проверка диаметра
    RuleCheckResult checkDiameter(
        double od,
        Standard standard
    ) const;

    // Проверка толщины стенки
    RuleCheckResult checkWallThickness(
        double od, double wall,
        Standard standard
    ) const;

    // Проверка допусков (по ISO 286)
    struct Tolerance {
        std::string grade;  // h7, H8, и т.д.
        double upperLimit = 0.0;
        double lowerLimit = 0.0;
        std::string tolerance;  // +0.025/-0.000
    };

    Tolerance getToleranceForDiameter(
        double diameter,
        const std::string& grade,
        Standard standard
    ) const;

    // Проверка материала по стандарту
    struct MaterialSpecification {
        std::string grade;         // Марка материала
        std::string standard;      // Стандарт материала
        double tensileStrength;    // МПа
        double yieldStrength;      // МПа
        double density;            // г/см³
        std::vector<std::string> applicableTo;  // Применимо к
    };

    std::optional<MaterialSpecification> getMaterialSpec(
        const std::string& materialGrade,
        Standard standard
    ) const;

    // Проверка размеров по стандартам
    struct SizeTable {
        std::vector<double> od;        // Наружные диаметры
        std::vector<double> wall;      // Толщины стенок
        Standard standard;
        std::string region;            // "EU" (EN), "Russia" (GOST), "USA" (ANSI)
    };

    std::optional<SizeTable> getStandardSizes(Standard standard) const;

    // Рекомендации по выбору стандарта
    struct StandardRecommendation {
        Standard standard;
        std::string reason;
        double score;  // 0.0-1.0 (насколько подходит)
    };

    std::vector<StandardRecommendation> recommendStandards(
        double od, double wall
    ) const;

    // Проверка всех применимых стандартов
    std::vector<ComplianceReport> checkAllStandards(
        const TubeModel& model,
        double od, double wall
    ) const;

private:
    const StandardsDatabase& database;

    // Вспомогательные функции
    bool isDiameterInRange(double od, Standard standard) const;
    bool isWallThicknessValid(double od, double wall, Standard standard) const;
    std::string getToleranceGrade(double od, Standard standard) const;
};

// Генератор отчётов соответствия
class ComplianceReportGenerator {
public:
    explicit ComplianceReportGenerator();
    ~ComplianceReportGenerator();

    // Генерация HTML отчёта
    std::string generateHTMLReport(const ComplianceReport& report) const;

    // Генерация PDF отчёта (требует внешней библиотеки)
    bool generatePDFReport(
        const ComplianceReport& report,
        const std::string& outputFile
    ) const;

    // Генерация JSON отчёта
    std::string generateJSONReport(const ComplianceReport& report) const;

    // Генерация CSV для импорта в таблицы
    std::string generateCSVReport(const ComplianceReport& report) const;

    // Сравнение двух отчётов
    struct ReportComparison {
        ComplianceReport before;
        ComplianceReport after;
        int newCompliantRules = 0;
        int newNonCompliantRules = 0;
        std::string summary;
    };

    ReportComparison compareReports(
        const ComplianceReport& before,
        const ComplianceReport& after
    ) const;

private:
    std::string htmlEscape(const std::string& text) const;
};

// Менеджер соответствия стандартам
class StandardsComplianceManager {
public:
    explicit StandardsComplianceManager();
    ~StandardsComplianceManager();

    // Компоненты
    StandardsDatabase database;
    StandardsChecker checker;
    ComplianceReportGenerator reportGenerator;

    // Сохранение отчёта
    bool saveReport(
        const ComplianceReport& report,
        const std::string& filePath,
        const std::string& format = "json"  // "json", "html", "csv"
    ) const;

    // Загрузка отчёта
    std::optional<ComplianceReport> loadReport(const std::string& filePath) const;

    // Архив отчётов
    std::vector<ComplianceReport> getReportHistory(
        const std::string& objectId,
        int limit = 50
    ) const;

    // Сертификация объекта
    struct Certificate {
        std::string certificateNumber;
        std::string objectId;
        std::string objectName;
        Standard standard;
        ComplianceStatus status = ComplianceStatus::Compliant;
        int64_t issueDate = 0;
        int64_t expiryDate = 0;
        std::string issuedBy;
        std::string signature;
    };

    std::optional<Certificate> issueCertificate(
        const ComplianceReport& report,
        const std::string& issuedBy
    );

    bool verifyCertificate(const Certificate& cert) const;

private:
    std::vector<ComplianceReport> reportHistory;
    std::vector<Certificate> certificates;
};

}  // namespace bendcore::standards
