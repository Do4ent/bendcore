// Анализ трубы: напряжения, допуски, коллизии, проверка стандартов
// Phase 25 Stage 1: Basic Analysis System
#pragma once

#include "bendcore/tube_model.hpp"
#include "bendcore/vec3.hpp"

#include <string>
#include <vector>
#include <optional>

namespace bendcore::analysis {

// Типы нарушений/ошибок
enum class IssueLevel {
    Info,       // Информация
    Warning,    // Предупреждение
    Error       // Ошибка (блокирует экспорт/выпуск)
};

enum class IssueType {
    // Геометрические ошибки
    SelfIntersection,           // Самопересечение трубы
    InvalidBendRadius,          // Недопустимый радиус гиба
    ExcessiveBending,           // Перегиб (угол > 180°)

    // Технологические ошибки
    InsufficientStraight,       // Недостаточный прямой участок между гибами
    MinimumLengthViolation,     // Нарушение минимальной длины первого/последнего участка
    InsufficientToolClearance,  // Недостаточный зазор между инструментом и окружением

    // Ошибки портов
    InvalidPortPosition,        // Неверная позиция порта
    PortOutsideTube,           // Порт вне трубы
    PortClearanceViolation,    // Нарушение зазора в зоне порта

    // Ошибки материала/станка
    MaterialMismatch,          // Несоответствие материала профилю
    BendRadiusOutOfRange,      // Радиус гиба вне диапазона станка
    NoSuitableMachine,         // Нет подходящего станка

    // Предупреждения
    SuggestedToolChange,       // Рекомендуется сменить инструмент
    WearCompensation,          // Требуется компенсация износа
    SpringbackVariation,       // Вариация пружинения из-за материала

    // Информация
    OptimalBendSequence,       // Оптимальная последовательность гибов
    AlternativeMachine,        // Доступны альтернативные станки
};

struct Issue {
    IssueLevel level;
    IssueType type;
    std::string message;
    std::optional<int> bendIndex;      // Индекс гиба, где возникла ошибка
    std::optional<int> portIndex;      // Индекс порта, где возникла ошибка
    std::optional<double> actualValue; // Фактическое значение
    std::optional<double> minValue;    // Минимальное допустимое значение
    std::optional<double> maxValue;    // Максимальное допустимое значение
};

// Результат анализа трубы
struct AnalysisResult {
    bool valid = true;                    // Труба пригодна для производства
    std::vector<Issue> issues;            // Список всех проблем

    // Параметры трубы
    double totalLength = 0.0;             // Полная длина трубы
    double blankLength = 0.0;             // Длина заготовки с припусками
    int bendCount = 0;                    // Количество гибов
    double totalBendAngle = 0.0;          // Суммарный угол поворота
    double maxBendRadius = 0.0;           // Максимальный радиус гиба
    double minBendRadius = 0.0;           // Минимальный радиус гиба
    double maxCurvature = 0.0;            // Максимальная кривизна пути

    // Оценки
    double complexityScore = 0.0;         // Оценка сложности (0-100)
    double manufacturingRisk = 0.0;       // Риск производства (0-100)
    std::string suggestedMachine;         // Рекомендуемый станок
};

// Material density definition for mass calculation
struct MaterialDensity {
    std::string name;
    double density;  // g/cm³
};

// Standard material densities
static const std::vector<MaterialDensity> MATERIAL_DENSITIES = {
    {"Steel", 7.85},
    {"Aluminum", 2.70},
    {"Copper", 8.96},
    {"Titanium", 4.54},
    {"Custom", 5.0}
};

// Параметры материала для анализа
struct MaterialSpec {
    std::string name;
    double youngModulus = 0.0;            // Модуль Юнга, МПа
    double yieldStrength = 0.0;           // Предел текучести, МПа
    double tensileStrength = 0.0;         // Предел прочности, МПа
    double ultimateBendRadius = 0.0;      // Предел гибки (макс. радиус для ∅), мм
    bool allowSpringback = false;         // Допускается пружинение
    double springbackFactor = 0.0;        // Коэффициент пружинения (0-1)
};

// Параметры машины для анализа
struct MachineSpec {
    std::string name;
    std::string type;                     // "manual", "powered", "cnc"
    double minTubeOD = 0.0;              // Минимальный наружный диаметр, мм
    double maxTubeOD = 0.0;              // Максимальный наружный диаметр, мм
    double minBendRadius = 0.0;          // Минимальный радиус гиба, мм
    double maxBendRadius = 0.0;          // Максимальный радиус гиба, мм
    double bendAngleResolution = 0.0;    // Разрешение по углу, градусы
    double maxTubeLength = 0.0;          // Максимальная длина трубы, мм
    int maxBendCount = 0;                // Максимальное количество гибов
};

class Analyzer {
public:
    explicit Analyzer();
    ~Analyzer();

    // Основной анализ трубы
    AnalysisResult analyze(
        const TubeModel& model,
        double od, double wall,
        const MaterialSpec& material,
        const std::vector<MachineSpec>& machines
    ) const;

    // Проверка отдельных параметров
    std::vector<Issue> checkGeometry(const TubeModel& model, double od) const;
    std::vector<Issue> checkBendParameters(
        const TubeModel& model,
        const MaterialSpec& material,
        const MachineSpec& machine
    ) const;
    std::vector<Issue> checkPorts(const TubeModel& model, double od, double wall) const;
    std::vector<Issue> checkMaterialCompatibility(
        const TubeModel& model,
        const MaterialSpec& material
    ) const;
    std::vector<Issue> checkMachineCapability(
        const TubeModel& model,
        const MachineSpec& machine
    ) const;

    // Рекомендации и оптимизация
    std::string suggestOptimalMachine(
        const TubeModel& model,
        const std::vector<MachineSpec>& machines
    ) const;

    double calculateComplexity(
        const TubeModel& model,
        const MaterialSpec& material,
        const MachineSpec& machine
    ) const;

    double calculateManufacturingRisk(
        const TubeModel& model,
        const MaterialSpec& material,
        const MachineSpec& machine
    ) const;

    // Gap checking for clearance analysis
    std::vector<Issue> checkGaps(
        const TubeModel& model,
        double minimumGap = 5.0
    ) const;

    // Mass calculation for tube structures
    // Returns mass in kilograms
    double calculateTubeMass(
        const TubeModel& model,
        double outerDiameter,      // mm
        double wallThickness,      // mm
        const std::string& materialName  // "Steel", "Aluminum", etc.
    ) const;

private:
    // Вспомогательные функции для анализа
    Issue createIssue(
        IssueLevel level,
        IssueType type,
        const std::string& message,
        std::optional<int> bendIdx = {},
        std::optional<double> actual = {},
        std::optional<double> minVal = {},
        std::optional<double> maxVal = {}
    ) const;
};

}  // namespace bendcore::analysis
