#include <gtest/gtest.h>
#include "bendcore/simulation/cfd_solver.hpp"
#include "bendcore/simulation/structural_solver.hpp"
#include "bendcore/simulation/thermal_solver.hpp"
#include "bendcore/simulation/simulation_manager.hpp"
#include "bendcore/simulation/result_visualization.hpp"
#include "bendcore/geometry/vector.hpp"

using namespace BendCore::CadCore::Simulation;
using namespace BendCore::CadCore;

// ==================== CFD Solver Tests ====================

class CFDMeshTest : public ::testing::Test {
protected:
    CFDMesh mesh;

    void SetUp() override {
        mesh.clear();
    }
};

TEST_F(CFDMeshTest, AddPointsAndRetrieve) {
    MeshPoint pt1;
    pt1.position = Vector3D(0, 0, 0);
    pt1.id = 0;
    mesh.addPoint(pt1);

    EXPECT_EQ(mesh.getNumPoints(), 1);
    EXPECT_EQ(mesh.getPoints()[0].id, 0);
}

TEST_F(CFDMeshTest, AddMultiplePoints) {
    for (int i = 0; i < 10; i++) {
        MeshPoint pt;
        pt.position = Vector3D(i, i, i);
        pt.id = i;
        mesh.addPoint(pt);
    }

    EXPECT_EQ(mesh.getNumPoints(), 10);
}

TEST_F(CFDMeshTest, AddElements) {
    MeshElement elem;
    elem.id = 0;
    elem.vertex_indices = {0, 1, 2, 3};
    elem.volume = 1.0;
    mesh.addElement(elem);

    EXPECT_EQ(mesh.getNumElements(), 1);
}

TEST_F(CFDMeshTest, ClearMesh) {
    MeshPoint pt;
    pt.position = Vector3D(0, 0, 0);
    pt.id = 0;
    mesh.addPoint(pt);

    EXPECT_EQ(mesh.getNumPoints(), 1);

    mesh.clear();
    EXPECT_EQ(mesh.getNumPoints(), 0);
}

TEST_F(CFDMeshTest, GetTotalVolume) {
    MeshElement elem1, elem2;
    elem1.id = 0;
    elem1.volume = 1.5;
    elem2.id = 1;
    elem2.volume = 2.5;

    mesh.addElement(elem1);
    mesh.addElement(elem2);

    double total = mesh.getTotalVolume();
    EXPECT_NEAR(total, 4.0, 0.01);
}

class CFDSolverTest : public ::testing::Test {
protected:
    CFDSolver solver;
    std::shared_ptr<CFDMesh> mesh;

    void SetUp() override {
        mesh = std::make_shared<CFDMesh>();

        // Create simple mesh
        for (int i = 0; i < 5; i++) {
            MeshPoint pt;
            pt.position = Vector3D(i * 0.5, 0, 0);
            pt.id = i;
            mesh->addPoint(pt);
        }

        solver.setMesh(mesh);
    }
};

TEST_F(CFDSolverTest, SetDensity) {
    solver.setFluidDensity(1.225);
    // Verify by checking convergence (density affects convergence)
}

TEST_F(CFDSolverTest, SetViscosity) {
    solver.setFluidViscosity(1.81e-5);
    // Verify through solve behavior
}

TEST_F(CFDSolverTest, SetTurbulenceModel) {
    solver.setTurbulenceModel(TurbulenceModel::K_EPSILON);
    // Verify turbulence is computed
}

TEST_F(CFDSolverTest, SimpleSolve) {
    BoundaryCondition inlet;
    inlet.type = BoundaryConditionType::DIRICHLET;
    inlet.velocity_x = 1.0;
    inlet.velocity_y = 0.0;
    inlet.velocity_z = 0.0;
    inlet.face_indices = {0};

    solver.addBoundaryCondition(inlet);
    solver.setNumberOfIterations(10);

    bool success = solver.solve();
    EXPECT_TRUE(success);
}

TEST_F(CFDSolverTest, GetMaxVelocity) {
    solver.solve();
    double max_vel = solver.getMaxVelocity();
    EXPECT_GE(max_vel, 0.0);
}

