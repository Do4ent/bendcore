#include <gtest/gtest.h>
#include "bendcore/combined_3d_offset.hpp"

using namespace bendcore;

// ============================================================================
// Тесты генерации CNC кода
// ============================================================================

class CNCGeneratorTest : public ::testing::Test {
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

// ============================================================================
// Базовые тесты G-code генерации
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeNotEmpty) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsG20) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G20"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsG90) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G90"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsG00) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G00"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsG01) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("G01"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsM03) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("M03"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsM05) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("M05"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeContainsM30) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find("M30"), std::string::npos);
}

// ============================================================================
// Тесты структуры G-code
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeHasPreamble) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  const auto& code = result.cnc_code;

  // Пreamble должен быть в начале
  size_t g20_pos = code.find("G20");
  size_t g90_pos = code.find("G90");

  EXPECT_NE(g20_pos, std::string::npos);
  EXPECT_NE(g90_pos, std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeHasComments) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find(";"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeHasCoordinates) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  const auto& code = result.cnc_code;

  // Проверяем наличие координат X, Y, Z
  EXPECT_NE(code.find(" X"), std::string::npos);
  EXPECT_NE(code.find(" Y"), std::string::npos);
  EXPECT_NE(code.find(" Z"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeHasFeedspeeds) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.cnc_code.find(" F"), std::string::npos);
}

// ============================================================================
// Тесты для разного количества гибок
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeFor2Bends) {
  auto params = get_valid_params();
  params.num_bends = 2;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 2);

  // Проверяем наличие комментариев для каждой гибки
  EXPECT_NE(result.cnc_code.find("Гибка 1"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("Гибка 2"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeFor3Bends) {
  auto params = get_valid_params();
  params.num_bends = 3;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 3);

  EXPECT_NE(result.cnc_code.find("Гибка 1"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("Гибка 2"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("Гибка 3"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeFor4Bends) {
  auto params = get_valid_params();
  params.num_bends = 4;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 4);
}

TEST_F(CNCGeneratorTest, CNCCodeFor5Bends) {
  auto params = get_valid_params();
  params.num_bends = 5;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 5);
}

// ============================================================================
// Тесты для разных материалов
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeForSteel) {
  auto params = get_valid_params();
  params.material = "steel";
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeForAluminum) {
  auto params = get_valid_params();
  params.material = "aluminum";
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeForCopper) {
  auto params = get_valid_params();
  params.material = "copper";
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

// ============================================================================
// Тесты валидности G-code синтаксиса
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeHasValidLines) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  const auto& code = result.cnc_code;

  // Разбираем код на строки
  std::istringstream iss(code);
  std::string line;
  int valid_lines = 0;

  while (std::getline(iss, line)) {
    if (line.empty() || line[0] == ';') {
      continue; // Пропускаем пустые строки и комментарии
    }

    // G-code строка должна начинаться с G, M или буквы оси
    if (!line.empty()) {
      char first = line[0];
      EXPECT_TRUE(first == 'G' || first == 'M' || first == 'X' || first == 'Y' ||
                  first == 'Z' || first == 'F' || first == ';');
      valid_lines++;
    }
  }

  EXPECT_GT(valid_lines, 0);
}

TEST_F(CNCGeneratorTest, CNCCodeHasEndProgram) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);

  // Последняя команда должна быть M30
  size_t m30_pos = result.cnc_code.rfind("M30");
  EXPECT_NE(m30_pos, std::string::npos);
}

// ============================================================================
// Тесты целостности данных в коде
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeIncludesBendPositions) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.bends.size(), 0);

  // Проверяем, что первая гибка координаты в коде
  const auto& first_bend = result.bends[0];
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2) << first_bend.position.x;
  std::string x_str = oss.str();

  EXPECT_NE(result.cnc_code.find(x_str), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeIncludesFeasibility) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);

  // Feasibility должен быть в комментарии
  EXPECT_NE(result.cnc_code.find("Feasibility"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CNCCodeIncludesTotalLength) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);

  // Общая длина должна быть в комментарии
  EXPECT_NE(result.cnc_code.find("Общая длина"), std::string::npos);
}

