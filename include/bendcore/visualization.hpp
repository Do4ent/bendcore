// Система визуализации трубы: 3D сетка, оптимизация, рендеринг
// Phase 25 Stage 2: Advanced Visualization
#pragma once

#include "bendcore/tube_model.hpp"
#include "bendcore/vec3.hpp"

#include <vector>
#include <optional>
#include <cstdint>

namespace bendcore::visualization {

// Типы режимов отображения
enum class RenderMode {
    Solid,           // Сплошной (Phong lighting)
    Wireframe,       // Каркас (только рёбра)
    SolidWireframe,  // Сплошной + каркас
    Normals,         // Нормали
    HiddenLine       // Скрытые линии (каркас, невидимые светлее)
};

// Уровни детализации (LOD)
enum class LODLevel {
    VeryHigh,   // > 50000 вершин
    High,       // 25000-50000
    Medium,     // 10000-25000
    Low,        // 5000-10000
    VeryLow     // < 5000
};

// Параметры сетки
struct MeshParameters {
    double deflection = 1.0;           // Отклонение сетки, мм (0.5-10)
    int maxVertices = 50000;           // Макс. вершин в сетке
    bool enableNormals = true;         // Вычислять нормали
    bool enableColors = true;          // Раскрашивать (например, по ошибкам)
    bool optimizeMemory = true;        // Оптимизировать память (удалить дубликаты)
    double normalSmoothing = 0.5;      // Сглаживание нормалей (0-1)
    bool generateTangents = false;     // Генерировать тангенты (для шейдеров)
};

// Геометрия для рендеринга
struct RenderMesh {
    std::vector<float> positions;      // x, y, z для каждой вершины (x3)
    std::vector<float> normals;        // нормали к вершинам (x3)
    std::vector<float> colors;         // RGBA для каждой вершины (x4)
    std::vector<uint32_t> indices;     // индексы для треугольников
    std::vector<float> tangents;       // тангенты (если нужны)

    // Метаданные
    double boundingSphereRadius = 0.0; // Радиус ограничивающей сферы
    Vec3 boundingSphereCentre{0, 0, 0};// Центр сферы
    int vertexCount = 0;
    int triangleCount = 0;
    LODLevel lodLevel = LODLevel::High;
};

// Параметры камеры
struct CameraParameters {
    Vec3 position{0, 100, 100};        // Позиция камеры
    Vec3 target{0, 0, 0};              // Центр просмотра
    Vec3 up{0, 1, 0};                  // Направление вверх
    double fov = 45.0;                 // Поле зрения, градусы
    double near = 0.1;                 // Ближняя плоскость отсечения
    double far = 10000.0;              // Дальняя плоскость отсечения
    bool orthographic = false;         // Ортогональная проекция
};

// Стандартные виды
enum class StandardView {
    Isometric,      // Изометрический
    Front,          // Вид спереди (Z)
    Top,            // Вид сверху (Y)
    Right,          // Вид справа (X)
    Bottom,         // Вид снизу (-Y)
    Back,           // Вид сзади (-Z)
    Left            // Вид слева (-X)
};

// Визуализатор трубы
class Visualizer {
public:
    explicit Visualizer();
    ~Visualizer();

    // Создание сетки для рендеринга
    RenderMesh createMesh(
        const TubeModel& model,
        double od, double wall,
        const MeshParameters& params = MeshParameters()
    ) const;

    // Оптимизация сетки (удаление дубликатов, сжатие памяти)
    RenderMesh optimizeMesh(const RenderMesh& mesh, int targetVertices = 0) const;

    // Вычисление параметров камеры для стандартного вида
    CameraParameters getCameraForView(
        StandardView view,
        const RenderMesh& mesh,
        double zoomFactor = 1.0
    ) const;

    // Подгонка камеры под геометрию
    CameraParameters fitCameraToMesh(
        const RenderMesh& mesh,
        double padding = 1.1
    ) const;

    // Раскрашивание сетки по ошибкам анализа
    RenderMesh colorByIssues(
        const RenderMesh& mesh,
        const std::vector<uint32_t>& errorIndices,
        uint32_t errorColor = 0xFF0000FF   // RGBA: красный по умолчанию
    ) const;

    // Раскрашивание по кривизне
    RenderMesh colorByCurvature(
        const RenderMesh& mesh,
        double minCurvature = 0.0,
        double maxCurvature = 1.0
    ) const;

    // Раскрашивание по напряжениям (для анализа)
    RenderMesh colorByStress(
        const RenderMesh& mesh,
        const std::vector<double>& stressValues,
        double minStress = 0.0,
        double maxStress = 100.0
    ) const;

    // Вычисление нормалей для вершин
    std::vector<float> computeNormals(
        const std::vector<float>& positions,
        const std::vector<uint32_t>& indices
    ) const;

    // Сглаживание нормалей (для лучшего внешнего вида)
    std::vector<float> smoothNormals(
        const std::vector<float>& normals,
        double smoothingFactor = 0.5
    ) const;

    // Вычисление ограничивающей сферы
    struct BoundingSphere {
        Vec3 centre;
        double radius;
    };

    BoundingSphere computeBoundingSphere(const RenderMesh& mesh) const;

    // Выбор оптимального уровня детализации (LOD)
    LODLevel selectLODLevel(int vertexCount) const;

    // Прогрессивная загрузка сетки (для больших моделей)
    struct MeshProgression {
        std::vector<RenderMesh> levels;  // от самой детальной (levels[0]) к самой грубой
        int currentLevel = 0;
    };

    MeshProgression createProgressiveMesh(
        const TubeModel& model,
        double od, double wall
    ) const;

    // Анимация переходов между LOD
    RenderMesh interpolateLOD(
        const RenderMesh& lowLOD,
        const RenderMesh& highLOD,
        double alpha // 0.0 = low LOD, 1.0 = high LOD
    ) const;

    // Экспорт сетки в формат для передачи на UI (JSON/Binary)
    struct MeshExport {
        std::string format;  // "json" или "binary"
        std::vector<uint8_t> data;
        size_t compressedSize = 0;
    };

    MeshExport exportMeshForWeb(
        const RenderMesh& mesh,
        const std::string& format = "binary"
    ) const;

    // Информация о производительности рендеринга
    struct PerformanceStats {
        int vertexCount = 0;
        int triangleCount = 0;
        double meshMemoryMB = 0.0;
        double estimatedFrameTimeMs = 0.0;
        bool optimizedForMobile = false;
    };

    PerformanceStats computePerformanceStats(const RenderMesh& mesh) const;

private:
    // Вспомогательные функции
    void addFaceNormals(
        std::vector<float>& normals,
        const std::vector<float>& positions,
        const std::vector<uint32_t>& indices
    ) const;

    uint32_t stressToColor(double stress, double minStress, double maxStress) const;
    uint32_t curvatureToColor(double curvature, double minCurv, double maxCurv) const;
};

}  // namespace bendcore::visualization