TEST_F(CFDSolverTest, GetMaxPressure) {
    solver.solve();
    double max_press = solver.getMaxPressure();
    EXPECT_GT(max_press, 0.0);
}

TEST_F(CFDSolverTest, GetConvergenceStatus) {
    solver.solve();
    EXPECT_TRUE(solver.isConverged() || solver.getIterationCount() > 0);
}

// ==================== Structural Solver Tests ====================

class StructuralSolverTest : public ::testing::Test {
protected:
    StructuralSolver solver;

    void SetUp() override {
        solver.setNumNodes(4);
        solver.setNumElements(1);

        // Create simple tetrahedral element
        solver.addNodeCoordinates(0, Vector3D(0, 0, 0));
        solver.addNodeCoordinates(1, Vector3D(1, 0, 0));
        solver.addNodeCoordinates(2, Vector3D(0, 1, 0));
        solver.addNodeCoordinates(3, Vector3D(0, 0, 1));

        solver.addElement(0, {0, 1, 2, 3});
    }
};

TEST_F(StructuralSolverTest, SetMaterialProperties) {
    MaterialProperties props;
    props.name = "Aluminum";
    props.young_modulus = 7.0e10;
    props.poisson_ratio = 0.33;

    solver.setMaterialProperties(props);
    EXPECT_EQ(solver.getMaterialProperties().name, "Aluminum");
}

TEST_F(StructuralSolverTest, ComputeShearModulus) {
    MaterialProperties props;
    props.young_modulus = 2.1e11;
    props.poisson_ratio = 0.3;
    props.computeShearModulus();

    double expected_G = 2.1e11 / (2.0 * (1.0 + 0.3));
    EXPECT_NEAR(props.shear_modulus, expected_G, 1e8);
}

TEST_F(StructuralSolverTest, AddLoad) {
    Load load;
    load.type = LoadType::FORCE;
    load.node_id = 0;
    load.force = Vector3D(100, 0, 0);

    solver.addLoad(load);
    // Verify through solve
}

TEST_F(StructuralSolverTest, AddConstraint) {
    Constraint constraint;
    constraint.type = ConstraintType::FIXED;
    constraint.node_id = 0;

    solver.addConstraint(constraint);
    // Verify through solve
}

TEST_F(StructuralSolverTest, SimpleSolve) {
    Constraint constraint;
    constraint.type = ConstraintType::FIXED;
    constraint.node_id = 0;
    solver.addConstraint(constraint);

    Load load;
    load.type = LoadType::FORCE;
    load.node_id = 1;
    load.force = Vector3D(100, 0, 0);
    solver.addLoad(load);

    bool success = solver.solve();
    EXPECT_TRUE(success);
}

TEST_F(StructuralSolverTest, GetMaxStress) {
    solver.solve();
    double max_stress = solver.getMaxStress();
    EXPECT_GE(max_stress, 0.0);
}

TEST_F(StructuralSolverTest, GetMaxDisplacement) {
    solver.solve();
    double max_disp = solver.getMaxDisplacement();
    EXPECT_GE(max_disp, 0.0);
}

TEST_F(StructuralSolverTest, GetMinSafetyFactor) {
    solver.solve();
    double min_sf = solver.getMinSafetyFactor();
    EXPECT_GT(min_sf, 0.0);
}

TEST_F(StructuralSolverTest, VonMisesStress) {
    StressTensor stress;
    stress.components[0] = 100e6;  // σxx
    stress.components[1] = 50e6;   // σyy
    stress.components[2] = 30e6;   // σzz
    stress.components[3] = 0;      // τxy
    stress.components[4] = 0;      // τyz
    stress.components[5] = 0;      // τxz

    double vm = stress.vonMisesStress();
    EXPECT_GT(vm, 0.0);
}

TEST_F(StructuralSolverTest, EquivalentStrain) {
    StrainTensor strain;
    strain.components[0] = 0.001;  // εxx
    strain.components[1] = 0.0005; // εyy
    strain.components[2] = 0.0003; // εzz

    double eq = strain.equivalentStrain();
    EXPECT_GT(eq, 0.0);
}

