// Тесты для системы анализа труб (Phase 25 Stage 1)
#include "bendcore/analysis.hpp"
#include "bendcore/tube_model.hpp"

#include <cmath>
#include <cstdio>

using namespace bendcore;
using namespace bendcore::analysis;

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
    TestCase tc{"analysis"};

    // Тест 1: Создание анализатора
    {
        Analyzer analyzer;
        assert(tc, true, "Analyzer создан успешно");
    }

    // Тест 2: Анализ простого пути
    {
        Analyzer analyzer;

        // Простой путь: прямая + один гиб + прямая
        TubeModel model;
        PathSegment seg1{SegmentKind::Straight, 50.0};
        PathSegment seg2{SegmentKind::Bend, 50.0, 90.0, 0.0};  // радиус 50, угол 90°
        PathSegment seg3{SegmentKind::Straight, 50.0};
        model.path = std::vector<PathSegment>{seg1, seg2, seg3};

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MaterialSpec mat;
        mat.name = "медь";
        mat.youngModulus = 120000;
        mat.yieldStrength = 200;
        mat.ultimateBendRadius = 10.0;

        MachineSpec mach;
        mach.name = "Станок 1";
        mach.type = "powered";
        mach.minTubeOD = 5.0;
        mach.maxTubeOD = 100.0;
        mach.minBendRadius = 10.0;
        mach.maxBendRadius = 500.0;
        mach.maxTubeLength = 500.0;
        mach.maxBendCount = 10;

        AnalysisResult result = analyzer.analyze(model, 10.0, 1.0, mat, {mach});

        assert(tc, result.valid, "Анализ простого пути прошёл успешно");
        assert(tc, result.bendCount == 1, "Обнаружен 1 гиб");
        assert(tc, !result.suggestedMachine.empty(), "Выбран станок");
    }

    // Тест 3: Проверка геометрических ошибок
    {
        Analyzer analyzer;

        // Путь с недопустимым радиусом (меньше радиуса трубы)
        TubeModel model;
        PathSegment seg1{SegmentKind::Straight, 50.0};
        PathSegment seg2{SegmentKind::Bend, 3.0, 90.0, 0.0};  // радиус 3.0 < ∅/2 = 5.0
        PathSegment seg3{SegmentKind::Straight, 50.0};
        model.path = std::vector<PathSegment>{seg1, seg2, seg3};

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MaterialSpec mat;
        mat.name = "медь";

        MachineSpec mach;
        mach.name = "Станок 1";
        mach.type = "powered";
        mach.minBendRadius = 1.0;
        mach.maxBendRadius = 500.0;

        AnalysisResult result = analyzer.analyze(model, 10.0, 1.0, mat, {mach});

        // Должна быть ошибка о недопустимом радиусе
        bool hasRadiusError = false;
        for (const auto& issue : result.issues) {
            if (issue.type == IssueType::InvalidBendRadius) {
                hasRadiusError = true;
                break;
            }
        }
        assert(tc, hasRadiusError, "Обнаружена ошибка неправильного радиуса");
    }

    // Тест 4: Проверка ограничений станка
    {
        Analyzer analyzer;

        TubeModel model;
        PathSegment seg1{SegmentKind::Straight, 50.0};
        PathSegment seg2{SegmentKind::Bend, 50.0, 90.0, 0.0};
        PathSegment seg3{SegmentKind::Straight, 50.0};
        model.path = std::vector<PathSegment>{seg1, seg2, seg3};

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MaterialSpec mat;
        mat.name = "медь";

        // Станок с очень ограниченными параметрами
        MachineSpec mach;
        mach.name = "Маленький станок";
        mach.type = "manual";
        mach.minTubeOD = 20.0;  // Минимум 20 мм, а у нас 10 мм
        mach.maxTubeOD = 30.0;

        AnalysisResult result = analyzer.analyze(model, 10.0, 1.0, mat, {mach});

        // Труба должна быть непригодна для этого станка
        bool hasError = false;
        for (const auto& issue : result.issues) {
            if (issue.level == IssueLevel::Error) {
                hasError = true;
                break;
            }
        }
        assert(tc, hasError, "Обнаружена ошибка ограничения станка");
    }

    // Тест 5: Расчёт сложности
    {
        Analyzer analyzer;

        // Простой путь
        TubeModel simple;
        simple.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 50.0},
            {SegmentKind::Bend, 50.0, 90.0, 0.0},
            {SegmentKind::Straight, 50.0}
        };

        MaterialSpec mat;
        mat.name = "медь";

        MachineSpec mach;
        mach.name = "Станок 1";
        mach.minBendRadius = 10.0;
        mach.maxBendRadius = 500.0;

        double complexity = analyzer.calculateComplexity(simple, mat, mach);
        assert(tc, complexity >= 0.0 && complexity <= 100.0, "Сложность в диапазоне [0, 100]");
        assert(tc, complexity < 50.0, "Простой путь имеет низкую сложность");
    }

    // Тест 6: Проверка портов
    {
        Analyzer analyzer;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        // Порт с диаметром больше трубы
        Port portBad{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 20.0, "Bad"};
        model.ports = {portBad};

        MaterialSpec mat;
        mat.name = "медь";

        MachineSpec mach;
        mach.name = "Станок 1";
        mach.minBendRadius = 1.0;
        mach.maxBendRadius = 500.0;

        AnalysisResult result = analyzer.analyze(model, 10.0, 1.0, mat, {mach});

        bool hasPortError = false;
        for (const auto& issue : result.issues) {
            if (issue.type == IssueType::InvalidPortPosition) {
                hasPortError = true;
                break;
            }
        }
        assert(tc, hasPortError, "Обнаружена ошибка порта");
    }

    // Тест 7: Сравнение нескольких станков
    {
        Analyzer analyzer;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 50.0},
            {SegmentKind::Bend, 50.0, 45.0, 0.0},
            {SegmentKind::Straight, 50.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MaterialSpec mat;
        mat.name = "медь";

        // Несколько станков
        std::vector<MachineSpec> machines{
            {"Станок A", "manual", 5.0, 50.0, 10.0, 200.0, {}, 300.0, 5},
            {"Станок B", "powered", 8.0, 100.0, 15.0, 300.0, {}, 500.0, 10},
            {"Станок C", "cnc", 1.0, 200.0, 5.0, 400.0, {}, 1000.0, 20}
        };

        AnalysisResult result = analyzer.analyze(model, 10.0, 1.0, mat, machines);

        assert(tc, result.valid, "Анализ с несколькими станками прошёл успешно");
        assert(tc, !result.suggestedMachine.empty(), "Выбран оптимальный станок");
        assert(tc, result.complexityScore > 0.0, "Вычислена сложность");
        assert(tc, result.manufacturingRisk >= 0.0, "Вычислен риск производства");
    }

    // Результаты
    printf("\n=== Test Analysis ===\n");
    printf("Passed: %d\n", tc.passed);
    printf("Failed: %d\n", tc.failed);

    return tc.failed == 0 ? 0 : 1;
}
