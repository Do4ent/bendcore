#include "bendcore/visualization.hpp"
#include "bendcore/tube_body.hpp"
#include "bendcore/blank.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace bendcore::visualization {

Visualizer::Visualizer() = default;
Visualizer::~Visualizer() = default;

RenderMesh Visualizer::createMesh(
    const TubeModel& model,
    double od, double wall,
    const MeshParameters& params
) const {
    RenderMesh mesh;

    if (!model.path.has_value()) {
        return mesh;
    }

    // Создаём 3D тело трубы (требует OCCT)
    // Здесь упрощённо: используем геометрию пути
    const auto& path = *model.path;

    // Примерный расчёт вершин на основе параметров
    mesh.lodLevel = selectLODLevel(params.maxVertices);

    // Генерируем вершины цилиндра вдоль пути
    const int circleVertices = std::max(8, static_cast<int>(360.0 / std::asin(params.deflection / (od / 2.0))));
    int estimatedVertices = circleVertices * (path.size() + 10);

    if (estimatedVertices > params.maxVertices) {
        // Уменьшаем детализацию
        // Это упрощённая логика
    }

    // Заполняем позиции вершин
    for (int circle = 0; circle < circleVertices; ++circle) {
        double angle = 2.0 * 3.141592653589793 * circle / circleVertices;
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);

        for (const auto& seg : path) {
            // Упрощённо: только для прямых участков
            if (seg.kind == SegmentKind::Straight) {
                double radius = od / 2.0;
                float x = static_cast<float>(radius * cosA);
                float y = static_cast<float>(radius * sinA);
                float z = static_cast<float>(seg.length);

                mesh.positions.push_back(x);
                mesh.positions.push_back(y);
                mesh.positions.push_back(z);
            }
        }
    }

    // Генерируем индексы для треугольников
    for (int i = 0; i < static_cast<int>(mesh.positions.size() / 3) - circleVertices; ++i) {
        int v0 = i;
        int v1 = i + 1;
        int v2 = i + circleVertices;
        int v3 = i + circleVertices + 1;

        // Первый треугольник
        mesh.indices.push_back(v0);
        mesh.indices.push_back(v2);
        mesh.indices.push_back(v1);

        // Второй треугольник
        mesh.indices.push_back(v1);
        mesh.indices.push_back(v2);
        mesh.indices.push_back(v3);
    }

    mesh.vertexCount = mesh.positions.size() / 3;
    mesh.triangleCount = mesh.indices.size() / 3;

    // Вычисляем нормали если нужны
    if (params.enableNormals) {
        mesh.normals = computeNormals(mesh.positions, mesh.indices);
    }

    // Устанавливаем цвета по умолчанию (серый)
    if (params.enableColors) {
        for (int i = 0; i < mesh.vertexCount; ++i) {
            mesh.colors.push_back(0.7f);  // R
            mesh.colors.push_back(0.7f);  // G
            mesh.colors.push_back(0.7f);  // B
            mesh.colors.push_back(1.0f);  // A
        }
    }

    // Вычисляем ограничивающую сферу
    auto boundingSphere = computeBoundingSphere(mesh);
    mesh.boundingSphereCentre = boundingSphere.centre;
    mesh.boundingSphereRadius = boundingSphere.radius;

    return mesh;
}

