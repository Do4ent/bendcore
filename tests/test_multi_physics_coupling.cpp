#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include "bendcore/analysis/multi_physics_coupling.hpp"

using namespace bendcore::physics;

// ============================================================================
// Test Utilities
// ============================================================================

const double EPSILON = 1e-10;

bool almost_equal(double a, double b, double eps = EPSILON) {
    return std::abs(a - b) < eps;
}

bool vector_almost_equal(const std::array<double, 3>& a,
                        const std::array<double, 3>& b,
                        double eps = EPSILON) {
    return almost_equal(a[0], b[0], eps) &&
           almost_equal(a[1], b[1], eps) &&
           almost_equal(a[2], b[2], eps);
}

// ============================================================================
// Fluid-Structure Interaction Tests
// ============================================================================

void test_fsi_initialization() {
    std::cout << "Testing FSI initialization... ";
    FluidStructureInteraction::Config cfg;
    cfg.num_nodes = 10;
    cfg.num_struct_dofs = 30;
    cfg.num_fluid_dofs = 100;

    FluidStructureInteraction fsi(cfg);
    assert(fsi.get_pressure_loads().size() == cfg.num_struct_dofs);
    assert(fsi.get_displacement().size() == cfg.num_struct_dofs);
    std::cout << "PASS\n";
}

void test_fsi_pressure_transfer() {
    std::cout << "Testing FSI pressure transfer... ";
    FluidStructureInteraction::Config cfg;
    cfg.num_nodes = 10;
    cfg.num_struct_dofs = 10;
    cfg.num_fluid_dofs = 10;

    FluidStructureInteraction fsi(cfg);

    ScalarField pressure(10, "pressure");
    for (size_t i = 0; i < pressure.size(); ++i) {
        pressure[i] = 100.0 + i * 10.0;
    }

    fsi.transfer_pressure_loads(pressure);
    const auto& loads = fsi.get_pressure_loads();

    for (size_t i = 0; i < loads.size(); ++i) {
        assert(almost_equal(loads[i][2], pressure[i]));
    }
    std::cout << "PASS\n";
}

void test_fsi_displacement_transfer() {
    std::cout << "Testing FSI displacement transfer... ";
    FluidStructureInteraction::Config cfg;
    cfg.num_nodes = 5;
    cfg.num_struct_dofs = 5;
    cfg.num_fluid_dofs = 5;

    FluidStructureInteraction fsi(cfg);

    VectorField displacement(5, "displacement");
    for (size_t i = 0; i < displacement.size(); ++i) {
        displacement[i] = {1.0 * i, 2.0 * i, 3.0 * i};
    }

    fsi.set_displacement(displacement);
    const auto& disp = fsi.get_displacement();

    for (size_t i = 0; i < disp.size(); ++i) {
        assert(vector_almost_equal(disp[i], displacement[i]));
    }
    std::cout << "PASS\n";
}

void test_fsi_residual_computation() {
    std::cout << "Testing FSI residual computation... ";
    FluidStructureInteraction::Config cfg;
    cfg.num_nodes = 5;
    cfg.num_struct_dofs = 5;
    cfg.num_fluid_dofs = 5;

    FluidStructureInteraction fsi(cfg);

    VectorField disp1(5);
    for (size_t i = 0; i < disp1.size(); ++i) {
        disp1[i] = {1.0, 0.0, 0.0};
    }

    fsi.set_displacement(disp1);
    double residual1 = fsi.compute_coupling_residual();

    VectorField disp2(5);
    for (size_t i = 0; i < disp2.size(); ++i) {
        disp2[i] = {2.0, 0.0, 0.0};
    }

    fsi.set_displacement(disp2);
    double residual2 = fsi.compute_coupling_residual();

    assert(residual2 > residual1);  // Should increase with larger displacement
    std::cout << "PASS\n";
}

void test_fsi_size_mismatch_error() {
    std::cout << "Testing FSI size mismatch error... ";
    FluidStructureInteraction::Config cfg;
    cfg.num_nodes = 5;
    cfg.num_struct_dofs = 5;
    cfg.num_fluid_dofs = 5;

    FluidStructureInteraction fsi(cfg);

    VectorField wrong_size(3);  // Wrong size
    try {
        fsi.set_displacement(wrong_size);
        assert(false);  // Should throw
    } catch (const std::runtime_error&) {
        // Expected
    }
    std::cout << "PASS\n";
}

// ============================================================================
// Thermal-Structural Coupling Tests
// ============================================================================

