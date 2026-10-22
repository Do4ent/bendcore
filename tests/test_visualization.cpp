// Тесты для системы визуализации (Phase 25 Stage 2)
#include "bendcore/visualization.hpp"
#include "bendcore/tube_model.hpp"

#include <cmath>
#include <cstdio>

using namespace bendcore;
using namespace bendcore::visualization;

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
    TestCase tc{"visualization"};

    // Тест 1: Создание визуализатора
    {
        Visualizer viz;
        assert(tc, true, "Visualizer создан успешно");
    }

    // Тест 2: Создание сетки
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 50.0},
            {SegmentKind::Bend, 50.0, 90.0, 0.0},
            {SegmentKind::Straight, 50.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        params.deflection = 2.0;
        params.maxVertices = 10000;
        params.enableNormals = true;
        params.enableColors = true;

        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);

        assert(tc, mesh.vertexCount > 0, "Сетка содержит вершины");
        assert(tc, mesh.triangleCount > 0, "Сетка содержит треугольники");
        assert(tc, mesh.positions.size() == static_cast<size_t>(mesh.vertexCount * 3),
               "Позиции согласованы с числом вершин");
    }

    // Тест 3: Оптимизация сетки
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        params.deflection = 0.5;

        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);
        int originalVertices = mesh.vertexCount;

        RenderMesh optimized = viz.optimizeMesh(mesh);

        assert(tc, optimized.vertexCount <= originalVertices,
               "Оптимизация уменьшает или сохраняет число вершин");
        assert(tc, optimized.triangleCount > 0, "Оптимизированная сетка содержит треугольники");
    }

    // Тест 4: Вычисление камеры для стандартного вида
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);

        CameraParameters camera = viz.getCameraForView(StandardView::Isometric, mesh);

        assert(tc, camera.position[0] != 0 || camera.position[1] != 0 || camera.position[2] != 0,
               "Позиция камеры установлена");
        assert(tc, camera.fov > 0.0 && camera.fov < 180.0, "FOV камеры в допустимом диапазоне");
    }

    // Тест 5: Вычисление ограничивающей сферы
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 50.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{50, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);

        auto sphere = viz.computeBoundingSphere(mesh);

        assert(tc, sphere.radius > 0.0, "Радиус ограничивающей сферы положительный");
        assert(tc, sphere.radius < 1000.0, "Радиус ограничивающей сферы разумный");
    }

    // Тест 6: Выбор уровня детализации (LOD)
    {
        Visualizer viz;

        LODLevel veryHigh = viz.selectLODLevel(60000);
        LODLevel high = viz.selectLODLevel(30000);
        LODLevel medium = viz.selectLODLevel(15000);
        LODLevel low = viz.selectLODLevel(7000);
        LODLevel veryLow = viz.selectLODLevel(3000);

        assert(tc, veryHigh == LODLevel::VeryHigh, "VeryHigh LOD выбран для большого числа вершин");
        assert(tc, high == LODLevel::High, "High LOD выбран правильно");
        assert(tc, medium == LODLevel::Medium, "Medium LOD выбран правильно");
        assert(tc, low == LODLevel::Low, "Low LOD выбран правильно");
        assert(tc, veryLow == LODLevel::VeryLow, "VeryLow LOD выбран для малого числа вершин");
    }

    // Тест 7: Раскрашивание по ошибкам
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);

        std::vector<uint32_t> errorIndices = {0, 1, 2};
        RenderMesh colored = viz.colorByIssues(mesh, errorIndices, 0xFF0000FF);

        assert(tc, colored.colors.size() > 0, "Раскрашенная сетка содержит цвета");
        assert(tc, colored.vertexCount == mesh.vertexCount,
               "Число вершин не изменилось при раскрашивании");
    }

    // Тест 8: Расчёт нормалей
    {
        Visualizer viz;

        std::vector<float> positions = {
            0.0f, 0.0f, 0.0f,  // v0
            1.0f, 0.0f, 0.0f,  // v1
            0.0f, 1.0f, 0.0f   // v2
        };

        std::vector<uint32_t> indices = {0, 1, 2};

        std::vector<float> normals = viz.computeNormals(positions, indices);

        assert(tc, normals.size() == positions.size(), "Нормали имеют правильный размер");
        assert(tc, normals.size() > 0, "Нормали вычислены");
    }

    // Тест 9: Прогрессивная сетка
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        auto progression = viz.createProgressiveMesh(model, 10.0, 1.0);

        assert(tc, progression.levels.size() > 0, "Прогрессивная сетка содержит уровни");

        // Проверяем, что каждый уровень менее детализирован чем предыдущий
        if (progression.levels.size() > 1) {
            for (size_t i = 1; i < progression.levels.size(); ++i) {
                assert(tc, progression.levels[i].vertexCount <= progression.levels[i-1].vertexCount,
                       "Каждый уровень имеет меньше вершин чем предыдущий");
            }
        }
    }

    // Тест 10: Расчёт статистики производительности
    {
        Visualizer viz;

        TubeModel model;
        model.path = std::vector<PathSegment>{
            {SegmentKind::Straight, 100.0}
        };

        Port port1{Vec3{0, 0, 0}, Vec3{0, 0, -1}, 10.0, "A"};
        Port port2{Vec3{100, 0, 0}, Vec3{0, 0, 1}, 10.0, "B"};
        model.ports = {port1, port2};

        MeshParameters params;
        params.maxVertices = 20000;
        RenderMesh mesh = viz.createMesh(model, 10.0, 1.0, params);

        auto stats = viz.computePerformanceStats(mesh);

        assert(tc, stats.vertexCount == mesh.vertexCount, "Статистика вершин согласована");
        assert(tc, stats.meshMemoryMB > 0.0, "Память сетки вычислена");
        assert(tc, stats.estimatedFrameTimeMs >= 0.0, "Время кадра положительное");
    }

    // Результаты
    printf("\n=== Test Visualization ===\n");
    printf("Passed: %d\n", tc.passed);
    printf("Failed: %d\n", tc.failed);

    return tc.failed == 0 ? 0 : 1;
}