RenderMesh Visualizer::optimizeMesh(const RenderMesh& mesh, int targetVertices) const {
    RenderMesh optimized = mesh;

    // Удаляем дубликаты вершин
    std::unordered_map<std::string, uint32_t> vertexMap;
    std::vector<float> uniquePositions;
    std::vector<float> uniqueNormals;
    std::vector<float> uniqueColors;
    std::vector<uint32_t> newIndices;

    auto vertexKey = [](float x, float y, float z) {
        // Упрощённый ключ для вершин
        int ix = static_cast<int>(x * 1000) / 1000;
        int iy = static_cast<int>(y * 1000) / 1000;
        int iz = static_cast<int>(z * 1000) / 1000;
        return std::to_string(ix) + "," + std::to_string(iy) + "," + std::to_string(iz);
    };

    for (size_t i = 0; i < mesh.positions.size(); i += 3) {
        float x = mesh.positions[i];
        float y = mesh.positions[i + 1];
        float z = mesh.positions[i + 2];

        std::string key = vertexKey(x, y, z);
        uint32_t vertexIndex;

        if (vertexMap.find(key) != vertexMap.end()) {
            vertexIndex = vertexMap[key];
        } else {
            vertexIndex = uniquePositions.size() / 3;
            vertexMap[key] = vertexIndex;

            uniquePositions.push_back(x);
            uniquePositions.push_back(y);
            uniquePositions.push_back(z);

            if (i + 2 < mesh.normals.size()) {
                uniqueNormals.push_back(mesh.normals[i]);
                uniqueNormals.push_back(mesh.normals[i + 1]);
                uniqueNormals.push_back(mesh.normals[i + 2]);
            }

            if (i + 3 < mesh.colors.size()) {
                uniqueColors.push_back(mesh.colors[i]);
                uniqueColors.push_back(mesh.colors[i + 1]);
                uniqueColors.push_back(mesh.colors[i + 2]);
                uniqueColors.push_back(mesh.colors[i + 3]);
            }
        }

        newIndices.push_back(vertexIndex);
    }

    optimized.positions = uniquePositions;
    optimized.normals = uniqueNormals;
    optimized.colors = uniqueColors;
    optimized.indices = newIndices;
    optimized.vertexCount = uniquePositions.size() / 3;
    optimized.triangleCount = newIndices.size() / 3;

    return optimized;
}

CameraParameters Visualizer::getCameraForView(
    StandardView view,
    const RenderMesh& mesh,
    double zoomFactor
) const {
    CameraParameters camera;

    double radius = mesh.boundingSphereRadius * zoomFactor;
    double distance = radius / std::sin(camera.fov * 3.141592653589793 / 360.0);

    const Vec3& centre = mesh.boundingSphereCentre;

    switch (view) {
        case StandardView::Isometric:
            camera.position = {centre[0] + distance, centre[1] + distance, centre[2] + distance};
            break;
        case StandardView::Front:
            camera.position = {centre[0], centre[1], centre[2] + distance};
            camera.up = {0, 1, 0};
            break;
        case StandardView::Top:
            camera.position = {centre[0], centre[1] + distance, centre[2]};
            camera.up = {0, 0, -1};
            break;
        case StandardView::Right:
            camera.position = {centre[0] + distance, centre[1], centre[2]};
            camera.up = {0, 1, 0};
            break;
        case StandardView::Bottom:
            camera.position = {centre[0], centre[1] - distance, centre[2]};
            camera.up = {0, 0, 1};
            break;
        case StandardView::Back:
            camera.position = {centre[0], centre[1], centre[2] - distance};
            camera.up = {0, 1, 0};
            break;
        case StandardView::Left:
            camera.position = {centre[0] - distance, centre[1], centre[2]};
            camera.up = {0, 1, 0};
            break;
    }

    camera.target = centre;
    return camera;
}

CameraParameters Visualizer::fitCameraToMesh(
    const RenderMesh& mesh,
    double padding
) const {
    return getCameraForView(StandardView::Isometric, mesh, padding);
}

RenderMesh Visualizer::colorByIssues(
    const RenderMesh& mesh,
    const std::vector<uint32_t>& errorIndices,
    uint32_t errorColor
) const {
    RenderMesh colored = mesh;

    // По умолчанию зелёный цвет
    for (int i = 0; i < colored.vertexCount; ++i) {
        colored.colors.push_back(0.0f);  // R
        colored.colors.push_back(1.0f);  // G
        colored.colors.push_back(0.0f);  // B
        colored.colors.push_back(1.0f);  // A
    }

    // Раскрашиваем вершины с ошибками в красный
    for (uint32_t idx : errorIndices) {
        if (idx < static_cast<uint32_t>(colored.vertexCount)) {
            size_t colorIdx = idx * 4;
            colored.colors[colorIdx] = 1.0f;      // R
            colored.colors[colorIdx + 1] = 0.0f;  // G
            colored.colors[colorIdx + 2] = 0.0f;  // B
            colored.colors[colorIdx + 3] = 1.0f;  // A
        }
    }

    return colored;
}