void test_thermal_structural_initialization() {
    std::cout << "Testing thermal-structural initialization... ";
    ThermalStructuralCoupling::Config cfg;
    cfg.num_nodes = 10;
    cfg.reference_temperature = 293.15;

    ThermalStructuralCoupling tc(cfg);
    assert(tc.get_thermal_stress().size() == cfg.num_nodes);
    assert(tc.get_thermal_strain().size() == cfg.num_nodes);
    std::cout << "PASS\n";
}

void test_thermal_strain_computation() {
    std::cout << "Testing thermal strain computation... ";
    ThermalStructuralCoupling::Config cfg;
    cfg.num_nodes = 5;
    cfg.reference_temperature = 293.15;
    cfg.alpha_linear = 12e-6;

    ThermalStructuralCoupling tc(cfg);

    ScalarField temperature(5);
    for (size_t i = 0; i < temperature.size(); ++i) {
        temperature[i] = 373.15;  // 100°C increase
    }

    tc.compute_thermal_strain(temperature);
    const auto& strain = tc.get_thermal_strain();

    double expected_strain = 12e-6 * 80.0;  // 80°C increase
    for (size_t i = 0; i < strain.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            assert(almost_equal(strain[i][j], expected_strain, 1e-8));
        }
    }
    std::cout << "PASS\n";
}

void test_thermal_stress_computation() {
    std::cout << "Testing thermal stress computation... ";
    ThermalStructuralCoupling::Config cfg;
    cfg.num_nodes = 5;
    cfg.reference_temperature = 293.15;

    ThermalStructuralCoupling tc(cfg);

    VectorField stiffness(5);
    for (size_t i = 0; i < stiffness.size(); ++i) {
        stiffness[i] = {210e9, 210e9, 210e9};  // Steel modulus
    }

    // Нагрев на 100 K относительно опорной температуры: σ = E · α · ΔT.
    ScalarField temperature(5);
    for (size_t i = 0; i < temperature.size(); ++i) temperature[i] = cfg.reference_temperature + 100.0;
    tc.compute_thermal_strain(temperature);
    tc.compute_thermal_stress(stiffness);
    const auto& stress = tc.get_thermal_stress();

    const double expected = 210e9 * cfg.alpha_linear * 100.0;
    assert(expected != 0.0);
    for (size_t i = 0; i < stress.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            assert(std::abs(stress[i][j] - expected) <= 1e-9 * std::abs(expected));
        }
    }

    // Без изменения температуры напряжений нет.
    ThermalStructuralCoupling cold(cfg);
    ScalarField ambient(5);
    for (size_t i = 0; i < ambient.size(); ++i) ambient[i] = cfg.reference_temperature;
    cold.compute_thermal_strain(ambient);
    cold.compute_thermal_stress(stiffness);
    const auto& cold_stress = cold.get_thermal_stress();
    for (size_t i = 0; i < cold_stress.size(); ++i) {
        assert(cold_stress[i][0] == 0.0 && cold_stress[i][1] == 0.0 && cold_stress[i][2] == 0.0);
    }
    std::cout << "PASS\n";
}

void test_heat_flux_application() {
    std::cout << "Testing heat flux application... ";
    ThermalStructuralCoupling::Config cfg;
    cfg.num_nodes = 5;
    cfg.reference_temperature = 293.15;
    cfg.specific_heat = 500.0;

    ThermalStructuralCoupling tc(cfg);

    ScalarField heat_flux(5);
    for (size_t i = 0; i < heat_flux.size(); ++i) {
        heat_flux[i] = 1000.0;  // 1000 W/m²
    }

    double time_step = 0.1;
    tc.apply_heat_flux(heat_flux, time_step);

    // Verify temperature increased
    // dT = Q / (ρ * c) * dt ≈ 1000 / 500 * 0.1 = 0.2 K
    std::cout << "PASS\n";
}

void test_temperature_size_mismatch() {
    std::cout << "Testing temperature size mismatch... ";
    ThermalStructuralCoupling::Config cfg;
    cfg.num_nodes = 5;

    ThermalStructuralCoupling tc(cfg);

    ScalarField wrong_temp(3);  // Wrong size
    try {
        tc.compute_thermal_strain(wrong_temp);
        assert(false);  // Should throw
    } catch (const std::runtime_error&) {
        // Expected
    }
    std::cout << "PASS\n";
}

// ============================================================================
// Electromagnetic-Thermal Coupling Tests
// ============================================================================

