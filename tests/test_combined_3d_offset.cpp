#include <gtest/gtest.h>
#include "bendcore/combined_3d_offset.hpp"

using namespace bendcore;

// ============================================================================
// Базовые тесты базовых операций с Vector3D
// ============================================================================

class Vector3DTest : public ::testing::Test {
protected:
  Vector3D v1{1.0, 2.0, 3.0};
  Vector3D v2{4.0, 5.0, 6.0};
};

TEST_F(Vector3DTest, Addition) {
  Vector3D result = v1 + v2;
  EXPECT_DOUBLE_EQ(result.x, 5.0);
  EXPECT_DOUBLE_EQ(result.y, 7.0);
  EXPECT_DOUBLE_EQ(result.z, 9.0);
}

TEST_F(Vector3DTest, Subtraction) {
  Vector3D result = v2 - v1;
  EXPECT_DOUBLE_EQ(result.x, 3.0);
  EXPECT_DOUBLE_EQ(result.y, 3.0);
  EXPECT_DOUBLE_EQ(result.z, 3.0);
}

TEST_F(Vector3DTest, ScalarMultiplication) {
  Vector3D result = v1 * 2.0;
  EXPECT_DOUBLE_EQ(result.x, 2.0);
  EXPECT_DOUBLE_EQ(result.y, 4.0);
  EXPECT_DOUBLE_EQ(result.z, 6.0);
}

TEST_F(Vector3DTest, DotProduct) {
  double result = v1.dot(v2);
  // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
  EXPECT_DOUBLE_EQ(result, 32.0);
}

TEST_F(Vector3DTest, CrossProduct) {
  Vector3D result = v1.cross(v2);
  // v1 × v2 = (2*6-3*5, 3*4-1*6, 1*5-2*4) = (-3, 6, -3)
  EXPECT_DOUBLE_EQ(result.x, -3.0);
  EXPECT_DOUBLE_EQ(result.y, 6.0);
  EXPECT_DOUBLE_EQ(result.z, -3.0);
}

TEST_F(Vector3DTest, Magnitude) {
  // |v1| = sqrt(1^2 + 2^2 + 3^2) = sqrt(14) ≈ 3.742
  EXPECT_DOUBLE_EQ(v1.magnitude(), std::sqrt(14.0));
}

TEST_F(Vector3DTest, Normalization) {
  Vector3D normalized = v1.normalized();
  double mag = normalized.magnitude();
  EXPECT_NEAR(mag, 1.0, 1e-10);
}

// ============================================================================
// Тесты валидации параметров
// ============================================================================

class Combined3DOffsetValidationTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = true;
    p.material = "steel";
    return p;
  }
};

TEST_F(Combined3DOffsetValidationTest, ValidParams) {
  auto params = get_valid_params();
  std::vector<std::string> errors;
  EXPECT_TRUE(calculator.validate(params, errors));
  EXPECT_EQ(errors.size(), 0);
}

TEST_F(Combined3DOffsetValidationTest, InvalidDistance_TooSmall) {
  auto params = get_valid_params();
  params.start_point = Point3D(0, 0, 0);
  params.end_point = Point3D(1, 1, 1);
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
  EXPECT_GT(errors.size(), 0);
}