RenderMesh Visualizer::colorByCurvature(
    const RenderMesh& mesh,
    double minCurvature,
    double maxCurvature
) const {
    RenderMesh colored = mesh;

    // Упрощённый расчёт кривизны по нормалям
    for (int i = 0; i < colored.vertexCount; ++i) {
        double curvature = 0.5;  // Средняя кривизна

        if (i > 0 && i < colored.vertexCount - 1) {
            // Расчёт из нормалей
            if (mesh.normals.size() > static_cast<size_t>(i * 3 + 2)) {
                // Упрощённо берём величину изменения нормали
                // В реальности здесь должна быть вычисленная кривизна поверхности
            }
        }

        uint32_t color = curvatureToColor(curvature, minCurvature, maxCurvature);
        colored.colors.push_back((color >> 24) & 0xFF);  // R
        colored.colors.push_back((color >> 16) & 0xFF);  // G
        colored.colors.push_back((color >> 8) & 0xFF);   // B
        colored.colors.push_back(color & 0xFF);          // A
    }

    return colored;
}

RenderMesh Visualizer::colorByStress(
    const RenderMesh& mesh,
    const std::vector<double>& stressValues,
    double minStress,
    double maxStress
) const {
    RenderMesh colored = mesh;

    for (size_t i = 0; i < static_cast<size_t>(colored.vertexCount); ++i) {
        double stress = (i < stressValues.size()) ? stressValues[i] : 0.0;
        uint32_t color = stressToColor(stress, minStress, maxStress);

        colored.colors.push_back((color >> 24) & 0xFF);  // R
        colored.colors.push_back((color >> 16) & 0xFF);  // G
        colored.colors.push_back((color >> 8) & 0xFF);   // B
        colored.colors.push_back(color & 0xFF);          // A
    }

    return colored;
}

std::vector<float> Visualizer::computeNormals(
    const std::vector<float>& positions,
    const std::vector<uint32_t>& indices
) const {
    std::vector<float> normals(positions.size(), 0.0f);

    // Вычисляем нормали граней
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        // Получаем вершины треугольника
        Vec3 v0{positions[i0 * 3], positions[i0 * 3 + 1], positions[i0 * 3 + 2]};
        Vec3 v1{positions[i1 * 3], positions[i1 * 3 + 1], positions[i1 * 3 + 2]};
        Vec3 v2{positions[i2 * 3], positions[i2 * 3 + 1], positions[i2 * 3 + 2]};

        // Вычисляем векторы рёбер
        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;

        // Нормаль = cross(edge1, edge2)
        Vec3 normal = edge1.cross(edge2);
        double len = normal.length();
        if (len > 1e-6) {
            normal = normal / len;
        }

        // Добавляем нормаль к вершинам
        for (uint32_t idx : {i0, i1, i2}) {
            normals[idx * 3] += normal[0];
            normals[idx * 3 + 1] += normal[1];
            normals[idx * 3 + 2] += normal[2];
        }
    }

    // Нормализуем
    for (size_t i = 0; i < normals.size(); i += 3) {
        Vec3 n{normals[i], normals[i + 1], normals[i + 2]};
        double len = n.length();
        if (len > 1e-6) {
            n = n / len;
            normals[i] = n[0];
            normals[i + 1] = n[1];
            normals[i + 2] = n[2];
        }
    }

    return normals;
}

std::vector<float> Visualizer::smoothNormals(
    const std::vector<float>& normals,
    double smoothingFactor
) const {
    // Упрощённое сглаживание нормалей
    std::vector<float> smoothed = normals;

    for (size_t i = 0; i < normals.size(); i += 3) {
        // Усредняем с соседними нормалями
        // В реальной реализации нужна информация о связности вершин
    }

    return smoothed;
}

Visualizer::BoundingSphere Visualizer::computeBoundingSphere(const RenderMesh& mesh) const {
    BoundingSphere result;
    result.centre = {0, 0, 0};
    result.radius = 0.0;

    if (mesh.positions.empty()) {
        return result;
    }

    // Вычисляем центр как среднее всех вершин
    Vec3 centre{0, 0, 0};
    for (size_t i = 0; i < mesh.positions.size(); i += 3) {
        centre[0] += mesh.positions[i];
        centre[1] += mesh.positions[i + 1];
        centre[2] += mesh.positions[i + 2];
    }
    centre = centre / (mesh.positions.size() / 3);

    // Вычисляем максимальное расстояние от центра
    double maxDist = 0.0;
    for (size_t i = 0; i < mesh.positions.size(); i += 3) {
        Vec3 v{mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]};
        double dist = (v - centre).length();
        maxDist = std::max(maxDist, dist);
    }

    result.centre = centre;
    result.radius = maxDist;
    return result;
}