// ==================== Thermal Solver Tests ====================

class ThermalSolverTest : public ::testing::Test {
protected:
    ThermalSolver solver;

    void SetUp() override {
        solver.setNumNodes(5);

        for (int i = 0; i < 5; i++) {
            solver.addNodeCoordinates(i, Vector3D(i, 0, 0));
        }
    }
};

TEST_F(ThermalSolverTest, SetMaterialProperties) {
    ThermalMaterialProperties props;
    props.name = "Copper";
    props.thermal_conductivity = 400.0;
    props.specific_heat_capacity = 385.0;
    props.density = 8960.0;

    solver.setMaterialProperties(props);
    EXPECT_EQ(solver.getMaterialProperties().name, "Copper");
}

TEST_F(ThermalSolverTest, SetInitialTemperature) {
    solver.setInitialTemperature(373.15);  // 100°C
    // Verify through solve
}

TEST_F(ThermalSolverTest, AddBoundaryCondition) {
    ThermalBoundaryCondition bc;
    bc.type = ThermalBCType::TEMPERATURE;
    bc.temperature = 323.15;  // 50°C
    bc.face_indices = {0};

    solver.addBoundaryCondition(bc);
    // Verify through solve
}

TEST_F(ThermalSolverTest, AddHeatSource) {
    HeatSource source;
    source.element_id = 2;
    source.heat_generation = 1000.0;  // W/m³

    solver.addHeatSource(source);
    // Verify through solve
}

TEST_F(ThermalSolverTest, SteadyStateSolve) {
    ThermalBoundaryCondition bc;
    bc.type = ThermalBCType::TEMPERATURE;
    bc.temperature = 373.15;
    bc.face_indices = {0};
    solver.addBoundaryCondition(bc);

    solver.setTransientMode(false);
    bool success = solver.solve();
    EXPECT_TRUE(success);
}

TEST_F(ThermalSolverTest, TransientSolve) {
    solver.setTimeStep(0.1);
    solver.setTotalTime(1.0);
    solver.setTransientMode(true);

    bool success = solver.solve();
    EXPECT_TRUE(success);
}

TEST_F(ThermalSolverTest, GetMaxTemperature) {
    solver.solve();
    double max_temp = solver.getMaxTemperature();
    EXPECT_GT(max_temp, 250.0);  // Should be above absolute zero in Kelvin
}

TEST_F(ThermalSolverTest, GetMinTemperature) {
    solver.solve();
    double min_temp = solver.getMinTemperature();
    EXPECT_GT(min_temp, 0.0);
}

// ==================== Simulation Manager Tests ====================

class SimulationManagerTest : public ::testing::Test {
protected:
    SimulationManager manager;

    void SetUp() override {
        // Create configuration
        SimulationConfig config;
        config.type = SimulationType::CFD;
        config.name = "Test Simulation";
        manager.setConfiguration(config);
    }
};

TEST_F(SimulationManagerTest, CreateCFDSolver) {
    auto cfd = manager.createCFDSolver();
    EXPECT_NE(cfd, nullptr);
}

TEST_F(SimulationManagerTest, CreateStructuralSolver) {
    auto structural = manager.createStructuralSolver();
    EXPECT_NE(structural, nullptr);
}

TEST_F(SimulationManagerTest, CreateThermalSolver) {
    auto thermal = manager.createThermalSolver();
    EXPECT_NE(thermal, nullptr);
}

TEST_F(SimulationManagerTest, GetProgress) {
    const auto& progress = manager.getProgress();
    EXPECT_GE(progress.progress_percent, 0.0);
    EXPECT_LE(progress.progress_percent, 100.0);
}

TEST_F(SimulationManagerTest, ValidateConfiguration) {
    auto cfd = manager.createCFDSolver();
    bool valid = manager.validateConfiguration();
    EXPECT_TRUE(valid);
}

