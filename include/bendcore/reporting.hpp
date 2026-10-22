// Система генерации отчётов и документации
// Phase 25 Stage 5: Compliance Reporting & Documentation
#pragma once

#include "bendcore/tube_model.hpp"
#include "bendcore/analysis.hpp"
#include "bendcore/standards.hpp"
#include "bendcore/visualization.hpp"

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::reporting {

// Карта заготовки (bend card)
struct BendCard {
    std::string cardNumber;              // Номер карты гибки
    std::string tubeId;                  // ID трубы
    std::string tubeName;                // Имя трубы
    double outerDiameter = 0.0;          // Наружный диаметр, мм
    double wallThickness = 0.0;          // Толщина стенки, мм
    double blankLength = 0.0;            // Длина заготовки, мм
    std::vector<double> bendRadii;       // Радиусы гибов
    std::vector<double> bendAngles;      // Углы гибов (градусы)
    std::vector<double> straightLengths; // Прямые участки между гибами
    std::string material;                // Материал
    std::string machine;                 // Станок/инструмент
    std::string operatorNotes;           // Примечания оператора
    int64_t createdTimestamp = 0;        // Время создания
    std::string createdBy;               // Кто создал
    std::string approvedBy;              // Кто утвердил
    int64_t approvedTimestamp = 0;       // Время утверждения
    bool isApproved = false;             // Утверждена ли карта
    std::vector<std::string> warnings;   // Предупреждения
    std::vector<std::string> errors;     // Ошибки
};

// Развёртка трубы (unfolding specification)
struct UnfoldingSpec {
    std::string unfoldId;
    std::string tubeId;
    double totalLength = 0.0;            // Полная развёрнутая длина
    std::vector<double> segmentLengths;  // Длины участков
    std::vector<double> bendArcs;        // Дуги гибов
    std::string dimensionNote;           // Примечание для размеров
    bool includeMarks = true;            // Включать ли метки гибов
    double markOffset = 2.0;             // Смещение меток, мм
};

// Спецификация (BOM - Bill of Materials)
struct SpecificationEntry {
    int position = 0;                    // Позиция в спецификации
    std::string designation;             // Обозначение (код трубы)
    std::string description;             // Описание
    double quantity = 1.0;               // Количество
    std::string unit;                    // Единица (шт, м, т)
    std::string standard;                // Применимый стандарт
    std::string notes;                   // Примечания
};

// CAM экспорт для станка
struct CAMExport {
    std::string format;                  // Формат (G-code, proprietary)
    std::string machineType;             // Тип станка
    std::vector<std::string> gCodeLines; // G-code команды
    std::map<std::string, std::string> machineParameters;  // Параметры станка
    std::vector<std::string> toolDefinitions;  // Определения инструментов
    int64_t exportTimestamp = 0;
};

// Полный производственный отчёт
struct ProductionReport {
    std::string reportId;
    std::string projectName;
    int64_t generatedTimestamp = 0;
    std::string generatedBy;

    // Компоненты отчёта
    std::vector<BendCard> bendCards;
    std::vector<UnfoldingSpec> unfoldings;
    std::vector<SpecificationEntry> specification;
    std::vector<analysis::AnalysisResult> analysisResults;
    std::vector<standards::ComplianceReport> complianceReports;

    // Документация
    struct DocumentationSection {
        std::string title;
        std::string content;
        std::vector<std::string> attachments;
    };

    std::vector<DocumentationSection> sections;

    // Метаданные
    std::string approvalStatus;  // "draft", "pending", "approved", "released"
    std::vector<std::string> approvers;
    std::vector<int64_t> approvalTimestamps;
};

// Генератор отчётов
class ReportGenerator {
public:
    explicit ReportGenerator();
    ~ReportGenerator();

    // Создание карты гибки
    BendCard generateBendCard(
        const TubeModel& model,
        double od, double wall,
        const std::string& material,
        const std::string& machine,
        const std::string& generatedBy
    ) const;

    // Создание развёртки
    UnfoldingSpec generateUnfolding(
        const TubeModel& model,
        double od
    ) const;

    // Создание спецификации
    std::vector<SpecificationEntry> generateSpecification(
        const std::vector<TubeModel>& tubes,
        const std::string& projectName
    ) const;

    // CAM экспорт для G-code
    CAMExport exportToGCode(
        const BendCard& bendCard,
        const std::string& machineProfile
    ) const;

    // CAM экспорт для proprietary форматов (заглушка)
    CAMExport exportToProprietyFormat(
        const BendCard& bendCard,
        const std::string& machineFormat
    ) const;

    // Генерация полного производственного отчёта
    ProductionReport generateProductionReport(
        const std::string& projectName,
        const std::vector<TubeModel>& tubes,
        double od, double wall,
        const std::string& generatedBy
    ) const;

    // Экспорт в различные форматы
    enum class ExportFormat {
        PDF,      // PDF документ
        HTML,     // HTML с встроенными стилями
        DOCX,     // Microsoft Word
        DWG,      // AutoCAD drawing
        SVG,      // Масштабируемая векторная графика
        JSON      // JSON для обработки
    };

    std::string exportReport(
        const ProductionReport& report,
        ExportFormat format
    ) const;

    // Экспорт BOM в CSV для закупок
    std::string exportBOMtoCSV(
        const std::vector<SpecificationEntry>& bom
    ) const;

    // Экспорт карты гибки в SVG (для просмотра)
    std::string exportBendCardToSVG(
        const BendCard& card
    ) const;

private:
    // Вспомогательные функции
    std::string generateCardNumber() const;
    std::string generateGCodeLine(
        int commandIndex,
        const std::string& command,
        const std::map<std::string, double>& parameters
    ) const;
};

// Менеджер документации и аудита
class DocumentationManager {
public:
    explicit DocumentationManager();
    ~DocumentationManager();

    // Сохранение отчёта с историей версий
    bool saveProductionReport(
        const ProductionReport& report,
        const std::string& filePath
    );

    // Загрузка отчёта
    std::optional<ProductionReport> loadProductionReport(
        const std::string& filePath
    ) const;

    // Архив отчётов по проекту
    std::vector<ProductionReport> getProjectReports(
        const std::string& projectName,
        int limit = 50
    ) const;

    // Аудит-лог всех изменений
    struct AuditLog {
        int64_t timestamp = 0;
        std::string action;              // "created", "modified", "approved", "released"
        std::string userId;
        std::string description;
        std::string oldValue;
        std::string newValue;
    };

    void logAuditEntry(const AuditLog& entry);
    std::vector<AuditLog> getAuditTrail(
        const std::string& reportId,
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Экспорт для архивирования
    struct ExportPackage {
        std::vector<ProductionReport> reports;
        std::vector<AuditLog> auditTrail;
        std::map<std::string, std::string> metadata;
        int64_t createdTimestamp = 0;
    };

    bool createExportPackage(
        const std::string& projectName,
        const std::string& outputPath
    );

    // Импорт из архива
    bool importExportPackage(
        const std::string& packagePath
    );

    // Статистика проекта
    struct ProjectStatistics {
        std::string projectName;
        int totalTubes = 0;
        int approvedCards = 0;
        int pendingCards = 0;
        double totalBlankLength = 0.0;
        std::map<std::string, int> tubesByMaterial;
        std::map<std::string, int> tubesByMachine;
        int64_t projectStartDate = 0;
        int64_t lastModifiedDate = 0;
    };

    ProjectStatistics getProjectStatistics(
        const std::string& projectName
    ) const;

private:
    std::vector<ProductionReport> reportArchive;
    std::vector<AuditLog> auditTrail;
};

// Интегрированный менеджер отчётности
class ReportingManager {
public:
    explicit ReportingManager();
    ~ReportingManager();

    // Компоненты
    ReportGenerator reportGenerator;
    DocumentationManager documentationManager;

    // Полный workflow от анализа к производству
    struct CompleteWorkflow {
        analysis::AnalysisResult analysis;
        standards::ComplianceReport compliance;
        BendCard bendCard;
        UnfoldingSpec unfolding;
        CAMExport camExport;
        ProductionReport report;
    };

    CompleteWorkflow executeCompleteWorkflow(
        const TubeModel& model,
        double od, double wall,
        const std::string& material,
        const std::string& machine,
        const std::string& userId
    );

    // Проверка готовности к производству
    struct ProductionReadiness {
        bool isReady = false;
        std::vector<std::string> blockers;      // Ошибки, блокирующие производство
        std::vector<std::string> warnings;      // Предупреждения (можно игнорировать)
        std::string readinessScore;             // "Ready", "With Warnings", "Not Ready"
    };

    ProductionReadiness checkProductionReadiness(
        const analysis::AnalysisResult& analysis,
        const standards::ComplianceReport& compliance,
        const BendCard& bendCard
    ) const;

    // Выпуск (release) для производства
    bool releaseForProduction(
        const ProductionReport& report,
        const std::string& approvedBy
    );

    // Статус производства
    struct ProductionStatus {
        std::string reportId;
        std::string status;              // "draft", "pending_approval", "approved", "in_production", "completed"
        int percentComplete = 0;         // 0-100%
        std::vector<std::string> notes;
        int64_t estimatedCompletionTime = 0;
    };

    std::optional<ProductionStatus> getProductionStatus(
        const std::string& reportId
    ) const;

    // Качество и контроль
    struct QualityCheckpoint {
        std::string checkpointId;
        std::string checkType;           // "visual", "dimensional", "functional"
        bool passed = false;
        std::string notes;
        int64_t checkTimestamp = 0;
        std::string checkedBy;
    };

    void recordQualityCheckpoint(
        const std::string& reportId,
        const QualityCheckpoint& checkpoint
    );

    std::vector<QualityCheckpoint> getQualityCheckpoints(
        const std::string& reportId
    ) const;
};

}  // namespace bendcore::reporting