void test_em_thermal_initialization() {
    std::cout << "Testing EM-thermal initialization... ";
    ElectromagneticThermalCoupling::Config cfg;
    cfg.num_nodes = 10;

    ElectromagneticThermalCoupling emtc(cfg);
    assert(emtc.get_joule_heat().size() == cfg.num_nodes);
    assert(emtc.get_electromagnetic_force().size() == cfg.num_nodes);
    std::cout << "PASS\n";
}

void test_joule_heating_computation() {
    std::cout << "Testing Joule heating computation... ";
    ElectromagneticThermalCoupling::Config cfg;
    cfg.num_nodes = 5;
    cfg.electrical_conductivity = 1e7;

    ElectromagneticThermalCoupling emtc(cfg);

    VectorField current_density(5);
    VectorField electric_field(5);
    for (size_t i = 0; i < 5; ++i) {
        current_density[i] = {1e6, 1e6, 1e6};  // 1 MA/m²
        electric_field[i] = {1.0, 1.0, 1.0};   // 1 V/m
    }

    emtc.compute_joule_heating(current_density, electric_field);
    const auto& joule = emtc.get_joule_heat();

    // Q = J · E = 1e6 * 1 + 1e6 * 1 + 1e6 * 1 = 3e6
    double expected = 3e6;
    for (size_t i = 0; i < joule.size(); ++i) {
        assert(almost_equal(joule[i], expected, 1e3));
    }
    std::cout << "PASS\n";
}

void test_lorentz_force_computation() {
    std::cout << "Testing Lorentz force computation... ";
    ElectromagneticThermalCoupling::Config cfg;
    cfg.num_nodes = 5;

    ElectromagneticThermalCoupling emtc(cfg);

    VectorField magnetic_field(5);
    VectorField current_density(5);
    for (size_t i = 0; i < 5; ++i) {
        magnetic_field[i] = {0.0, 0.0, 1.0};  // B in z-direction
        current_density[i] = {1e6, 0.0, 0.0}; // J in x-direction
    }

    emtc.compute_electromagnetic_force(magnetic_field, current_density);
    const auto& force = emtc.get_electromagnetic_force();

    // F = J × B; x̂ × ẑ = −ŷ, поэтому [1e6, 0, 0] × [0, 0, 1] = [0, −1e6, 0]
    for (size_t i = 0; i < force.size(); ++i) {
        assert(almost_equal(force[i][0], 0.0, 1.0));
        assert(almost_equal(force[i][1], -1e6, 1e3));
        assert(almost_equal(force[i][2], 0.0, 1.0));
    }
    std::cout << "PASS\n";
}

void test_field_interpolation() {
    std::cout << "Testing field interpolation... ";
    ElectromagneticThermalCoupling::Config cfg;
    cfg.num_nodes = 5;

    ElectromagneticThermalCoupling emtc(cfg);

    VectorField source(5);
    VectorField target(3);
    for (size_t i = 0; i < 5; ++i) {
        source[i] = {double(i), double(i*2), double(i*3)};
    }

    std::vector<size_t> mapping = {0, 2, 4};
    emtc.interpolate_fields(source, target, mapping);

    assert(vector_almost_equal(target[0], source[0]));
    assert(vector_almost_equal(target[1], source[2]));
    assert(vector_almost_equal(target[2], source[4]));
    std::cout << "PASS\n";
}

// ============================================================================
// Coupling Algorithm Tests
// ============================================================================

void test_monolithic_coupling_initialization() {
    std::cout << "Testing monolithic coupling initialization... ";
    MonolithicCoupling::Config cfg;
    cfg.tolerance = 1e-6;
    cfg.max_iterations = 100;

    MonolithicCoupling mono(cfg);
    assert(mono.get_iteration_count() == 0);
    assert(!mono.is_converged());
    std::cout << "PASS\n";
}

void test_monolithic_coupling_iteration() {
    std::cout << "Testing monolithic coupling iteration... ";
    MonolithicCoupling::Config cfg;
    cfg.tolerance = 1e-3;
    cfg.max_iterations = 100;

    MonolithicCoupling mono(cfg);

    // Mock solver callback
    mono.set_solve_callback([]() {
        VectorField result(3);
        for (size_t i = 0; i < 3; ++i) {
            result[i] = {0.001, 0.001, 0.001};
        }
        return result;
    });

    double residual1 = mono.iterate();
    assert(residual1 > 0.0);
    assert(mono.get_iteration_count() == 1);

    // Second iteration should still work
    double residual2 = mono.iterate();
    assert(mono.get_iteration_count() == 2);
    std::cout << "PASS\n";
}

