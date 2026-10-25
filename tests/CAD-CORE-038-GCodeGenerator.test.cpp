#include <gtest/gtest.h>
#include "../src/manufacturing/GCodeGenerator.hpp"

using namespace bendcore::manufacturing;

class GCodeGeneratorTest : public ::testing::Test {
 protected:
  GCodeGenerator generator_;

  Tube createTestTube() {
    Tube tube;
    tube.id = "TUBE-001";
    tube.material = "Steel";
    tube.length = 100.0;
    tube.outer_diameter = 12.0;
    tube.wall_thickness = 1.0;

    Bend bend1;
    bend1.position = 25.0;
    bend1.angle = 45.0;
    bend1.radius = 15.0;
    bend1.dwell_time = 1.0;
    tube.bends.push_back(bend1);

    Bend bend2;
    bend2.position = 75.0;
    bend2.angle = 45.0;
    bend2.radius = 15.0;
    bend2.dwell_time = 1.0;
    tube.bends.push_back(bend2);

    return tube;
  }

  BendingMachine createTestMachine() {
    BendingMachine machine;
    machine.name = "CNC-001";
    machine.type = MachineType::CNC_PIPE_BENDER;
    machine.min_radius = 10.0;
    machine.max_radius = 100.0;
    machine.max_bending_angle = 180.0;
    machine.tube_diameter_range = {5.0, 50.0};
    machine.spindle_speed = 2000;
    machine.cost_per_hour = 50.0;
    machine.availability = 1.0;
    return machine;
  }
};

TEST_F(GCodeGeneratorTest, GenerateBasicGCode) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_FALSE(gcode.empty());
  EXPECT_NE(gcode.find("G"), std::string::npos);  // Contains G-code
}

TEST_F(GCodeGeneratorTest, GCodeContainsHeader) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.include_comments = true;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("TUBE-001"), std::string::npos);
  EXPECT_NE(gcode.find("Steel"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, GCodeValidation) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  std::string gcode = generator_.generateGCode(tube, machine, config);
  bool valid = generator_.validateGCode(gcode);

  EXPECT_TRUE(valid);
}

TEST_F(GCodeGeneratorTest, InvalidGCodeDetection) {
  std::string invalid_gcode = "This is not G-code at all";
  bool valid = generator_.validateGCode(invalid_gcode);

  EXPECT_FALSE(valid);
}

TEST_F(GCodeGeneratorTest, EmptyGCodeDetection) {
  std::string empty_gcode = "";
  bool valid = generator_.validateGCode(empty_gcode);

  EXPECT_FALSE(valid);
}