LODLevel Visualizer::selectLODLevel(int vertexCount) const {
    if (vertexCount > 50000) return LODLevel::VeryHigh;
    if (vertexCount > 25000) return LODLevel::High;
    if (vertexCount > 10000) return LODLevel::Medium;
    if (vertexCount > 5000) return LODLevel::Low;
    return LODLevel::VeryLow;
}

Visualizer::MeshProgression Visualizer::createProgressiveMesh(
    const TubeModel& model,
    double od, double wall
) const {
    MeshProgression progression;

    // Создаём сетки разных уровней детализации
    std::vector<double> deflections = {5.0, 3.0, 1.5, 0.75, 0.25};

    for (double def : deflections) {
        MeshParameters params;
        params.deflection = def;
        params.maxVertices = 50000;

        RenderMesh mesh = createMesh(model, od, wall, params);
        progression.levels.push_back(mesh);
    }

    return progression;
}

RenderMesh Visualizer::interpolateLOD(
    const RenderMesh& lowLOD,
    const RenderMesh& highLOD,
    double alpha
) const {
    // Упрощённая интерполяция (в реальности нужна морфинг вершин)
    if (alpha < 0.5) {
        return lowLOD;
    } else {
        return highLOD;
    }
}

Visualizer::MeshExport Visualizer::exportMeshForWeb(
    const RenderMesh& mesh,
    const std::string& format
) const {
    MeshExport result;
    result.format = format;

    if (format == "binary") {
        // Простой бинарный формат
        // Структура: 4 байта - кол-во вершин, затем позиции, нормали, цвета, индексы

        uint32_t vertexCount = mesh.vertexCount;
        result.data.resize(sizeof(uint32_t) +
                          mesh.positions.size() * sizeof(float) +
                          mesh.normals.size() * sizeof(float) +
                          mesh.colors.size() * sizeof(float) +
                          mesh.indices.size() * sizeof(uint32_t));

        size_t offset = 0;

        // Пишем количество вершин
        std::memcpy(result.data.data() + offset, &vertexCount, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Пишем данные
        std::memcpy(result.data.data() + offset, mesh.positions.data(),
                   mesh.positions.size() * sizeof(float));
        offset += mesh.positions.size() * sizeof(float);

        // Дополняем нулями для выравнивания...
        result.compressedSize = result.data.size();
    }

    return result;
}

Visualizer::PerformanceStats Visualizer::computePerformanceStats(const RenderMesh& mesh) const {
    PerformanceStats stats;

    stats.vertexCount = mesh.vertexCount;
    stats.triangleCount = mesh.triangleCount;

    // Примерный расчёт памяти
    double positionsMem = mesh.positions.size() * sizeof(float) / (1024.0 * 1024.0);
    double normalsMem = mesh.normals.size() * sizeof(float) / (1024.0 * 1024.0);
    double colorsMem = mesh.colors.size() * sizeof(float) / (1024.0 * 1024.0);
    double indicesMem = mesh.indices.size() * sizeof(uint32_t) / (1024.0 * 1024.0);

    stats.meshMemoryMB = positionsMem + normalsMem + colorsMem + indicesMem;

    // Примерное время кадра (на 60 FPS)
    // Примерно 16.67 мс на кадр
    stats.estimatedFrameTimeMs = (mesh.triangleCount / 1000000.0) * 10.0;  // 10 мс на миллион треугольников

    stats.optimizedForMobile = mesh.vertexCount < 10000;

    return stats;
}

uint32_t Visualizer::stressToColor(double stress, double minStress, double maxStress) const {
    // Цветовая шкала: синий (низкий) -> зелёный -> жёлтый -> красный (высокий)
    double normalized = (stress - minStress) / (maxStress - minStress);
    normalized = std::max(0.0, std::min(1.0, normalized));

    uint8_t r, g, b;

    if (normalized < 0.5) {
        // Синий -> зелёный
        double t = normalized * 2.0;
        r = static_cast<uint8_t>(0);
        g = static_cast<uint8_t>(255 * t);
        b = static_cast<uint8_t>(255 * (1.0 - t));
    } else {
        // Зелёный -> красный
        double t = (normalized - 0.5) * 2.0;
        r = static_cast<uint8_t>(255 * t);
        g = static_cast<uint8_t>(255 * (1.0 - t));
        b = 0;
    }

    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

uint32_t Visualizer::curvatureToColor(double curvature, double minCurv, double maxCurv) const {
    // Аналогично stressToColor
    return stressToColor(curvature, minCurv, maxCurv);
}

}  // namespace bendcore::visualization