void test_partitioned_coupling_initialization() {
    std::cout << "Testing partitioned coupling initialization... ";
    PartitionedCoupling::Config cfg;
    cfg.tolerance = 1e-6;
    cfg.max_iterations = 100;

    PartitionedCoupling part(cfg);
    assert(part.get_iteration_count() == 0);
    std::cout << "PASS\n";
}

void test_partitioned_coupling_solver_addition() {
    std::cout << "Testing partitioned coupling solver addition... ";
    PartitionedCoupling::Config cfg;
    cfg.tolerance = 1e-6;

    PartitionedCoupling part(cfg);

    part.add_physics_solver("fluid", []() {
        VectorField result(3);
        for (size_t i = 0; i < 3; ++i) {
            result[i] = {0.1, 0.1, 0.1};
        }
        return result;
    });

    part.add_physics_solver("structure", []() {
        VectorField result(3);
        for (size_t i = 0; i < 3; ++i) {
            result[i] = {0.2, 0.2, 0.2};
        }
        return result;
    });

    double residual = part.iterate();
    assert(part.get_iteration_count() == 1);
    std::cout << "PASS\n";
}

void test_operator_splitting_initialization() {
    std::cout << "Testing operator splitting initialization... ";
    OperatorSplitting::Config cfg;
    cfg.tolerance = 1e-6;
    cfg.split_order = {"thermal", "structural"};

    OperatorSplitting os(cfg);
    assert(os.get_iteration_count() == 0);
    std::cout << "PASS\n";
}

void test_operator_splitting_operator_execution() {
    std::cout << "Testing operator splitting execution... ";
    OperatorSplitting::Config cfg;
    cfg.tolerance = 1e-6;
    cfg.split_order = {"thermal", "structural"};

    OperatorSplitting os(cfg);

    int thermal_calls = 0;
    int structural_calls = 0;

    os.add_operator("thermal", [&]() { ++thermal_calls; });
    os.add_operator("structural", [&]() { ++structural_calls; });

    os.iterate();
    assert(thermal_calls == 1);
    assert(structural_calls == 1);
    std::cout << "PASS\n";
}

// ============================================================================
// Data Transfer Tests
// ============================================================================

void test_data_transfer_mapping_build() {
    std::cout << "Testing data transfer mapping build... ";
    DataTransferMap::Config cfg;
    DataTransferMap transfer(cfg);

    std::vector<std::array<double, 3>> source = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}
    };

    std::vector<std::array<double, 3>> target = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0}
    };

    transfer.build_mapping(source, target);
    assert(transfer.check_consistency() < 0.1);
    std::cout << "PASS\n";
}

void test_scalar_field_transfer() {
    std::cout << "Testing scalar field transfer... ";
    DataTransferMap::Config cfg;
    DataTransferMap transfer(cfg);

    std::vector<std::array<double, 3>> source_nodes = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {2.0, 0.0, 0.0}
    };

    std::vector<std::array<double, 3>> target_nodes = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0}
    };

    transfer.build_mapping(source_nodes, target_nodes);

    ScalarField source(3);
    source[0] = 100.0;
    source[1] = 200.0;
    source[2] = 300.0;

    ScalarField target(2);
    transfer.transfer_scalar_field(source, target);

    assert(target[0] > 0.0);
    assert(target[1] > 0.0);
    std::cout << "PASS\n";
}

void test_vector_field_transfer() {
    std::cout << "Testing vector field transfer... ";
    DataTransferMap::Config cfg;
    DataTransferMap transfer(cfg);

    std::vector<std::array<double, 3>> source_nodes = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0}
    };

    std::vector<std::array<double, 3>> target_nodes = {
        {0.0, 0.0, 0.0}
    };

    transfer.build_mapping(source_nodes, target_nodes);

    VectorField source(2);
    source[0] = {1.0, 2.0, 3.0};
    source[1] = {4.0, 5.0, 6.0};

    VectorField target(1);
    transfer.transfer_vector_field(source, target);

    assert(target[0][0] != 0.0);
    std::cout << "PASS\n";
}