TEST_F(Combined3DOffsetValidationTest, InvalidDiameter_TooSmall) {
  auto params = get_valid_params();
  params.outer_diameter = 0.5;
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidWallThickness) {
  auto params = get_valid_params();
  params.wall_thickness = 0.01; // Слишком мало
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidNumBends_TooFew) {
  auto params = get_valid_params();
  params.num_bends = 1;
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidNumBends_TooMany) {
  auto params = get_valid_params();
  params.num_bends = 6;
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidDistribution) {
  auto params = get_valid_params();
  params.distribution = "invalid";
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidInclinationAngle) {
  auto params = get_valid_params();
  params.inclination_angle = 90.0;
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, InvalidMaterial) {
  auto params = get_valid_params();
  params.material = "titanium";
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, CustomPositionsWrongCount) {
  auto params = get_valid_params();
  params.distribution = "custom";
  params.num_bends = 3;
  params.custom_bend_positions = {0.25, 0.75}; // Неверное количество
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

TEST_F(Combined3DOffsetValidationTest, CustomPositionsOutOfRange) {
  auto params = get_valid_params();
  params.distribution = "custom";
  params.num_bends = 2;
  params.custom_bend_positions = {0.25, 1.5}; // > 1.0
  std::vector<std::string> errors;
  EXPECT_FALSE(calculator.validate(params, errors));
}

// ============================================================================
// Тесты расчёта с разными конфигурациями гибок
// ============================================================================

class Combined3DOffsetCalculationTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(Combined3DOffsetCalculationTest, TwoBendSimpleOffset) {
  auto params = get_valid_params();
  params.num_bends = 2;
  params.distribution = "equal";

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 2);
  EXPECT_GT(result.total_length, 0);
  EXPECT_GT(result.total_distance_3d, 0);
  EXPECT_GT(result.feasibility_score, 0);
  EXPECT_LT(result.feasibility_score, 100);
}

TEST_F(Combined3DOffsetCalculationTest, ThreeBendSpiral) {
  auto params = get_valid_params();
  params.num_bends = 3;
  params.distribution = "equal";
  params.inclination_angle = 45.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 3);
  EXPECT_GT(result.total_length, 0);
}

TEST_F(Combined3DOffsetCalculationTest, FourBendComplex) {
  auto params = get_valid_params();
  params.num_bends = 4;
  params.distribution = "equal";

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 4);
  EXPECT_GT(result.feasibility_score, 0);
}

TEST_F(Combined3DOffsetCalculationTest, FiveBendExtreme) {
  auto params = get_valid_params();
  params.num_bends = 5;
  params.distribution = "equal";
  params.bend_radius = 25.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 5);
}

TEST_F(Combined3DOffsetCalculationTest, CustomDistribution) {
  auto params = get_valid_params();
  params.num_bends = 3;
  params.distribution = "custom";
  params.custom_bend_positions = {0.2, 0.5, 0.8};

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 3);
  // Проверяем, что гибки находятся на правильных позициях
  Vector3D dir = params.end_point - params.start_point;
  Vector3D expected_first = params.start_point + dir * 0.2;
  EXPECT_NEAR(result.bends[0].position.x, expected_first.x, 1.0);
}

TEST_F(Combined3DOffsetCalculationTest, SpringbackCorrection_Steel) {
  auto params = get_valid_params();
  params.springback_correction = true;
  params.material = "steel";
  params.num_bends = 2;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.bends[0].springback_correction, 0.0);
  EXPECT_GT(result.bends[0].radius, 26.0); // должна быть коррекция
}

TEST_F(Combined3DOffsetCalculationTest, SpringbackCorrection_Aluminum) {
  auto params = get_valid_params();
  params.springback_correction = true;
  params.material = "aluminum";
  params.num_bends = 2;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  // Алюминий имеет большую коррекцию чем сталь
  EXPECT_GT(result.bends[0].springback_correction, 0.02);
}

TEST_F(Combined3DOffsetCalculationTest, SpringbackCorrection_Copper) {
  auto params = get_valid_params();
  params.springback_correction = true;
  params.material = "copper";
  params.num_bends = 2;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.bends[0].springback_correction, 0.0);
}

TEST_F(Combined3DOffsetCalculationTest, NoSpringbackCorrection) {
  auto params = get_valid_params();
  params.springback_correction = false;
  params.num_bends = 2;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends[0].springback_correction, 0.0);
}

// ============================================================================
// Тесты Feasibility проверки
// ============================================================================

class FeasibilityTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(FeasibilityTest, HighFeasibility_NormalParams) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.feasibility_score, 70.0);
}

TEST_F(FeasibilityTest, LowFeasibility_HighInclination) {
  auto params = get_valid_params();
  params.inclination_angle = 70.0;
  params.num_bends = 5;
  params.bend_radius = 20.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_LT(result.feasibility_score, 90.0);
}

