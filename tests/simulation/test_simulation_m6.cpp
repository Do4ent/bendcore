#include <gtest/gtest.h>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
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

TEST(CFDSolverModelTest, UnsupportedTurbulenceModelsAreRejected) {
    for (auto model : {TurbulenceModel::SPALART_ALLMARAS, TurbulenceModel::K_OMEGA_SST}) {
        CFDSolver s;
        auto mesh = std::make_shared<CFDMesh>();
        for (int i = 0; i < 3; ++i) {
            MeshPoint p;
            p.position = Vector3D(i, 0, 0);
            mesh->addPoint(p);
        }
        s.setMesh(mesh);
        s.setTurbulenceModel(model);
        EXPECT_FALSE(s.solve());
        EXPECT_FALSE(s.isConverged());
    }
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

TEST_F(CFDSolverTest, ViscousLimitGivesLinearProfile) {
    // Re_cell = rho*u*h/mu = 5e-4: diffusion dominates, so between u=1 at
    // the inlet and a no-slip node the profile is linear.
    BoundaryCondition inlet;
    inlet.type = BoundaryConditionType::DIRICHLET;
    inlet.velocity_x = 1.0;
    inlet.face_indices = {0};
    BoundaryCondition wall;
    wall.type = BoundaryConditionType::NO_SLIP;
    wall.face_indices = {4};
    solver.addBoundaryCondition(inlet);
    solver.addBoundaryCondition(wall);
    solver.setTurbulenceModel(TurbulenceModel::LAMINAR);
    solver.setFluidDensity(1.0);
    solver.setFluidViscosity(1000.0);
    solver.setNumberOfIterations(200);

    ASSERT_TRUE(solver.solve());
    const auto& v = solver.getVelocityField();
    EXPECT_NEAR(v[1].velocity_x, 0.75, 1e-3);
    EXPECT_NEAR(v[2].velocity_x, 0.50, 1e-3);
    EXPECT_NEAR(v[3].velocity_x, 0.25, 1e-3);
    EXPECT_DOUBLE_EQ(v[4].velocity_x, 0.0);
}

TEST_F(CFDSolverTest, ConvectionDominatedStaysBoundedAndMonotone) {
    // Air at 1 m/s on h = 0.5 m: Re_cell ~ 3e4. Upwinding must not
    // overshoot: 0 <= u <= 1 and u decreases towards the wall.
    BoundaryCondition inlet;
    inlet.type = BoundaryConditionType::DIRICHLET;
    inlet.velocity_x = 1.0;
    inlet.face_indices = {0};
    BoundaryCondition wall;
    wall.type = BoundaryConditionType::NO_SLIP;
    wall.face_indices = {4};
    solver.addBoundaryCondition(inlet);
    solver.addBoundaryCondition(wall);
    solver.setTurbulenceModel(TurbulenceModel::LAMINAR);
    solver.setNumberOfIterations(200);

    ASSERT_TRUE(solver.solve());
    const auto& v = solver.getVelocityField();
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_GE(v[i].velocity_x, 0.0);
        EXPECT_LE(v[i].velocity_x, 1.0 + 1e-12);
        if (i > 0) EXPECT_LE(v[i].velocity_x, v[i - 1].velocity_x + 1e-12);
    }
    EXPECT_GT(v[3].velocity_x, 0.99);  // boundary layer only at the wall
}

TEST_F(CFDSolverTest, NoBoundaryConditionsMeansFluidAtRest) {
    ASSERT_TRUE(solver.solve());
    EXPECT_DOUBLE_EQ(solver.getMaxVelocity(), 0.0);
    EXPECT_DOUBLE_EQ(solver.getMaxPressure(), 101325.0);
}

TEST_F(CFDSolverTest, TemperatureIsAdvectedDownstream) {
    BoundaryCondition inlet;
    inlet.type = BoundaryConditionType::DIRICHLET;
    inlet.velocity_x = 1.0;
    inlet.temperature = 350.0;
    inlet.face_indices = {0};
    solver.addBoundaryCondition(inlet);
    solver.setTurbulenceModel(TurbulenceModel::LAMINAR);
    solver.setNumberOfIterations(50);

    ASSERT_TRUE(solver.solve());
    for (double t : solver.getTemperatureField()) EXPECT_NEAR(t, 350.0, 1e-9);
}