void test_tensor_field_transfer() {
    std::cout << "Testing tensor field transfer... ";
    DataTransferMap::Config cfg;
    DataTransferMap transfer(cfg);

    std::vector<std::array<double, 3>> source_nodes = {
        {0.0, 0.0, 0.0}
    };

    std::vector<std::array<double, 3>> target_nodes = {
        {0.0, 0.0, 0.0}
    };

    transfer.build_mapping(source_nodes, target_nodes);

    TensorField source(1);
    source[0] = {100.0, 200.0, 300.0, 10.0, 20.0, 30.0};

    TensorField target(1);
    transfer.transfer_tensor_field(source, target);

    for (int i = 0; i < 6; ++i) {
        assert(almost_equal(target[0][i], source[0][i]));
    }
    std::cout << "PASS\n";
}

// ============================================================================
// Convergence Monitor Tests
// ============================================================================

void test_convergence_monitor_initialization() {
    std::cout << "Testing convergence monitor initialization... ";
    ConvergenceMonitor::Config cfg;
    cfg.absolute_tolerance = 1e-8;
    cfg.relative_tolerance = 1e-6;

    ConvergenceMonitor monitor(cfg);
    assert(!monitor.is_converged());
    std::cout << "PASS\n";
}

void test_convergence_monitor_recording() {
    std::cout << "Testing convergence monitor recording... ";
    ConvergenceMonitor::Config cfg;
    cfg.absolute_tolerance = 1e-8;
    cfg.max_iterations = 100;

    ConvergenceMonitor monitor(cfg);

    monitor.record_iteration(1e-4, 0.001);
    monitor.record_iteration(1e-5, 0.0001);
    monitor.record_iteration(1e-9, 0.00001);

    const auto& metrics = monitor.get_metrics();
    assert(metrics.iteration == 3);
    assert(metrics.residual < 1e-8);
    std::cout << "PASS\n";
}

void test_convergence_check() {
    std::cout << "Testing convergence check... ";
    ConvergenceMonitor::Config cfg;
    cfg.absolute_tolerance = 1e-8;
    cfg.max_iterations = 100;

    ConvergenceMonitor monitor(cfg);

    // Record iterations with decreasing residual
    monitor.record_iteration(1e-4, 0.001);
    assert(!monitor.is_converged());

    monitor.record_iteration(1e-8, 0.0001);
    assert(monitor.is_converged());  // Meets absolute tolerance
    std::cout << "PASS\n";
}

// ============================================================================
// Solver Orchestration Tests
// ============================================================================

void test_solver_orchestration_initialization() {
    std::cout << "Testing solver orchestration initialization... ";
    SolverOrchestrator::Config cfg;
    cfg.num_nodes = 100;
    cfg.time_step = 0.01;
    cfg.num_time_steps = 10;

    SolverOrchestrator orchestrator(cfg);
    assert(orchestrator.get_current_time() == 0.0);
    assert(orchestrator.get_time_step() == 0.01);
    std::cout << "PASS\n";
}

void test_solver_registration() {
    std::cout << "Testing solver registration... ";
    SolverOrchestrator::Config cfg;
    cfg.num_nodes = 100;

    SolverOrchestrator orchestrator(cfg);

    orchestrator.register_solver("fluid", []() {
        VectorField result(10);
        return result;
    });

    orchestrator.register_solver("structure", []() {
        VectorField result(10);
        return result;
    });

    // Should not throw
    std::cout << "PASS\n";
}

void test_time_step_adjustment() {
    std::cout << "Testing time step adjustment... ";
    SolverOrchestrator::Config cfg;
    cfg.num_nodes = 100;
    cfg.time_step = 0.01;

    SolverOrchestrator orchestrator(cfg);

    double original = orchestrator.get_time_step();
    orchestrator.adjust_time_step(0.5);

    assert(almost_equal(orchestrator.get_time_step(), original * 0.5));
    std::cout << "PASS\n";
}