TEST_F(GCodeGeneratorTest, FanucCodeGeneration) {
  auto tube = createTestTube();
  GCodeGenerator::GCodeConfig config;
  config.controller = GCodeGenerator::MachineControllerType::FANUC;

  std::string gcode = generator_.generateFanucCode(tube, config);

  EXPECT_FALSE(gcode.empty());
  EXPECT_NE(gcode.find("FANUC"), std::string::npos);
  EXPECT_NE(gcode.find("G90"), std::string::npos);
  EXPECT_NE(gcode.find("G54"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, SiemensCodeGeneration) {
  auto tube = createTestTube();
  GCodeGenerator::GCodeConfig config;
  config.controller = GCodeGenerator::MachineControllerType::SIEMENS;

  std::string gcode = generator_.generateSiemensCode(tube, config);

  EXPECT_FALSE(gcode.empty());
  EXPECT_NE(gcode.find("SIEMENS"), std::string::npos);
  EXPECT_NE(gcode.find("SINUMERIK"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, HeidenhainCodeGeneration) {
  auto tube = createTestTube();
  GCodeGenerator::GCodeConfig config;
  config.controller = GCodeGenerator::MachineControllerType::HEIDENHAIN;

  std::string gcode = generator_.generateHeidenhainCode(tube, config);

  EXPECT_FALSE(gcode.empty());
  EXPECT_NE(gcode.find("HEIDENHAIN"), std::string::npos);
  EXPECT_NE(gcode.find("BEGIN PGM"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, EstimateMachiningTime) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  double time = generator_.estimateMachiningTime(tube, machine, config);

  EXPECT_GT(time, 0.0);
  EXPECT_LT(time, 100.0);  // Should be reasonable
}

TEST_F(GCodeGeneratorTest, ToolChanges) {
  auto tube = createTestTube();
  std::vector<GCodeGenerator::ToolDefinition> tools;

  GCodeGenerator::ToolDefinition tool1;
  tool1.tool_number = 1;
  tool1.tool_name = "End Mill 5mm";
  tool1.diameter_mm = 5.0;
  tools.push_back(tool1);

  GCodeGenerator::ToolDefinition tool2;
  tool2.tool_number = 2;
  tool2.tool_name = "Drill 3mm";
  tool2.diameter_mm = 3.0;
  tools.push_back(tool2);

  GCodeGenerator::GCodeConfig config;
  config.include_tool_changes = true;

  std::string gcode = generator_.generateCustomGCode(tube, tools, config);

  EXPECT_NE(gcode.find("T1"), std::string::npos);
  EXPECT_NE(gcode.find("T2"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, FeedRateConfiguration) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.feed_rate_mm_min = 150.0;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("F150"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, SpindleSpeedConfiguration) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.spindle_speed_rpm = 3000;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("S3000"), std::string::npos);
}

TEST_F(GCodeGeneratorTest, DwellTime) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("G04"), std::string::npos);  // Dwell command
}

TEST_F(GCodeGeneratorTest, RapidMove) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("G00"), std::string::npos);  // Rapid move
}

TEST_F(GCodeGeneratorTest, CoordinateSystemAbsolute) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.coordinate_system = GCodeGenerator::CoordinateSystem::ABSOLUTE;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("G90"), std::string::npos);  // Absolute mode
}

TEST_F(GCodeGeneratorTest, BendValidation) {
  auto machine = createTestMachine();
  Bend bend1;
  bend1.position = 25.0;
  bend1.angle = 45.0;
  bend1.radius = 15.0;
  bend1.dwell_time = 1.0;

  bool valid = generator_.validateBendForMachine(bend1, machine);
  EXPECT_TRUE(valid);
}

TEST_F(GCodeGeneratorTest, BendValidationOutOfRange) {
  auto machine = createTestMachine();
  Bend bend;
  bend.position = 25.0;
  bend.angle = 200.0;  // Exceeds max_bending_angle
  bend.radius = 15.0;
  bend.dwell_time = 1.0;

  bool valid = generator_.validateBendForMachine(bend, machine);
  EXPECT_FALSE(valid);
}

TEST_F(GCodeGeneratorTest, BendRadiusValidation) {
  auto machine = createTestMachine();
  Bend bend;
  bend.position = 25.0;
  bend.angle = 45.0;
  bend.radius = 5.0;  // Below min_radius
  bend.dwell_time = 1.0;

  bool valid = generator_.validateBendForMachine(bend, machine);
  EXPECT_FALSE(valid);
}

TEST_F(GCodeGeneratorTest, ExportToFile) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;

  std::string gcode = generator_.generateGCode(tube, machine, config);
  bool success = generator_.exportToFile(gcode, "/tmp/test_gcode.nc");

  EXPECT_TRUE(success);
}

TEST_F(GCodeGeneratorTest, DecimalPrecision) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.decimal_places = 3;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_FALSE(gcode.empty());
}

TEST_F(GCodeGeneratorTest, Comments) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.include_comments = true;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  EXPECT_NE(gcode.find("("), std::string::npos);  // Comment character
}

TEST_F(GCodeGeneratorTest, NoComments) {
  auto tube = createTestTube();
  auto machine = createTestMachine();
  GCodeGenerator::GCodeConfig config;
  config.include_comments = false;

  std::string gcode = generator_.generateGCode(tube, machine, config);

  // Should have valid G-code regardless of comments
  EXPECT_TRUE(generator_.validateGCode(gcode));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