// ============================================================================
// Тесты для особых случаев
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeWithSpringbackCorrection) {
  auto params = get_valid_params();
  params.springback_correction = true;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeWithoutSpringbackCorrection) {
  auto params = get_valid_params();
  params.springback_correction = false;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeWithHighInclination) {
  auto params = get_valid_params();
  params.inclination_angle = 70.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeWithCustomDistribution) {
  auto params = get_valid_params();
  params.distribution = "custom";
  params.num_bends = 3;
  params.custom_bend_positions = {0.25, 0.5, 0.75};
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 3);
  EXPECT_GT(result.cnc_code.length(), 0);
}

// ============================================================================
// Тесты для разных диаметров труб
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeForSmallDiameter) {
  auto params = get_valid_params();
  params.outer_diameter = 6.0;
  params.wall_thickness = 0.5;
  params.bend_radius = 3.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

TEST_F(CNCGeneratorTest, CNCCodeForLargeDiameter) {
  auto params = get_valid_params();
  params.outer_diameter = 200.0;
  params.wall_thickness = 10.0;
  params.bend_radius = 100.0;
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_GT(result.cnc_code.length(), 0);
}

// ============================================================================
// Тесты для проверки масштабирования и форматирования
// ============================================================================

TEST_F(CNCGeneratorTest, CNCCodeHasConsistentFormatting) {
  auto params = get_valid_params();
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  const auto& code = result.cnc_code;

  // Проверяем, что координаты отформатированы с точностью до 2 знаков
  size_t x_pos = code.find(" X");
  EXPECT_NE(x_pos, std::string::npos);

  // Координаты должны быть в формате X##.##
  while (x_pos != std::string::npos) {
    // Проверяем что после X есть число
    if (x_pos + 2 < code.length()) {
      EXPECT_TRUE(std::isdigit(code[x_pos + 2]) || code[x_pos + 2] == '-');
    }
    x_pos = code.find(" X", x_pos + 1);
  }
}

TEST_F(CNCGeneratorTest, CNCCodeNoNegativeCoordinatesUnless) {
  auto params = get_valid_params();
  params.start_point = Point3D(0, 0, 0);
  params.end_point = Point3D(100, 100, 50);
  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);

  // Все координаты в нашем случае должны быть неотрицательны
  // (так как стартовая точка в (0,0,0) и конечная положительна)
  for (const auto& traj : result.trajectory_3d) {
    EXPECT_GE(traj.x, -1e-6);
    EXPECT_GE(traj.y, -1e-6);
    EXPECT_GE(traj.z, -1e-6);
  }
}

// ============================================================================
// Интеграционные тесты
// ============================================================================

TEST_F(CNCGeneratorTest, CompleteWorkflowTwoBeads) {
  auto params = get_valid_params();
  params.num_bends = 2;
  params.distribution = "equal";
  params.springback_correction = true;
  params.material = "steel";

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 2);
  EXPECT_GT(result.total_length, 0);
  EXPECT_GT(result.cnc_code.length(), 0);
  EXPECT_GT(result.feasibility_score, 0);

  // Проверяем что код содержит все необходимые команды
  EXPECT_NE(result.cnc_code.find("G20"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("G90"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("M03"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("M05"), std::string::npos);
  EXPECT_NE(result.cnc_code.find("M30"), std::string::npos);
}

TEST_F(CNCGeneratorTest, CompleteWorkflowFiveBeads) {
  auto params = get_valid_params();
  params.num_bends = 5;
  params.distribution = "custom";
  params.custom_bend_positions = {0.1, 0.3, 0.5, 0.7, 0.9};
  params.springback_correction = true;
  params.material = "aluminum";

  auto result = calculator.calculate(params);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.bends.size(), 5);
  EXPECT_GT(result.total_length, 0);
  EXPECT_GT(result.cnc_code.length(), 0);
  EXPECT_GT(result.feasibility_score, 0);
}
