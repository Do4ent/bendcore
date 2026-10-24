#pragma once

#include <array>
#include <string>
#include <cmath>
#include <vector>

namespace bendcore {

// Класс для расчёта параллельного смещения трубопровода
// Реализует алгоритм расчёта координат и параметров гибок при параллельном смещении
class ParallelOffsetCalculator {
public:
  // Входные параметры расчёта
  struct Parameters {
    double outer_diameter;      // Внешний диаметр трубы (мм)
    double wall_thickness;      // Толщина стенки (мм)
    double bend_angle;          // Угол гибки (градусы, диапазон 5-180)
    double offset_distance;     // Расстояние параллельного смещения (мм)
    double bend_radius;         // Радиус гибки (мм)
    std::string material;       // Материал ("Steel", "Aluminum", "Copper", etc.)
  };

  // Результаты расчёта
  struct Result {
    double total_length;              // Общая длина трубы (мм)
    double bend_1_length;             // Длина первой гибки (мм)
    double straight_section;          // Длина прямого участка между гибками (мм)
    double bend_2_length;             // Длина второй гибки (мм)
    std::array<double, 2> bend_angles; // Углы для каждой гибки (градусы)
    std::array<double, 3> bend_1_pos;  // 3D координаты центра первой гибки (x, y, z)
    std::array<double, 3> bend_2_pos;  // 3D координаты центра второй гибки (x, y, z)
    double confidence;                // Уверенность расчёта в процентах (0-100%)
    std::string warnings;             // Предупреждения если есть
    bool is_valid;                    // Флаг валидности конфигурации
  };

  // Структура для валидации
  struct ValidationError {
    std::string code;      // Код ошибки
    std::string message;   // Описание ошибки
    std::string parameter; // Параметр с ошибкой
  };

  ParallelOffsetCalculator() = default;
  ~ParallelOffsetCalculator() = default;

  // Основной метод расчёта параллельного смещения
  // Возвращает результат содержащий все параметры гибок и координаты
  Result calculate(const Parameters& params);

  // Валидация входных параметров
  // Проверяет корректность всех входных значений
  bool validate(const Parameters& params);

  // Получить сообщение об ошибке последней операции
  std::string get_error_message() const;

  // Получить вектор всех ошибок валидации
  std::vector<ValidationError> get_validation_errors() const;

  // Статический метод для получения коэффициента коррекции материала
  // Разные материалы имеют разные свойства при гибке
  static double get_material_factor(const std::string& material);

  // Проверка возможности выполнить гибку с заданными параметрами
  // Учитывает минимальный радиус гибки для диаметра трубы
  static bool can_bend(double od, double wall_thickness, double bend_radius,
                       const std::string& material);

private:
  mutable std::string error_message_;
  mutable std::vector<ValidationError> validation_errors_;

  // Вспомогательные методы расчёта

  // Расчёт длины участка гибки по углу
  double calculate_bend_arc_length(double bend_radius, double bend_angle_rad) const;

  // Расчёт координат точки гибки в 3D пространстве
  std::array<double, 3> calculate_bend_position(
    double distance,
    double angle_rad,
    double offset_distance,
    int bend_number) const;

  // Расчёт коэффициента нейтрального слоя на основе OD и толщины стенки
  double calculate_k_factor(double od, double wall_thickness) const;

  // Расчёт реальной длины трубы с учётом припусков на гибку
  double calculate_developed_length(
    double bend_1_length,
    double straight_section,
    double bend_2_length,
    double k_factor) const;

  // Валидация отдельных параметров
  bool validate_diameter(double od);
  bool validate_wall_thickness(double od, double wall);
  bool validate_bend_angle(double angle);
  bool validate_offset_distance(double offset, double od);
  bool validate_bend_radius(double radius, double od, double wall,
                           const std::string& material);

  // Добавить ошибку валидации
  void add_validation_error(const std::string& code,
                            const std::string& message,
                            const std::string& parameter);
};

}  // namespace bendcore
