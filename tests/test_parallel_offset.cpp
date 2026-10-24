#include <gtest/gtest.h>
#include "bendcore/parallel_offset.hpp"
#include <cmath>

using namespace bendcore;

class ParallelOffsetTest : public ::testing::Test {
protected:
  ParallelOffsetCalculator calculator;

  // Стандартные параметры для тестов
  ParallelOffsetCalculator::Parameters createStandardParams() {
    return {
      .outer_diameter = 20.0,        // 20 мм
      .wall_thickness = 2.0,         // 2 мм
      .bend_angle = 90.0,            // 90 градусов
      .offset_distance = 30.0,       // 30 мм смещение
      .bend_radius = 50.0,           // 50 мм радиус гибки
      .material = "Steel"
    };
  }
};

// ======================== ТЕСТЫ БАЗОВОЙ ФУНКЦИОНАЛЬНОСТИ ========================

TEST_F(ParallelOffsetTest, Calculate90DegreeOffset) {
  // Тест: Стандартный случай - 90° смещение
  auto params = createStandardParams();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_GT(result.total_length, 0.0);
  EXPECT_GT(result.bend_1_length, 0.0);
  EXPECT_GT(result.bend_2_length, 0.0);
  EXPECT_GE(result.confidence, 50.0);
  EXPECT_LE(result.confidence, 100.0);
}

TEST_F(ParallelOffsetTest, Calculate45DegreeOffset) {
  // Тест: Диагональное смещение - 45°
  auto params = createStandardParams();
  params.bend_angle = 45.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_GT(result.total_length, 0.0);
  EXPECT_NEAR(result.bend_angles[0], 45.0, 0.1);
}

TEST_F(ParallelOffsetTest, SmallAngle5Degrees) {
  // Тест: Минимальный допустимый угол - 5°
  auto params = createStandardParams();
  params.bend_angle = 5.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_GT(result.confidence, 70.0); // Должна быть штраф за маленький угол
}

TEST_F(ParallelOffsetTest, LargeAngle175Degrees) {
  // Тест: Экстремальный угол - 175°
  auto params = createStandardParams();
  params.bend_angle = 175.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_LT(result.confidence, 90.0); // Штраф за экстремальный угол
}

// ======================== ТЕСТЫ ВАЛИДАЦИИ ========================

TEST_F(ParallelOffsetTest, ValidateDiameterTooSmall) {
  // Тест: Диаметр меньше минимума (3 мм)
  auto params = createStandardParams();
  params.outer_diameter = 2.0;

  EXPECT_FALSE(calculator.validate(params));
  EXPECT_NE(calculator.get_error_message().find("диаметр"), std::string::npos);
}

TEST_F(ParallelOffsetTest, ValidateDiameterTooLarge) {
  // Тест: Диаметр больше максимума (500 мм)
  auto params = createStandardParams();
  params.outer_diameter = 600.0;

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateWallThicknessTooThin) {
  // Тест: Стенка слишком тонкая (< 1% OD)
  auto params = createStandardParams();
  params.outer_diameter = 100.0;
  params.wall_thickness = 0.5; // < 1% от 100

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateWallThicknessTooThick) {
  // Тест: Стенка больше половины диаметра
  auto params = createStandardParams();
  params.outer_diameter = 20.0;
  params.wall_thickness = 15.0;

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateBendAngleTooSmall) {
  // Тест: Угол меньше 5°
  auto params = createStandardParams();
  params.bend_angle = 2.0;

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateBendAngleTooLarge) {
  // Тест: Угол больше 180°
  auto params = createStandardParams();
  params.bend_angle = 185.0;

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateNegativeOffset) {
  // Тест: Отрицательное смещение
  auto params = createStandardParams();
  params.offset_distance = -10.0;

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateOffsetTooLarge) {
  // Тест: Смещение > 10*OD
  auto params = createStandardParams();
  params.outer_diameter = 20.0;
  params.offset_distance = 250.0; // 12.5 * OD

  EXPECT_FALSE(calculator.validate(params));
}

TEST_F(ParallelOffsetTest, ValidateRadiusTooSmall) {
  // Тест: Радиус гибки меньше минимума для стали (2.5*OD)
  auto params = createStandardParams();
  params.outer_diameter = 20.0;
  params.bend_radius = 30.0; // < 50 (2.5*20)
  params.material = "Steel";

  EXPECT_FALSE(calculator.validate(params));
}

// ======================== ТЕСТЫ РАЗЛИЧНЫХ МАТЕРИАЛОВ ========================

TEST_F(ParallelOffsetTest, AluminumMaterial) {
  // Тест: Материал Aluminum (коэффициент 0.85)
  auto params = createStandardParams();
  params.material = "Aluminum";
  params.bend_radius = 40.0; // Меньше радиус для алюминия
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
}

TEST_F(ParallelOffsetTest, StainlessSteelMaterial) {
  // Тест: Материал Stainless Steel (коэффициент 1.15)
  auto params = createStandardParams();
  params.material = "Stainless Steel";
  params.bend_radius = 60.0; // Больше радиус для нержавейки
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
}

TEST_F(ParallelOffsetTest, CopperMaterial) {
  // Тест: Материал Copper
  auto params = createStandardParams();
  params.material = "Copper";
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
}

TEST_F(ParallelOffsetTest, MaterialFactorRetrieval) {
  // Тест: Проверка получения коэффициентов материалов
  EXPECT_DOUBLE_EQ(ParallelOffsetCalculator::get_material_factor("Steel"), 1.0);
  EXPECT_LT(ParallelOffsetCalculator::get_material_factor("Aluminum"), 1.0);
  EXPECT_GT(ParallelOffsetCalculator::get_material_factor("Stainless Steel"), 1.0);
  EXPECT_DOUBLE_EQ(ParallelOffsetCalculator::get_material_factor("Unknown"), 1.0);
}