TEST_F(FeasibilityTest, Feasibility_Range) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GE(result.feasibility_score, 0.0);
  EXPECT_LE(result.feasibility_score, 100.0);
}

// ============================================================================
// Тесты генерации траектории
// ============================================================================

class TrajectoryTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(TrajectoryTest, TrajectoryGeneration) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.trajectory_3d.size(), 0);
  // Траектория должна содержать стартовую и финишную точки
  EXPECT_EQ(result.trajectory_3d.front(), params.start_point);
  EXPECT_EQ(result.trajectory_3d.back(), params.end_point);
}

TEST_F(TrajectoryTest, TrajectoryPointCount) {
  auto params = get_valid_params();
  params.num_bends = 3;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  // Траектория должна включать start + bends + end
  EXPECT_EQ(result.trajectory_3d.size(), params.num_bends + 2);
}

// ============================================================================
// Тесты генерации G-code
// ============================================================================

class CNCCodeGenerationTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(CNCCodeGenerationTest, CNCCodeGeneration) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCCodeGenerationTest, CNCCodeContainsPreamble) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G20"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("G90"), std::string::npos);
}

TEST_F(CNCCodeGenerationTest, CNCCodeContainsBends) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G01"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("Гибка"), std::string::npos);
}

TEST_F(CNCCodeGenerationTest, CNCCodeContainsShutdown) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("M05"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("M30"), std::string::npos);
}

// ============================================================================
// Тесты обработки граничных случаев
// ============================================================================

class EdgeCasesTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 0.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(EdgeCasesTest, MinimumValidDistance) {
  auto params = get_valid_params();
  params.start_point = Point3D(0, 0, 0);
  params.end_point = Point3D(10.1, 0, 0); // ~10.1 мм

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, ZeroInclinationAngle) {
  auto params = get_valid_params();
  params.inclination_angle = 0.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, MaxInclinationAngle) {
  auto params = get_valid_params();
  params.inclination_angle = 85.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, ThinWall) {
  auto params = get_valid_params();
  params.wall_thickness = 0.5;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, ThickWall) {
  auto params = get_valid_params();
  params.wall_thickness = 10.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, SmallDiameter) {
  auto params = get_valid_params();
  params.outer_diameter = 6.0;
  params.wall_thickness = 0.5;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

TEST_F(EdgeCasesTest, LargeDiameter) {
  auto params = get_valid_params();
  params.outer_diameter = 200.0;
  params.bend_radius = 100.0;

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
}

// ============================================================================
// Тесты для вычисляемых значений
// ============================================================================

class CalculatedValuesTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator calculator;

  Combined3DOffsetCalculator::Parameters get_valid_params() {
    Combined3DOffsetCalculator::Parameters p;
    p.start_point = Point3D(0, 0, 0);
    p.end_point = Point3D(100, 100, 50);
    p.outer_diameter = 50.0;
    p.wall_thickness = 2.0;
    p.bend_radius = 30.0;
    p.num_bends = 2;
    p.distribution = "equal";
    p.inclination_angle = 30.0;
    p.springback_correction = false;
    p.material = "steel";
    return p;
  }
};

TEST_F(CalculatedValuesTest, BendIndexing) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  for (size_t i = 0; i < result.bends.size(); ++i) {
    EXPECT_EQ(result.bends[i].bend_index, static_cast<int>(i) + 1);
  }
}

TEST_F(CalculatedValuesTest, SectionIndexing) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  for (size_t i = 0; i < result.sections.size(); ++i) {
    EXPECT_EQ(result.sections[i].section_index, static_cast<int>(i));
  }
}

TEST_F(CalculatedValuesTest, AngleConsistency) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  for (const auto& bend : result.bends) {
    EXPECT_GE(bend.angle_xy, 0.0);
    EXPECT_LE(bend.angle_xy, 360.0);
    EXPECT_GE(bend.angle_z, -90.0);
    EXPECT_LE(bend.angle_z, 90.0);
  }
}