TEST_F(CFDSolverTest, KEpsilonProducesTurbulenceDownstream) {
    BoundaryCondition inlet;
    inlet.type = BoundaryConditionType::DIRICHLET;
    inlet.velocity_x = 10.0;
    inlet.face_indices = {0};
    BoundaryCondition wall;
    wall.type = BoundaryConditionType::NO_SLIP;
    wall.face_indices = {4};
    solver.addBoundaryCondition(inlet);
    solver.addBoundaryCondition(wall);
    solver.setTurbulenceModel(TurbulenceModel::K_EPSILON);
    solver.setNumberOfIterations(500);

    ASSERT_TRUE(solver.solve());
    const auto& v = solver.getVelocityField();
    // Inlet k = 1.5 (0.05 * 10)^2 = 0.375
    EXPECT_NEAR(v[0].turbulent_kinetic_energy, 0.375, 1e-12);
    for (size_t i = 1; i < 4; ++i) {
        EXPECT_GT(v[i].turbulent_kinetic_energy, 0.0);
        EXPECT_GT(v[i].turbulent_dissipation, 0.0);
        EXPECT_TRUE(std::isfinite(v[i].velocity_x));
    }
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

TEST_F(StructuralSolverTest, OnlyOneFixedNodeIsAMechanism) {
    // Node 0 fixed, force at node 1: the tetrahedron can still rotate about node 0.
    Constraint constraint;
    constraint.type = ConstraintType::FIXED;
    constraint.node_id = 0;
    solver.addConstraint(constraint);

    Load load;
    load.type = LoadType::FORCE;
    load.node_id = 1;
    load.force = Vector3D(100, 0, 0);
    solver.addLoad(load);

    EXPECT_FALSE(solver.solve());
    EXPECT_FALSE(solver.isConverged());
}

TEST_F(StructuralSolverTest, UnsupportedLoadIsRejected) {
    for (size_t node : {0u, 2u, 3u}) {
        Constraint c;
        c.type = ConstraintType::FIXED;
        c.node_id = node;
        solver.addConstraint(c);
    }
    Load load;
    load.type = LoadType::PRESSURE;
    load.node_id = 1;
    load.pressure = 1e5;
    solver.addLoad(load);
    EXPECT_FALSE(solver.solve());
}

namespace {

// Unit cube [0,1]^3 split into 6 tetrahedra (Kuhn triangulation along the 000-111 diagonal).
// Node index = x + 2y + 4z.
void buildCube(StructuralSolver& s) {
    s.setNumNodes(8);
    for (int i = 0; i < 8; ++i) s.addNodeCoordinates(i, Vector3D(i & 1, (i >> 1) & 1, (i >> 2) & 1));
    const int axis_bit[3] = {1, 2, 4};
    const int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    s.setNumElements(6);
    for (int e = 0; e < 6; ++e) {
        const int a = axis_bit[perms[e][0]], b = axis_bit[perms[e][1]];
        s.addElement(e, {0u, static_cast<size_t>(a), static_cast<size_t>(a + b), 7u});
    }
}

// x = 0 face: u_x = 0; origin fully fixed; (0,1,0) u_z = 0; (0,0,1) u_y = 0. Removes all
// rigid-body modes without restraining the lateral (Poisson) contraction.
void supportCube(StructuralSolver& s) {
    for (size_t node : {0u, 2u, 4u, 6u}) {
        Constraint c;
        c.type = ConstraintType::FIXED_X;
        c.node_id = node;
        s.addConstraint(c);
    }
    Constraint origin;
    origin.type = ConstraintType::FIXED;
    origin.node_id = 0;
    s.addConstraint(origin);
    Constraint z;
    z.type = ConstraintType::FIXED_Z;
    z.node_id = 2;
    s.addConstraint(z);
    Constraint y;
    y.type = ConstraintType::FIXED_Y;
    y.node_id = 4;
    s.addConstraint(y);
}

void expectUniaxial(const StructuralSolver& s, double sigma, double E, double nu) {
    // Patch test: constant-strain tetrahedra reproduce uniform uniaxial stress exactly.
    const double eps = sigma / E;
    for (const auto& r : s.getElementResults()) {
        EXPECT_NEAR(r.stress.xx(), sigma, 1e-6 * sigma);
        for (int k = 1; k < 6; ++k) EXPECT_NEAR(r.stress.components[k], 0.0, 1e-6 * sigma);
        EXPECT_NEAR(r.strain.xx(), eps, 1e-9 * eps + 1e-15);
        EXPECT_NEAR(r.strain.yy(), -nu * eps, 1e-9 * eps + 1e-15);
        EXPECT_NEAR(r.strain.zz(), -nu * eps, 1e-9 * eps + 1e-15);
        EXPECT_NEAR(r.stress.vonMisesStress(), sigma, 1e-6 * sigma);
        EXPECT_NEAR(r.stress.principalStress1(), sigma, 1e-6 * sigma);
        EXPECT_NEAR(r.stress.principalStress3(), 0.0, 1e-6 * sigma);
    }
    for (int i = 0; i < 8; ++i) {
        const Vector3D x(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        const Vector3D& u = s.getDisplacements()[i];
        EXPECT_NEAR(u.x, eps * x.x, 1e-9 * eps);
        EXPECT_NEAR(u.y, -nu * eps * x.y, 1e-9 * eps);
        EXPECT_NEAR(u.z, -nu * eps * x.z, 1e-9 * eps);
    }
}

}  // namespace

TEST(StructuralPatchTest, PrescribedStretchGivesHookeStress) {
    StructuralSolver s;
    const double E = 2.1e11, nu = 0.3, delta = 1e-4;  // cube edge 1 m
    buildCube(s);
    supportCube(s);
    for (size_t node : {1u, 3u, 5u, 7u}) {
        Constraint c;
        c.type = ConstraintType::FIXED_X;
        c.node_id = node;
        c.displacement = Vector3D(delta, 0, 0);
        s.addConstraint(c);
    }
    ASSERT_TRUE(s.solve());
    expectUniaxial(s, E * delta, E, nu);
    EXPECT_NEAR(s.getMinSafetyFactor(), s.getMaterialProperties().yield_strength / (E * delta), 1e-6);
}

TEST(StructuralPatchTest, ConsistentFaceForcesGiveUniformTension) {
    // Uniform traction sigma on the x = 1 face, split by the Kuhn triangulation into
    // triangles (1,3,7) and (1,5,7): consistent loads area/3 per vertex -> nodes 1 and 7
    // carry sigma/3, nodes 3 and 5 carry sigma/6.
    StructuralSolver s;
    const double sigma = 1.0e6;
    buildCube(s);
    supportCube(s);
    const std::pair<size_t, double> loads[] = {{1, 1.0 / 3}, {7, 1.0 / 3}, {3, 1.0 / 6}, {5, 1.0 / 6}};
    for (const auto& [node, share] : loads) {
        Load l;
        l.type = LoadType::FORCE;
        l.node_id = node;
        l.force = Vector3D(sigma * share, 0, 0);
        s.addLoad(l);
    }
    ASSERT_TRUE(s.solve());
    expectUniaxial(s, sigma, 2.1e11, 0.3);
    EXPECT_NEAR(s.getMaxStress(), sigma, 1e-6 * sigma);
}

TEST_F(StructuralSolverTest, UnloadedSolveHasNoStress) {
    for (size_t node : {0u, 2u, 3u}) {
        Constraint c;
        c.type = ConstraintType::FIXED;
        c.node_id = node;
        solver.addConstraint(c);
    }
    ASSERT_TRUE(solver.solve());
    EXPECT_EQ(solver.getMaxStress(), 0.0);
    EXPECT_EQ(solver.getMaxDisplacement(), 0.0);
    EXPECT_TRUE(std::isinf(solver.getMinSafetyFactor()));
}

TEST_F(StructuralSolverTest, VonMisesStress) {
    StressTensor stress;
    stress.components[0] = 100e6;  // σxx
    stress.components[1] = 50e6;   // σyy
    stress.components[2] = 30e6;   // σzz
    stress.components[3] = 0;      // τxy
    stress.components[4] = 0;      // τyz
    stress.components[5] = 0;      // τxz

    // sqrt(((100-50)^2 + (50-30)^2 + (30-100)^2) / 2) MPa = sqrt(3900) MPa
    EXPECT_NEAR(stress.vonMisesStress(), std::sqrt(3900.0) * 1e6, 1.0);
    // Pure shear tau: principal stresses +tau, 0, -tau.
    StressTensor shear;
    shear.components[3] = 40e6;
    EXPECT_NEAR(shear.principalStress1(), 40e6, 1e-3);
    EXPECT_NEAR(shear.principalStress2(), 0.0, 1e-3);
    EXPECT_NEAR(shear.principalStress3(), -40e6, 1e-3);
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

TEST_F(ThermalSolverTest, SteadyStateLinearProfile) {
    // Bar x = 0..4 with fixed ends: steady conduction gives a linear profile
    ThermalBoundaryCondition hot;
    hot.type = ThermalBCType::TEMPERATURE;
    hot.temperature = 400.0;
    hot.face_indices = {0};
    ThermalBoundaryCondition cold = hot;
    cold.temperature = 300.0;
    cold.face_indices = {4};
    solver.addBoundaryCondition(hot);
    solver.addBoundaryCondition(cold);
    solver.setTransientMode(false);

    ASSERT_TRUE(solver.solve());
    const auto& r = solver.getTemperatureField();
    ASSERT_EQ(r.size(), 5u);
    EXPECT_NEAR(r[1].temperature, 375.0, 1e-9);
    EXPECT_NEAR(r[2].temperature, 350.0, 1e-9);
    EXPECT_NEAR(r[3].temperature, 325.0, 1e-9);
}

TEST_F(ThermalSolverTest, SteadyStateHeatSource) {
    // Ends at 300 K, source q at the middle node: T_mid = 300 + q/k
    ThermalBoundaryCondition ends;
    ends.type = ThermalBCType::TEMPERATURE;
    ends.temperature = 300.0;
    ends.face_indices = {0, 4};
    solver.addBoundaryCondition(ends);
    HeatSource source;
    source.element_id = 2;
    source.heat_generation = 1000.0;
    solver.addHeatSource(source);
    solver.setTransientMode(false);

    ASSERT_TRUE(solver.solve());
    const double k = solver.getMaterialProperties().thermal_conductivity;
    const auto& r = solver.getTemperatureField();
    EXPECT_NEAR(r[2].temperature, 300.0 + 1000.0 / k, 1e-9);
    EXPECT_NEAR(r[1].temperature, 300.0 + 500.0 / k, 1e-9);
    EXPECT_NEAR(r[1].temperature, r[3].temperature, 1e-9);
}

TEST_F(ThermalSolverTest, SteadyStateWithoutAnchorFails) {
    // Only an adiabatic BC: temperature level is undetermined
    solver.setTransientMode(false);
    EXPECT_FALSE(solver.solve());
}

TEST_F(ThermalSolverTest, TransientSolve) {
    solver.setTimeStep(0.1);
    solver.setTotalTime(1.0);
    solver.setTransientMode(true);

    // Без граничных условий и источников поле остаётся равным начальному.
    ASSERT_TRUE(solver.solve());
    EXPECT_EQ(solver.getIterationCount(), 10u);
    EXPECT_NEAR(solver.getElapsedTime(), 1.0, 1e-12);
    for (const auto& r : solver.getTemperatureField()) {
        EXPECT_NEAR(r.temperature, 293.15, 1e-9);
    }
}

// Три узла с шагом h = 1, концы держат 400 K, середина стартует с 300 K, a = k/(rho c) = 1.
// Для узла 1: dT/dt = 2a (400 - T) / h^2. Неявный Эйлер с шагом dt даёт точно
// T_n = 400 - 100 (1 + 2a dt)^-n; при dt -> 0 — решение ОДУ 400 - 100 exp(-2 a t).
TEST(ThermalTransientTest, BackwardEulerMatchesClosedForm) {
    auto run = [](double dt, double total) {
        ThermalSolver s;
        ThermalMaterialProperties m;
        m.name = "unit";
        m.thermal_conductivity = 1.0;
        m.density = 1.0;
        m.specific_heat_capacity = 1.0;
        s.setMaterialProperties(m);
        s.setNumNodes(3);
        for (int i = 0; i < 3; ++i) s.addNodeCoordinates(i, Vector3D(i, 0, 0));
        s.setInitialTemperature(300.0);
        ThermalBoundaryCondition ends;
        ends.type = ThermalBCType::TEMPERATURE;
        ends.temperature = 400.0;
        ends.face_indices = {0, 2};
        s.addBoundaryCondition(ends);
        s.setTimeStep(dt);
        s.setTotalTime(total);
        s.setTransientMode(true);
        EXPECT_TRUE(s.solve());
        return s.getTemperatureField()[1].temperature;
    };
    EXPECT_NEAR(run(0.1, 1.0), 400.0 - 100.0 * std::pow(1.2, -10.0), 1e-9);
    // Сходимость к решению ОДУ: ошибка неявного Эйлера O(dt).
    const double exact = 400.0 - 100.0 * std::exp(-2.0 * 0.5);
    EXPECT_NEAR(run(1e-3, 0.5), exact, 0.05);
    EXPECT_LT(std::abs(run(1e-3, 0.5) - exact), std::abs(run(1e-2, 0.5) - exact));
}

// Тепловой поток q = -k dT/dx на линейном профиле (400 -> 300 K на 4 м): 25 k Вт/м^2 по +x.
TEST_F(ThermalSolverTest, HeatFluxFollowsFourierLaw) {
    ThermalBoundaryCondition hot;
    hot.type = ThermalBCType::TEMPERATURE;
    hot.temperature = 400.0;
    hot.face_indices = {0};
    ThermalBoundaryCondition cold = hot;
    cold.temperature = 300.0;
    cold.face_indices = {4};
    solver.addBoundaryCondition(hot);
    solver.addBoundaryCondition(cold);
    ASSERT_TRUE(solver.solve());
    const double k = solver.getMaterialProperties().thermal_conductivity;
    for (const auto& r : solver.getTemperatureField()) {
        EXPECT_NEAR(r.heat_flux.x, 25.0 * k, 1e-6);
        EXPECT_NEAR(r.heat_flux.y, 0.0, 1e-9);
    }
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
    const std::string path = ::testing::TempDir() + "test_visualization.vtk";
    ASSERT_TRUE(visualization.exportToVTK(path));
    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("POINTS 5 double\n0 0 0\n1 0 0\n2 0 0\n3 0 0\n4 0 0\n"), std::string::npos);
    EXPECT_NE(text.find("CELLS 5 10\n1 0\n1 1\n"), std::string::npos);
    EXPECT_NE(text.find("POINT_DATA 5\nSCALARS TestField double 1\nLOOKUP_TABLE default\n1\n2\n3\n4\n5\n"),
              std::string::npos);

    // A field on other nodes cannot share the point list: export refuses.
    auto other = std::make_shared<VisualizationField>("Other", FieldType::SCALAR);
    other->setScalarData({1.0});
    other->setNodePositions({Vector3D(9, 9, 9)});
    visualization.addField(other);
    EXPECT_FALSE(visualization.exportToVTK(path));
}

TEST(ColorMapTest, ViridisMatchesMatplotlibSamples) {
    ColorMap colormap(ColorMapType::VIRIDIS);
    colormap.setRange(0.0, 1.0);
    const Color lo = colormap.getColor(0.0);
    const Color mid = colormap.getColor(128.0 / 255.0);
    const Color hi = colormap.getColor(1.0);
    EXPECT_NEAR(lo.r, 0.267004, 1e-6);
    EXPECT_NEAR(lo.b, 0.329415, 1e-6);
    EXPECT_NEAR(mid.g, 0.566949, 1e-6);
    EXPECT_NEAR(hi.r, 0.993248, 1e-6);
    EXPECT_NEAR(hi.g, 0.906157, 1e-6);
    // Green channel grows monotonically along viridis.
    double prev = -1.0;
    for (int i = 0; i <= 20; ++i) {
        const double g = colormap.getColor(i / 20.0).g;
        EXPECT_GT(g, prev);
        prev = g;
    }
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