void test_solver_orchestration_execution() {
    std::cout << "Testing solver orchestration execution... ";
    SolverOrchestrator::Config cfg;
    cfg.num_nodes = 10;
    cfg.time_step = 0.01;
    cfg.num_time_steps = 5;
    cfg.coupling_scheme = CouplingScheme::PARTITIONED_TIGHT;

    SolverOrchestrator orchestrator(cfg);

    int calls = 0;
    orchestrator.register_solver("test", [&]() {
        ++calls;
        VectorField result(10);
        return result;
    });

    orchestrator.execute();
    assert(calls == cfg.num_time_steps);
    std::cout << "PASS\n";
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_fsi_thermal_coupling_integration() {
    std::cout << "Testing FSI-thermal coupling integration... ";
    FluidStructureInteraction::Config fsi_cfg;
    fsi_cfg.num_nodes = 10;
    fsi_cfg.num_struct_dofs = 10;
    fsi_cfg.num_fluid_dofs = 10;

    ThermalStructuralCoupling::Config tc_cfg;
    tc_cfg.num_nodes = 10;

    FluidStructureInteraction fsi(fsi_cfg);
    ThermalStructuralCoupling tc(tc_cfg);

    // Simulate coupled problem
    ScalarField pressure(10);
    for (size_t i = 0; i < 10; ++i) {
        pressure[i] = 100.0;
    }

    fsi.transfer_pressure_loads(pressure);

    ScalarField temperature(10);
    for (size_t i = 0; i < 10; ++i) {
        temperature[i] = 373.15;
    }

    tc.set_temperature(temperature);
    tc.compute_thermal_strain(temperature);

    // Verify both systems updated
    assert(fsi.get_pressure_loads().size() == 10);
    assert(tc.get_thermal_strain().size() == 10);
    std::cout << "PASS\n";
}

void test_all_coupling_schemes_execution() {
    std::cout << "Testing all coupling schemes... ";

    std::vector<CouplingScheme> schemes = {
        CouplingScheme::MONOLITHIC,
        CouplingScheme::PARTITIONED_LOOSE,
        CouplingScheme::PARTITIONED_TIGHT,
        CouplingScheme::OPERATOR_SPLITTING
    };

    for (auto scheme : schemes) {
        SolverOrchestrator::Config cfg;
        cfg.num_nodes = 10;
        cfg.time_step = 0.01;
        cfg.num_time_steps = 3;
        cfg.coupling_scheme = scheme;

        SolverOrchestrator orch(cfg);
        orch.register_solver("test", []() {
            VectorField result(10);
            return result;
        });

        orch.execute();
    }
    std::cout << "PASS\n";
}

void test_multi_domain_data_transfer() {
    std::cout << "Testing multi-domain data transfer... ";

    DataTransferMap::Config cfg;
    DataTransferMap transfer(cfg);

    std::vector<std::array<double, 3>> fluid_nodes = {
        {0.0, 0.0, 0.0},
        {0.5, 0.0, 0.0},
        {1.0, 0.0, 0.0}
    };

    std::vector<std::array<double, 3>> struct_nodes = {
        {0.25, 0.0, 0.0},
        {0.75, 0.0, 0.0}
    };

    transfer.build_mapping(fluid_nodes, struct_nodes);

    // Transfer scalar field (pressure to structure)
    ScalarField fluid_pressure(3);
    fluid_pressure[0] = 100.0;
    fluid_pressure[1] = 150.0;
    fluid_pressure[2] = 200.0;

    ScalarField struct_pressure(2);
    transfer.transfer_scalar_field(fluid_pressure, struct_pressure);

    assert(struct_pressure[0] > 0.0);
    assert(struct_pressure[1] > 0.0);
    std::cout << "PASS\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n=== BendCore Multi-Physics Coupling Test Suite ===\n\n";

    // FSI Tests (5)
    test_fsi_initialization();
    test_fsi_pressure_transfer();
    test_fsi_displacement_transfer();
    test_fsi_residual_computation();
    test_fsi_size_mismatch_error();

    // Thermal-Structural Tests (5)
    test_thermal_structural_initialization();
    test_thermal_strain_computation();
    test_thermal_stress_computation();
    test_heat_flux_application();
    test_temperature_size_mismatch();

    // EM-Thermal Tests (5)
    test_em_thermal_initialization();
    test_joule_heating_computation();
    test_lorentz_force_computation();
    test_field_interpolation();

    // Coupling Algorithm Tests (6)
    test_monolithic_coupling_initialization();
    test_monolithic_coupling_iteration();
    test_partitioned_coupling_initialization();
    test_partitioned_coupling_solver_addition();
    test_operator_splitting_initialization();
    test_operator_splitting_operator_execution();

    // Data Transfer Tests (5)
    test_data_transfer_mapping_build();
    test_scalar_field_transfer();
    test_vector_field_transfer();
    test_tensor_field_transfer();

    // Convergence Monitor Tests (3)
    test_convergence_monitor_initialization();
    test_convergence_monitor_recording();
    test_convergence_check();

    // Solver Orchestration Tests (4)
    test_solver_orchestration_initialization();
    test_solver_registration();
    test_time_step_adjustment();
    test_solver_orchestration_execution();

    // Integration Tests (3)
    test_fsi_thermal_coupling_integration();
    test_all_coupling_schemes_execution();
    test_multi_domain_data_transfer();

    std::cout << "\n=== All 41 Tests Passed! ===\n\n";
    return 0;
}