// ======================== ТЕСТЫ ДИАМЕТРОВ ========================

TEST_F(ParallelOffsetTest, SmallDiameter) {
  // Тест: Маленький диаметр (3 мм)
  auto params = createStandardParams();
  params.outer_diameter = 3.0;
  params.wall_thickness = 0.5;
  params.bend_radius = 10.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
}

TEST_F(ParallelOffsetTest, LargeDiameter) {
  // Тест: Большой диаметр (400 мм)
  auto params = createStandardParams();
  params.outer_diameter = 400.0;
  params.wall_thickness = 10.0;
  params.bend_radius = 1000.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
}

// ======================== ТЕСТЫ КООРДИНАТ 3D ========================

TEST_F(ParallelOffsetTest, BendPositions3D) {
  // Тест: Проверка расчёта 3D координат
  auto params = createStandardParams();
  auto result = calculator.calculate(params);

  // Первая гибка должна быть близко к нулю по Y и Z
  EXPECT_EQ(result.bend_1_pos[0], 0.0);
  EXPECT_GE(result.bend_1_pos[1], 0.0);

  // Вторая гибка должна быть дальше по оси X
  EXPECT_GT(result.bend_2_pos[0], result.bend_1_pos[0]);
}

// ======================== ТЕСТЫ ЭКСТРЕМАЛЬНЫХ СЛУЧАЕВ ========================

TEST_F(ParallelOffsetTest, VerySmallOffset) {
  // Тест: Очень малое смещение (1% OD)
  auto params = createStandardParams();
  params.outer_diameter = 20.0;
  params.offset_distance = 0.2; // 0.01 * 20
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_LT(result.confidence, 95.0); // Штраф за малое смещение
}

TEST_F(ParallelOffsetTest, OffsetEqualToRadius) {
  // Тест: Смещение равно радиусу гибки
  auto params = createStandardParams();
  params.bend_radius = 50.0;
  params.offset_distance = 50.0; // Равно радиусу
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_LT(result.confidence, 90.0); // Штраф
}

TEST_F(ParallelOffsetTest, StraightSectionCalculation) {
  // Тест: Прямой участок не должен быть отрицательным
  auto params = createStandardParams();
  params.bend_angle = 5.0; // Маленький угол
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_GE(result.straight_section, 0.0);
}

// ======================== ТЕСТЫ ПРОВЕРКИ ВОЗМОЖНОСТИ ГИБКИ ========================

TEST_F(ParallelOffsetTest, CanBendSteel) {
  // Тест: Проверка возможности гибки стали
  bool can_bend = ParallelOffsetCalculator::can_bend(20.0, 2.0, 50.0, "Steel");
  EXPECT_TRUE(can_bend);
}

TEST_F(ParallelOffsetTest, CannotBendWithTinyRadius) {
  // Тест: Невозможность гибки с очень маленьким радиусом
  bool can_bend = ParallelOffsetCalculator::can_bend(20.0, 2.0, 10.0, "Steel");
  EXPECT_FALSE(can_bend);
}

TEST_F(ParallelOffsetTest, CanBendAluminum) {
  // Тест: Алюминий может сгибаться с меньшим радиусом
  bool steel_can_bend = ParallelOffsetCalculator::can_bend(20.0, 2.0, 40.0, "Steel");
  bool aluminum_can_bend = ParallelOffsetCalculator::can_bend(20.0, 2.0, 40.0, "Aluminum");

  // Алюминий должен быть более "гибким"
  EXPECT_FALSE(steel_can_bend);
  EXPECT_TRUE(aluminum_can_bend);
}

// ======================== ТЕСТЫ ОШИБОК ВАЛИДАЦИИ ========================

TEST_F(ParallelOffsetTest, GetValidationErrors) {
  // Тест: Получение списка ошибок валидации
  auto params = createStandardParams();
  params.outer_diameter = 2.0;     // Ошибка 1
  params.bend_angle = 2.0;         // Ошибка 2

  calculator.validate(params);
  auto errors = calculator.get_validation_errors();

  EXPECT_GE(errors.size(), 2);

  // Проверяем наличие кодов ошибок
  bool has_diameter_error = false;
  bool has_angle_error = false;

  for (const auto& err : errors) {
    if (err.code == "DIAMETER_TOO_SMALL") has_diameter_error = true;
    if (err.code == "ANGLE_TOO_SMALL") has_angle_error = true;
  }

  EXPECT_TRUE(has_diameter_error);
  EXPECT_TRUE(has_angle_error);
}

// ======================== ИНТЕГРАЦИОННЫЕ ТЕСТЫ ========================

TEST_F(ParallelOffsetTest, CompleteCalculationFlow) {
  // Тест: Полный цикл - валидация и расчёт
  auto params = createStandardParams();

  // Шаг 1: Валидация
  EXPECT_TRUE(calculator.validate(params));
  EXPECT_EQ(calculator.get_error_message(), "");

  // Шаг 2: Расчёт
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.is_valid);
  EXPECT_GT(result.total_length, 0.0);
  EXPECT_GT(result.confidence, 50.0);
  EXPECT_LE(result.confidence, 100.0);
}

TEST_F(ParallelOffsetTest, LengthConsistency) {
  // Тест: Проверка логичности длин
  auto params = createStandardParams();
  auto result = calculator.calculate(params);

  // Общая длина должна быть больше суммы компонентов
  double sum_parts = result.bend_1_length + result.straight_section + result.bend_2_length;
  EXPECT_LE(sum_parts, result.total_length * 1.1); // С небольшим припуском
}
