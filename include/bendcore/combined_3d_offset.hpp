#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <optional>

namespace bendcore {

// ============================================================================
// Базовые 3D структуры данных
// ============================================================================

struct Vector3D {
  double x, y, z;

  Vector3D() : x(0), y(0), z(0) {}
  Vector3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

  Vector3D operator+(const Vector3D& v) const {
    return Vector3D(x + v.x, y + v.y, z + v.z);
  }

  Vector3D operator-(const Vector3D& v) const {
    return Vector3D(x - v.x, y - v.y, z - v.z);
  }

  Vector3D operator*(double scalar) const {
    return Vector3D(x * scalar, y * scalar, z * scalar);
  }

  double dot(const Vector3D& v) const {
    return x * v.x + y * v.y + z * v.z;
  }

  Vector3D cross(const Vector3D& v) const {
    return Vector3D(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
  }

  double magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
  }

  Vector3D normalized() const {
    double mag = magnitude();
    if (mag < 1e-10) return Vector3D(0, 0, 0);
    return Vector3D(x / mag, y / mag, z / mag);
  }
};

using Point3D = Vector3D;

// ============================================================================
// Главный класс калькулятора 3D смещения
// ============================================================================

class Combined3DOffsetCalculator {
public:
  // Параметры входа
  struct Parameters {
    Point3D start_point;           // Начальная 3D точка (X, Y, Z) мм
    Point3D end_point;             // Конечная 3D точка (X, Y, Z) мм
    double outer_diameter;         // Наружный диаметр мм
    double wall_thickness;         // Толщина стенки мм
    double bend_radius;            // Радиус гибки мм
    int num_bends;                 // Количество гибок (2-5)
    std::string distribution;      // "equal" или "custom"
    double inclination_angle;      // Угол наклона в градусах (-90 до 90)
    bool springback_correction;    // Коррекция пружинения
    std::string material;          // Материал: "steel", "aluminum", "copper"

    // Для custom распределения: позиции гибок (0.0 до 1.0 от start к end)
    std::vector<double> custom_bend_positions;
  };

  // Деталь одной гибки
  struct BendDetail {
    Point3D position;              // 3D координата гибки
    double angle_xy;               // Угол в XY плоскости (0-360 градусов)
    double angle_z;                // Угол наклона/возвышения (-90 до 90 градусов)
    double length;                 // Длина участка до этой гибки
    double radius;                 // Радиус этой гибки (с учётом коррекции)
    double springback_correction;  // Значение коррекции пружинения для этой гибки
    int bend_index;                // Индекс гибки (1, 2, 3, ...)
  };

  // Деталь прямого участка между гибками
  struct SectionDetail {
    Point3D start;                 // Начало секции
    Point3D end;                   // Конец секции
    double length;                 // Длина прямого участка
    double slope_percent;          // Процент наклона
    int section_index;             // Индекс секции
  };

  // Результаты расчёта
  struct Result {
    bool success;                  // Успешность расчёта
    std::string error_message;     // Сообщение об ошибке (если есть)

    double total_length;           // Общая длина трубопровода мм
    double total_distance_3d;      // 3D расстояние между точками

    std::vector<BendDetail> bends; // Все гибки
    std::vector<SectionDetail> sections;  // Все прямые участки
    std::vector<Point3D> trajectory_3d;   // Точки траектории для визуализации

    std::string cnc_code;          // G-code для ЧПУ
    double feasibility_score;      // % возможности производства (0-100)
    std::vector<std::string> warnings;   // Предупреждения при необходимости
  };

  // Конструктор
  Combined3DOffsetCalculator();

  // Основной метод расчёта
  Result calculate(const Parameters& params);

  // Валидация входных параметров
  bool validate(const Parameters& params, std::vector<std::string>& errors);

  // Генерация G-code для ЧПУ
  std::string generate_cnc_code(const Result& result);

protected:
  // Вспомогательные методы доступные подклассам

  // Проверка возможности производства
  double check_feasibility(const Parameters& params,
                          const std::vector<BendDetail>& bends) const;

private:
  // Внутренние вспомогательные методы

  // Расчёт расстояния между двумя 3D точками
  double distance_3d(const Point3D& p1, const Point3D& p2) const;

  // Определение углов в разных плоскостях
  void calculate_angles(const Vector3D& direction,
                       double& angle_xy, double& angle_z) const;

  // Распределение гибок вдоль траектории
  std::vector<Point3D> distribute_bends(const Point3D& start,
                                         const Point3D& end,
                                         int num_bends,
                                         const std::string& distribution,
                                         const std::vector<double>& custom_positions);

  // Расчёт коррекции пружинения для материала
  double get_springback_correction(const std::string& material,
                                   double radius) const;

  // Генерация точек траектории для 3D визуализации
  std::vector<Point3D> generate_trajectory_points(
    const std::vector<BendDetail>& bends,
    const Point3D& start_point,
    const Point3D& end_point) const;

  // Расчёт координат гибок с учётом всех параметров
  void calculate_bends_geometry(const Point3D& start,
                               const Point3D& end,
                               const std::vector<Point3D>& bend_positions,
                               double outer_diameter,
                               double wall_thickness,
                               bool springback_correction,
                               const std::string& material,
                               std::vector<BendDetail>& result_bends);

  // Расчёт прямых участков между гибками
  void calculate_sections(const Point3D& start,
                         const Point3D& end,
                         const std::vector<BendDetail>& bends,
                         std::vector<SectionDetail>& result_sections);

  // Вспомогательные константы для материалов
  static constexpr double SPRINGBACK_STEEL = 0.02;      // 2% для стали
  static constexpr double SPRINGBACK_ALUMINUM = 0.03;   // 3% для алюминия
  static constexpr double SPRINGBACK_COPPER = 0.025;    // 2.5% для меди

  static constexpr double MIN_BEND_RADIUS_FACTOR = 0.5; // Min radius = OD * 0.5
  static constexpr double MAX_INCLINATION_ANGLE = 85.0; // Max наклон
  static constexpr double MIN_DISTANCE_3D = 10.0;       // Min расстояние мм
  static constexpr double MAX_DISTANCE_3D = 10000.0;    // Max расстояние мм
};

}  // namespace bendcore
