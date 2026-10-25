/**
 * GAP-006: Основные 3D-операции (Feature Operations)
 * Реализация базовых операций формообразования на B-Rep модели
 *
 * Операции:
 * - Extrude: выдавливание эскиза в направлении
 * - Revolve: вращение профиля вокруг оси
 * - Sweep: движение профиля вдоль пути
 * - Loft: интерполяция между профилями
 * - Hole: создание отверстия
 * - Fillet: скругление рёбер
 * - Chamfer: фаска на рёбрах
 * - Shell: создание тонкостенной оболочки
 * - Boolean: объединение, вычитание, пересечение
 * - Mirror: зеркальное отражение
 * - Pattern: массив элементов
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/geometry/brep_builder.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"

namespace bendcore::geometry {

/**
 * Параметры для операций формообразования
 */

struct ExtrudeParams {
    double distance = 0.0;
    bool symmetric = false;
    double taper_angle = 0.0;
    std::string direction_face_id;  // Опциональная грань для определения направления
};

struct RevolveParams {
    double angle = 360.0;  // градусы
    Point3D axis_origin;
    Vector3D axis_direction;
};

struct SweepParams {
    std::vector<Point3D> path_curve;
    double scale_start = 1.0;
    double scale_end = 1.0;
    bool keep_contact = false;
};

struct LoftParams {
    std::vector<std::vector<Point3D>> profiles;
    bool smooth = true;
    bool closed = false;
};

struct HoleParams {
    Point3D center;
    double diameter = 0.0;
    double depth = 0.0;
    bool blind = false;
};

struct FilletParams {
    double radius = 0.0;
    std::vector<EdgeId> edges;
};

struct ChamferParams {
    double size = 0.0;
    std::vector<EdgeId> edges;
    bool use_distance = true;
};

struct ShellParams {
    double thickness = 1.0;
    std::vector<FaceId> removed_faces;  // Грани для удаления (открытия оболочки)
};

struct PatternParams {
    enum class PatternType { Linear, Circular, Rectangular };
    PatternType type = PatternType::Linear;
    int count = 2;
    double distance = 10.0;
    Vector3D direction = Vector3D(1, 0, 0);
    int rows = 1;
    int columns = 1;
};

/**
 * FeatureOperations - класс для выполнения операций формообразования
 */
class FeatureOperations {
public:
    explicit FeatureOperations(BRepBuilder& builder);
    ~FeatureOperations() = default;

    // ========== Базовые операции ==========

    /**
     * Extrude: создание объёма из профиля путём выдавливания
     */
    [[nodiscard]] std::optional<SolidId> extrude(
        const std::vector<Point3D>& profile,
        const ExtrudeParams& params
    );

    /**
     * Revolve: создание тела вращением профиля вокруг оси
     */
    [[nodiscard]] std::optional<SolidId> revolve(
        const std::vector<Point3D>& profile,
        const RevolveParams& params
    );

    /**
     * Sweep: движение профиля вдоль кривой пути
     */
    [[nodiscard]] std::optional<SolidId> sweep(
        const std::vector<Point3D>& profile,
        const SweepParams& params
    );

    /**
     * Loft: интерполяция между несколькими профилями
     */
    [[nodiscard]] std::optional<SolidId> loft(
        const LoftParams& params
    );

    // ========== Операции модификации ==========

    /**
     * Hole: создание отверстия в теле
     */
    [[nodiscard]] bool create_hole(
        SolidId solid,
        const HoleParams& params
    );

    /**
     * Fillet: скругление рёбер
     */
    [[nodiscard]] std::optional<SolidId> fillet(
        SolidId solid,
        const FilletParams& params
    );

    /**
     * Chamfer: создание фаски на рёбрах
     */
    [[nodiscard]] std::optional<SolidId> chamfer(
        SolidId solid,
        const ChamferParams& params
    );

    /**
     * Shell: преобразование в тонкостенную оболочку
     */
    [[nodiscard]] std::optional<SolidId> shell(
        SolidId solid,
        const ShellParams& params
    );

    // ========== Boolean операции ==========

    /**
     * Union: объединение двух тел
     */
    [[nodiscard]] std::optional<SolidId> union_solids(
        SolidId body1,
        SolidId body2
    );

    /**
     * Difference: вычитание одного тела из другого
     */
    [[nodiscard]] std::optional<SolidId> difference_solids(
        SolidId body,
        SolidId tool
    );

    /**
     * Intersection: пересечение двух тел
     */
    [[nodiscard]] std::optional<SolidId> intersection_solids(
        SolidId body1,
        SolidId body2
    );

    // ========== Трансформации ==========

    /**
     * Mirror: зеркальное отражение относительно плоскости
     */
    [[nodiscard]] std::optional<SolidId> mirror(
        SolidId solid,
        const Point3D& plane_origin,
        const Vector3D& plane_normal
    );

    /**
     * Pattern: создание массива элементов
     */
    [[nodiscard]] std::vector<SolidId> pattern(
        SolidId solid,
        const PatternParams& params
    );

    // ========== Диагностика и валидация ==========

    /**
     * Валидировать результат операции
     */
    [[nodiscard]] bool validate_result(SolidId solid) const;

    /**
     * Проверить возможность выполнения операции
     */
    [[nodiscard]] bool can_perform_operation(SolidId solid) const;

    /**
     * Получить ошибку последней операции
     */
    [[nodiscard]] std::string get_last_error() const { return last_error_; }

private:
    BRepBuilder& builder_;
    std::string last_error_;

    // Вспомогательные методы
    [[nodiscard]] SolidId create_extrusion_solid(
        const std::vector<Point3D>& profile,
        const Vector3D& direction,
        double distance
    );

    [[nodiscard]] std::vector<Point3D> rotate_profile(
        const std::vector<Point3D>& profile,
        double angle,
        const Point3D& axis_origin,
        const Vector3D& axis_dir
    );

    [[nodiscard]] std::vector<Point3D> interpolate_path(
        const std::vector<Point3D>& control_points,
        size_t segments
    );

    [[nodiscard]] FaceId create_fillet_face(
        FaceId original_face,
        EdgeId edge,
        double radius
    );

    [[nodiscard]] bool merge_shells(
        SolidId result_solid,
        const std::vector<ShellId>& shells
    );

    void set_error(const std::string& msg) { last_error_ = msg; }
};

}  // namespace bendcore::geometry