TEST_F(SimulationManagerTest, ExportToCSV) {
    manager.createCFDSolver();
    manager.createStructuralSolver();
    manager.createThermalSolver();

    bool success = manager.exportToCSV("/tmp/test_results.csv");
    EXPECT_TRUE(success);
}

// ==================== Result Visualization Tests ====================

class ResultVisualizationTest : public ::testing::Test {
protected:
    ResultVisualization visualization;

    void SetUp() override {
        // Create test field
        auto field = std::make_shared<VisualizationField>("TestField", FieldType::SCALAR);

        std::vector<double> scalar_data = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<Vector3D> positions = {
            Vector3D(0, 0, 0),
            Vector3D(1, 0, 0),
            Vector3D(2, 0, 0),
            Vector3D(3, 0, 0),
            Vector3D(4, 0, 0)
        };

        field->setScalarData(scalar_data);
        field->setNodePositions(positions);
        visualization.addField(field);
    }
};

TEST_F(ResultVisualizationTest, AddField) {
    EXPECT_EQ(visualization.getFields().size(), 1);
}

TEST_F(ResultVisualizationTest, GetField) {
    auto field = visualization.getField("TestField");
    EXPECT_NE(field, nullptr);
    EXPECT_EQ(field->getName(), "TestField");
}

TEST_F(ResultVisualizationTest, GenerateScalarFieldColors) {
    visualization.generateScalarFieldColors("TestField", ColorMapType::JET);
    EXPECT_GT(visualization.getScalarFieldColors().size(), 0);
}

TEST_F(ResultVisualizationTest, GenerateIsoSurface) {
    visualization.generateIsoSurface("TestField", 3.0);
    EXPECT_GT(visualization.getIsoSurfaces().size(), 0);
}

TEST_F(ResultVisualizationTest, GenerateVectorGlyphs) {
    auto field = std::make_shared<VisualizationField>("VectorField", FieldType::VECTOR);

    std::vector<Vector3D> vector_data = {
        Vector3D(1, 0, 0),
        Vector3D(1, 0, 0),
        Vector3D(1, 0, 0),
        Vector3D(1, 0, 0),
        Vector3D(1, 0, 0)
    };
    std::vector<Vector3D> positions = {
        Vector3D(0, 0, 0),
        Vector3D(1, 0, 0),
        Vector3D(2, 0, 0),
        Vector3D(3, 0, 0),
        Vector3D(4, 0, 0)
    };

    field->setVectorData(vector_data);
    field->setNodePositions(positions);
    visualization.addField(field);

    visualization.generateVectorGlyphs("VectorField", 1, 1.0);
    EXPECT_GT(visualization.getVectorGlyphs().size(), 0);
}

TEST_F(ResultVisualizationTest, SetColorMap) {
    ColorMap colormap(ColorMapType::VIRIDIS);
    colormap.setRange(0.0, 10.0);
    visualization.setColorMap(colormap);

    EXPECT_EQ(visualization.getColorMap().getMin(), 0.0);
    EXPECT_EQ(visualization.getColorMap().getMax(), 10.0);
}

TEST_F(ResultVisualizationTest, AddContourLevel) {
    ContourLevel level;
    level.value = 2.5;
    level.color = Color(1, 0, 0);
    level.opacity = 0.8;

    visualization.addContourLevel(level);
    EXPECT_EQ(visualization.getContourLevels().size(), 1);
}

TEST_F(ResultVisualizationTest, ExportToVTK) {
    bool success = visualization.exportToVTK("/tmp/test_visualization.vtk");
    EXPECT_TRUE(success);
}

TEST_F(ResultVisualizationTest, ExportColormap) {
    bool success = visualization.exportColormap("/tmp/test_colormap.txt");
    EXPECT_TRUE(success);
}

TEST_F(ResultVisualizationTest, ColorMapJet) {
    ColorMap colormap(ColorMapType::JET);
    colormap.setRange(0.0, 1.0);

    Color c_min = colormap.getColor(0.0);
    Color c_max = colormap.getColor(1.0);

    EXPECT_GE(c_min.b, 0.0);
    EXPECT_LE(c_max.r, 1.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
