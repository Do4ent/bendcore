cmake_minimum_required(VERSION 3.20)
project(bendcore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Детерминизм (спецификация, платформа): без fast-math, без контракции выражений.
if(MSVC)
  add_compile_options(/fp:precise /fp:contract-)
else()
  add_compile_options(-ffp-contract=off -fno-fast-math)
endif()

# WebAssembly (Emscripten): исключения нужны (ядро бросает std::invalid_argument при неверных данных).
if(EMSCRIPTEN)
  add_compile_options(-fexceptions)
  add_link_options(-fexceptions)
endif()

# Eigen (MPL-2.0): системный или stub для облачной среды
find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT TARGET Eigen3::Eigen)
  set(BENDCORE_EIGEN_DIR "" CACHE PATH "Каталог Eigen (внутри папка Eigen/), если нет системного Eigen")
  find_path(BENDCORE_EIGEN_INCLUDE Eigen/Core HINTS ${BENDCORE_EIGEN_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen-5.0.0 ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/eigen-5.0.0 ${CMAKE_CURRENT_SOURCE_DIR}/third_party_stubs NO_CMAKE_FIND_ROOT_PATH)

  # Fallback to stub if not found
  if(NOT BENDCORE_EIGEN_INCLUDE)
    set(BENDCORE_EIGEN_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party_stubs)
    message(STATUS "Using Eigen stub from third_party_stubs/")
  endif()

  add_library(bendcore_eigen INTERFACE)
  target_include_directories(bendcore_eigen SYSTEM INTERFACE ${BENDCORE_EIGEN_INCLUDE})
else()
  add_library(bendcore_eigen INTERFACE)
  target_link_libraries(bendcore_eigen INTERFACE Eigen3::Eigen)
endif()

# OpenSSL (для SHA256 хэширования в ассоциативных чертежах)
find_package(OpenSSL REQUIRED)

# zlib (для Deflate-декомпрессии ZIP в DWFx парсере)
find_package(ZLIB REQUIRED)

# BC-015: LibDWG для экспорта в формат DWG
find_package(LibDWG QUIET)
if(NOT LibDWG_FOUND)
  # Попытка найти через pkg-config
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LibDWG QUIET libdwg)
  endif()
endif()

# OpenMP (Phase H: Advanced Optimization)
# Required for parallel geometry computations and performance optimization
find_package(OpenMP REQUIRED)

# OCCT (Open Cascade Technology) для импорта STEP файлов (Q3 2027, Task 1)
# Windows-only integration with OCCT 7.7+
if(WIN32)
  find_package(OpenCASCADE 7.7 QUIET)
  if(NOT OpenCASCADE_FOUND)
    # Try to find OCCT via vcpkg
    find_package(OpenCASCADE CONFIG QUIET)
  endif()
else()
  set(OpenCASCADE_FOUND FALSE)
endif()

# M1 Core Components (CAD Foundation)
set(BENDCORE_SOURCES
  # Core architecture
  src/entity_id.cpp
  src/cad_document.cpp
  src/entity_tree.cpp
  src/document_event_bus.cpp
  src/command_manager.cpp
  src/transaction.cpp
  src/history_stack.cpp
  src/parameter_store.cpp
  src/feature_system.cpp

  # M2 Phase: Document Management (GAP-017, GAP-018, GAP-019)
  src/document/transaction.cpp
  src/document/undo_redo_manager.cpp
  src/serialization/document_serializer.cpp
  src/api/document_api.cpp

  # Dependencies and Rebuild (M2 Phase)
  src/dependency_graph.cpp
  src/rebuild_engine.cpp

  # I/O and serialization
  src/serializer.cpp
  src/atomic_save.cpp

  # Constraint solver
  src/constraint_system.cpp

  # M2 Phase: Advanced Sketcher (GAP-002, GAP-003, GAP-004)
  src/sketch/constraint_diagnostics.cpp
  src/sketch/primitives.cpp
  src/sketch/edit_tools.cpp

  # M2 Phase: Feature Operations (GAP-006, GAP-007, GAP-008)
  src/geometry/feature_operations.cpp
  src/topology/topological_reference.cpp

  # M5 Phase: Advanced Feature Operations (GAP-020) - Loft, Sweep, Revolve
  src/features/loft.cpp
  src/features/sweep.cpp
  src/features/revolve.cpp
  src/features/feature_system.cpp

  # M5 Phase: Surface Modeling (GAP-021) - TEMPORARILY DISABLED FOR COMPILATION
  # src/surfaces/nurbs_surface.cpp
  # src/surfaces/surface_types.cpp
  # src/surfaces/surface_operations.cpp

  # M3 Phase: Assembly (GAP-009, GAP-010)
  src/instances.cpp
  src/assembly/part_reference.cpp
  src/assembly/assembly.cpp
  src/assembly/component_instance.cpp
  src/assembly/assembly_constraints.cpp
  src/assembly/assembly_constraint_system.cpp
  src/assembly/assembly_validation.cpp

  # M3 Phase: Manufacturing - BOM Generation (GAP-012)
  src/manufacturing/bom.cpp

  # M8 Phase: Manufacturing Constraints (GAP-034)
  src/manufacturing_constraints.cpp

  # M3 Phase: Mass Properties Analysis (GAP-015)
  src/analysis/mass_properties.cpp

  # M7 Phase: Multi-Physics Coupling (GAP-029)
  src/analysis/multi_physics_coupling.cpp

  # M3 Phase: Drawing Extraction (GAP-014)
  src/drawing/drawing_extraction.cpp

  # M4 Phase: Drawing Workbench Foundation (GAP-016)
  # (Compiled separately with test to avoid conflicts)
  # src/drawing/drawing.cpp
  # src/drawing/drawing_manager.cpp

  # M4 Phase: View Management & Layouts (GAP-017)
  src/drawing/view_manager.cpp
  src/drawing/layout_manager.cpp
  src/drawing/view_properties.cpp

  # M4 Phase: Annotation & Dimensioning (GAP-018)
  src/drawing/annotation_dimension.cpp

  # M4 Phase: Drawing Export & Publishing (GAP-019)
  src/drawing/export_options.cpp
  src/drawing/drawing_export.cpp
  src/drawing/publishing_manager.cpp
  src/drawing/print_manager.cpp
  src/drawing/file_handler.cpp

  # M5 Phase: Direct Editing (GAP-022)
  src/editing/direct_editing_manager.cpp

  # M5 Phase: Performance Optimization and Caching (GAP-023)
  src/optimization/geometry_cache.cpp
  src/optimization/computation_cache.cpp
  src/optimization/parallel_executor.cpp
  src/optimization/memory_management.cpp
  src/optimization/performance_monitor.cpp
  src/optimization/optimization_profiles.cpp
  src/optimization/memory_optimizer.cpp
  src/optimization/performance_tuning.cpp

  # M6 Phase: Parametric AI (GAP-027)
  src/parametric_ai/design_learning.cpp
  src/parametric_ai/regression_engine.cpp
  src/parametric_ai/classification_system.cpp
  src/parametric_ai/prediction_system.cpp
  src/parametric_ai/learning_pipeline.cpp
  src/parametric_ai/design_advice.cpp

  # M6 Phase: Simulation Engine (GAP-024)
  src/simulation/cfd_solver.cpp
  src/simulation/structural_solver.cpp
  src/simulation/thermal_solver.cpp
  src/simulation/simulation_manager.cpp
  src/simulation/result_visualization.cpp

  # M6 Phase: FEA Integration (GAP-025)
  src/fea/mesh_generator.cpp
  src/fea/element_library.cpp
  src/fea/material_database.cpp
  src/fea/boundary_conditions.cpp
  src/fea/fea_solver.cpp
  src/fea/result_processor.cpp

  # M7 Phase: Mesh Optimization (GAP-028) - SKIPPED FOR NOW
  # src/mesh_optimization/quality_analyzer.cpp
  # src/mesh_optimization/adaptive_refinement.cpp
  # src/mesh_optimization/mesh_smoothing.cpp
  # src/mesh_optimization/mesh_optimization.cpp
  # src/mesh_optimization/parallel_mesh_generation.cpp
  # src/mesh_optimization/mesh_io.cpp

  # M6 Phase: Generative Design (GAP-026)
  src/generative/design_space.cpp
  src/generative/genetic_algorithm.cpp
  src/generative/particle_swarm.cpp
  src/generative/constraint_handler.cpp
  src/generative/design_generator.cpp
  src/generative/optimization_manager.cpp
  src/generative/visualization.cpp

  # M7 Phase: Real-time Solver (GAP-030)
  src/solver/real_time_solver.cpp
  src/solver/iterative_solvers.cpp
  src/solver/preconditioners.cpp
  src/solver/reduced_order_model.cpp
  src/solver/solver_scheduler.cpp
  src/solver/adaptive_refinement.cpp
  src/solver/performance_metrics.cpp

  # M7 Phase: Cloud & Collaboration (GAP-031)
  src/collaboration.cpp
  src/cloud/cloud_storage_manager.cpp
  src/cloud/realtime_collaboration.cpp
  src/cloud/rest_api_handler.cpp
  src/cloud/serialization.cpp
  src/cloud/user_management.cpp
  src/cloud/analytics_monitoring.cpp
  src/cloud/cloud_manager.cpp

  # M8 Phase: Material Library (GAP-033)
  src/materials/material_library.cpp

  # M8 Phase: Cost Analysis (GAP-035)
  src/cost_analysis.cpp

  # M8 Phase: Topology Optimization (GAP-032) - TEMPORARILY DISABLED DUE TO COMPILER ERRORS
  # src/topology_optimization/simp_method.cpp
  # src/topology_optimization/sensitivity_analysis.cpp
  # src/topology_optimization/density_filtering.cpp
  # src/topology_optimization/optimization_algorithms.cpp
  # src/topology_optimization/constraint_handling.cpp
  # src/topology_optimization/post_processing.cpp
  # src/topology_optimization/topology_optimizer.cpp

  # M17 Phase: Advanced Analytics (GAP-074 to GAP-078)
  src/analytics/design_analytics.cpp
  src/analytics/performance_monitoring.cpp
  src/analytics/usage_tracking.cpp
  src/analytics/report_generator.cpp
  src/analytics/data_visualization.cpp
)

add_library(bendcore_core STATIC
  ${BENDCORE_SOURCES}
)
target_include_directories(bendcore_core PUBLIC include third_party third_party_stubs)  # third_party: nlohmann/json.hpp (MIT), third_party_stubs: Eigen/Dense (fallback)
target_link_libraries(bendcore_core PRIVATE bendcore_eigen OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB OpenMP::OpenMP_CXX)

# BC-015: Link LibDWG if available
if(LibDWG_FOUND)
  target_include_directories(bendcore_core SYSTEM PRIVATE ${LibDWG_INCLUDE_DIRS})
  target_link_libraries(bendcore_core PRIVATE ${LibDWG_LIBRARIES})
  target_compile_definitions(bendcore_core PRIVATE BENDCORE_WITH_LIBDWG=1)
endif()

# Link OCCT libraries if available
if(OpenCASCADE_FOUND)
  target_include_directories(bendcore_core SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
  target_link_libraries(bendcore_core PRIVATE ${OpenCASCADE_LIBRARIES})
  target_compile_definitions(bendcore_core PRIVATE BENDCORE_WITH_OCCT=1)
endif()

# M1 Integration Test (CAD Foundation)
add_executable(test_m1_integration tests/integration/test_m1_integration.cpp)
target_link_libraries(test_m1_integration PRIVATE bendcore_core)

enable_testing()

# Тест: нативно — запуск исполняемого файла; WebAssembly — запуск через node (код возврата = результат теста).
find_program(BENDCORE_NODE node)
function(bendcore_add_test name target)
  if(EMSCRIPTEN)

    if(NOT BENDCORE_NODE)
      message(FATAL_ERROR "Для тестов WebAssembly нужен node в PATH (BENDCORE_NODE)")
    endif()
    target_link_options(${target} PRIVATE -sEXIT_RUNTIME=1)
    add_test(NAME ${name} COMMAND ${BENDCORE_NODE} $<TARGET_FILE:${target}>)
  else()
    add_test(NAME ${name} COMMAND ${target})
  endif()
endfunction()

# M1 Integration Test
bendcore_add_test(m1_integration test_m1_integration)

add_executable(test_bend_output tests/test_bend_output.cpp)
target_link_libraries(test_bend_output PRIVATE bendcore_core)
bendcore_add_test(bend_output test_bend_output)

add_executable(test_batch_check tests/test_batch_check.cpp)
target_link_libraries(test_batch_check PRIVATE bendcore_core)
bendcore_add_test(batch_check test_batch_check)

add_executable(test_instances tests/test_instances.cpp)
target_link_libraries(test_instances PRIVATE bendcore_core)
bendcore_add_test(instances test_instances)

add_executable(test_part_reference tests/assembly/test_part_reference.cpp)
target_link_libraries(test_part_reference PRIVATE bendcore_core)
bendcore_add_test(part_reference test_part_reference)

# M3 Phase: Assembly Constraints (GAP-011)
add_executable(test_assembly_constraints tests/assembly/test_assembly_constraints.cpp)
target_link_libraries(test_assembly_constraints PRIVATE bendcore_core)
bendcore_add_test(assembly_constraints test_assembly_constraints)

add_executable(test_entity_id tests/document/test_entity_id.cpp)
target_link_libraries(test_entity_id PRIVATE bendcore_core)
bendcore_add_test(entity_id test_entity_id)

add_executable(test_cad_document tests/document/test_cad_document.cpp)
target_link_libraries(test_cad_document PRIVATE bendcore_core)
bendcore_add_test(cad_document test_cad_document)

add_executable(test_hierarchy_extended tests/document/test_hierarchy_extended.cpp)
target_link_libraries(test_hierarchy_extended PRIVATE bendcore_core)
bendcore_add_test(hierarchy_extended test_hierarchy_extended)

# M2 Phase: Document Management (GAP-017, GAP-018, GAP-019)
add_executable(test_transaction tests/document/test_transaction.cpp)
target_link_libraries(test_transaction PRIVATE bendcore_core)
bendcore_add_test(transaction test_transaction)

add_executable(test_undo_redo_manager tests/document/test_undo_redo_manager.cpp)
target_link_libraries(test_undo_redo_manager PRIVATE bendcore_core)
bendcore_add_test(undo_redo_manager test_undo_redo_manager)

add_executable(test_document_serializer tests/serialization/test_document_serializer.cpp)
target_link_libraries(test_document_serializer PRIVATE bendcore_core)
bendcore_add_test(document_serializer test_document_serializer)

add_executable(test_document_api tests/api/test_document_api.cpp)
target_link_libraries(test_document_api PRIVATE bendcore_core)
bendcore_add_test(document_api test_document_api)

add_executable(test_command_executor tests/commands/test_command_executor.cpp)
target_link_libraries(test_command_executor PRIVATE bendcore_core)
bendcore_add_test(command_executor test_command_executor)

add_executable(test_transaction_manager tests/commands/test_transaction_manager.cpp)
target_link_libraries(test_transaction_manager PRIVATE bendcore_core)
bendcore_add_test(transaction_manager test_transaction_manager)

add_executable(test_history_stack tests/commands/test_history_stack.cpp)
target_link_libraries(test_history_stack PRIVATE bendcore_core)
bendcore_add_test(history_stack test_history_stack)

add_executable(test_document_event_bus tests/commands/test_document_event_bus.cpp)
target_link_libraries(test_document_event_bus PRIVATE bendcore_core)
bendcore_add_test(document_event_bus test_document_event_bus)

# M4 Phase: View Management & Layouts (GAP-017)
add_executable(test_view_management tests/drawing/test_view_management.cpp)
target_link_libraries(test_view_management PRIVATE bendcore_core)
bendcore_add_test(view_management test_view_management)

add_executable(test_drawing tests/test_drawing.cpp)
target_link_libraries(test_drawing PRIVATE bendcore_core)
bendcore_add_test(drawing test_drawing)

add_executable(test_dwg_export tests/test_dwg_export.cpp)
target_link_libraries(test_dwg_export PRIVATE bendcore_core)
bendcore_add_test(dwg_export test_dwg_export)

add_executable(dwg_export_e2e tests/dwg_export.e2e.cpp)
target_link_libraries(dwg_export_e2e PRIVATE bendcore_core)
bendcore_add_test(dwg_export_e2e dwg_export_e2e)

add_executable(e2e_ui_core tests/e2e_ui_core_test.cpp)
target_link_libraries(e2e_ui_core PRIVATE bendcore_core)
bendcore_add_test(e2e_ui_core e2e_ui_core)

add_executable(test_core_api tests/test_core_api.cpp)
target_link_libraries(test_core_api PRIVATE bendcore_core)
bendcore_add_test(core_api test_core_api)

# M4 Phase: Drawing Export & Publishing (GAP-019)
# Tests use Google Test - only build if GTest is available
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_drawing_export_m4 tests/test_drawing_export_m4.cpp)
  target_link_libraries(test_drawing_export_m4 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(drawing_export_m4 test_drawing_export_m4)

  add_executable(test_publishing_manager_m4 tests/test_publishing_manager_m4.cpp)
  target_link_libraries(test_publishing_manager_m4 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(publishing_manager_m4 test_publishing_manager_m4)

  add_executable(test_print_manager_m4 tests/test_print_manager_m4.cpp)
  target_link_libraries(test_print_manager_m4 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(print_manager_m4 test_print_manager_m4)

  add_executable(test_file_handler_m4 tests/test_file_handler_m4.cpp)
  target_link_libraries(test_file_handler_m4 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(file_handler_m4 test_file_handler_m4)
else()
  message(STATUS "GTest not found - M4 Phase tests skipped")
endif()

# Ядро отдельным процессом для Electron (строки JSON через stdin/stdout).
add_executable(bendcore_cli src/cli/main.cpp)
target_link_libraries(bendcore_cli PRIVATE bendcore_core)

add_executable(test_tubegeom tests/test_tubegeom.cpp)
target_link_libraries(test_tubegeom PRIVATE bendcore_core)
bendcore_add_test(tubegeom test_tubegeom)

add_executable(test_blank_comp tests/test_blank_comp.cpp)
target_link_libraries(test_blank_comp PRIVATE bendcore_core)
bendcore_add_test(blank_comp test_blank_comp)

add_executable(test_calib tests/test_calib.cpp)
target_link_libraries(test_calib PRIVATE bendcore_core)
bendcore_add_test(calib test_calib)

add_executable(test_ports tests/test_ports.cpp)
target_link_libraries(test_ports PRIVATE bendcore_core)
bendcore_add_test(ports test_ports)

add_executable(test_refs tests/test_refs.cpp)
target_link_libraries(test_refs PRIVATE bendcore_core)
bendcore_add_test(refs test_refs)

add_executable(test_import tests/test_import.cpp)
target_link_libraries(test_import PRIVATE bendcore_core)
target_compile_definitions(test_import PRIVATE BENDCORE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples")
bendcore_add_test(import test_import)

add_executable(test_axis tests/test_axis.cpp)
target_link_libraries(test_axis PRIVATE bendcore_core)
bendcore_add_test(axis test_axis)

add_executable(test_analysis tests/test_analysis.cpp)
target_link_libraries(test_analysis PRIVATE bendcore_core)
bendcore_add_test(analysis test_analysis)

# M7 Phase: Multi-Physics Coupling (GAP-029)
add_executable(test_multi_physics_coupling tests/test_multi_physics_coupling.cpp)
target_link_libraries(test_multi_physics_coupling PRIVATE bendcore_core)
bendcore_add_test(multi_physics_coupling test_multi_physics_coupling)

# M7 Phase: Real-time Solver (GAP-030)
add_executable(test_real_time_solver tests/solver/test_real_time_solver.cpp)
target_link_libraries(test_real_time_solver PRIVATE bendcore_core)
bendcore_add_test(real_time_solver test_real_time_solver)

add_executable(test_visualization tests/test_visualization.cpp)
target_link_libraries(test_visualization PRIVATE bendcore_core)
bendcore_add_test(visualization test_visualization)

add_executable(test_collaboration tests/test_collaboration.cpp)
target_link_libraries(test_collaboration PRIVATE bendcore_core)
bendcore_add_test(collaboration test_collaboration)

# M7 Phase: Cloud & Collaboration (GAP-031)
add_executable(test_cloud_collaboration tests/test_cloud_collaboration.cpp)
target_link_libraries(test_cloud_collaboration PRIVATE bendcore_core)
bendcore_add_test(cloud_collaboration test_cloud_collaboration)

add_executable(test_standards tests/test_standards.cpp)
target_link_libraries(test_standards PRIVATE bendcore_core)
bendcore_add_test(standards test_standards)

# M8 Phase: Topology Optimization (GAP-032)
add_executable(test_topology_optimization_m8 tests/test_topology_optimization_m8.cpp)
target_link_libraries(test_topology_optimization_m8 PRIVATE bendcore_core)
bendcore_add_test(topology_optimization_m8 test_topology_optimization_m8)

# M8 Phase: Cost Analysis (GAP-035)
add_executable(test_simple_cost_analysis tests/simple_cost_analysis.cpp)
target_link_libraries(test_simple_cost_analysis PRIVATE bendcore_core)
bendcore_add_test(simple_cost_analysis test_simple_cost_analysis)

find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_cost_analysis tests/cost_analysis.test.cpp)
  target_link_libraries(test_cost_analysis PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(cost_analysis test_cost_analysis)
else()
  message(STATUS "GTest not found - Comprehensive Cost Analysis tests skipped (using simple test)")
endif()

# M3 Phase: Assembly Creation (GAP-009)
add_executable(test_assembly_creation tests/assembly/test_assembly_creation.cpp)
target_link_libraries(test_assembly_creation PRIVATE bendcore_core)
bendcore_add_test(assembly_creation test_assembly_creation)

# M3 Phase: Assembly Validation (GAP-013)
add_executable(test_assembly_validation tests/assembly/test_assembly_validation.cpp)
target_link_libraries(test_assembly_validation PRIVATE bendcore_core)
bendcore_add_test(assembly_validation test_assembly_validation)

add_executable(test_reporting tests/test_reporting.cpp)
target_link_libraries(test_reporting PRIVATE bendcore_core)
bendcore_add_test(reporting test_reporting)

add_executable(test_reporting_ui tests/test_reporting_ui.cpp)
target_link_libraries(test_reporting_ui PRIVATE bendcore_core)
bendcore_add_test(reporting_ui test_reporting_ui)

# M5 Phase: Surface Modeling (GAP-021)
add_executable(test_surfaces_m5 tests/surfaces/test_surfaces_m5.cpp)
target_link_libraries(test_surfaces_m5 PRIVATE bendcore_core)
bendcore_add_test(surfaces_m5 test_surfaces_m5)

add_library(bendcore_formula STATIC src/formula.cpp)
target_include_directories(bendcore_formula PUBLIC include third_party)

# Disabled - uses deprecated AST node types
#add_executable(test_formulas tests/test_formulas.cpp)
#target_link_libraries(test_formulas PRIVATE bendcore_formula)
#bendcore_add_test(formulas test_formulas)

# Template tests (GoogleTest)
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_templates tests/test_templates.cpp)
  target_link_libraries(test_templates PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(templates test_templates)
endif()

# test_performance написан на GoogleTest: собирается, только если GTest установлен.
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_performance tests/test_performance.cpp)
  target_link_libraries(test_performance PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(performance test_performance)

  # Offset modules performance benchmarks
  add_executable(test_offset_performance tests/test_offset_performance.cpp)
  target_link_libraries(test_offset_performance PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(offset_performance test_offset_performance)

  # M2 Phase: Advanced Sketcher (GAP-002, GAP-003, GAP-004)
  add_executable(test_sketch_m2 tests/sketch/sketch_m2_tests.cpp)
  target_link_libraries(test_sketch_m2 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(sketch_m2 test_sketch_m2)

  # M6 Phase: Parametric AI (GAP-027)
  add_executable(test_parametric_ai_m6 tests/parametric_ai/test_parametric_ai_m6.cpp)
  target_link_libraries(test_parametric_ai_m6 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(parametric_ai_m6 test_parametric_ai_m6)
endif()

# PlaneGCS (LGPL-2.1-or-later, извлечён из FreeCAD Sketcher, см. third_party/planegcs/NOTICE.md) — 2D-эскизы.
# TEMPORARILY DISABLED for Phase 4 - Eigen stub implementation insufficient for planegcs dependencies
# Will be re-enabled with full Eigen library or updated stub
#set(PGCS_DIR third_party/planegcs)
#if(EMSCRIPTEN)
#  set(PGCS_LIB_TYPE STATIC)
#else()
#  set(PGCS_LIB_TYPE SHARED)
#endif()
#add_library(planegcs ${PGCS_LIB_TYPE}
#  ${PGCS_DIR}/Geo.cpp ${PGCS_DIR}/Constraints.cpp ${PGCS_DIR}/SubSystem.cpp
#  ${PGCS_DIR}/GCS.cpp ${PGCS_DIR}/qp_eq.cpp)
#target_include_directories(planegcs PUBLIC ${PGCS_DIR})
#target_link_libraries(planegcs PUBLIC bendcore_eigen)
#set_target_properties(planegcs PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
#if(MSVC)
#  target_compile_options(planegcs PRIVATE /fp:precise /fp:contract-)
#else()
#  target_compile_options(planegcs PRIVATE -ffp-contract=off -fno-fast-math -Wall -Wextra)
#endif()

#add_executable(test_planegcs tests/test_planegcs.cpp)
#target_link_libraries(test_planegcs PRIVATE planegcs)
#bendcore_add_test(planegcs test_planegcs)

# 2D-эскиз чертежа (7.9) поверх PlaneGCS: отдельная библиотека, ядро без LGPL-зависимости.
# TEMPORARILY DISABLED - depends on planegcs
#add_library(bendcore_sketch2d STATIC src/sketch2d.cpp)
#target_include_directories(bendcore_sketch2d PUBLIC include)
#target_link_libraries(bendcore_sketch2d PUBLIC planegcs)

# Disabled - depends on sketch2d/planegcs
#add_executable(test_sketch tests/test_sketch.cpp)
#target_link_libraries(test_sketch PRIVATE bendcore_core)
#bendcore_add_test(sketch test_sketch)

#add_executable(test_sketch2d tests/test_sketch2d.cpp)
#target_link_libraries(test_sketch2d PRIVATE bendcore_sketch2d)
#bendcore_add_test(sketch2d test_sketch2d)

# Комментарии и замечания (Q2 2027, Task 4)
add_executable(test_comments tests/test_comments.cpp)
target_link_libraries(test_comments PRIVATE bendcore_core)
bendcore_add_test(comments test_comments)

# Слои и управление видимостью (Q2 2027, Task 3)
add_executable(test_layers tests/test_layers.cpp)
target_link_libraries(test_layers PRIVATE bendcore_core)
bendcore_add_test(layers test_layers)

# Поиск и фильтрация (Q3 2027, Task 3)
add_executable(test_search tests/test_search.cpp)
target_link_libraries(test_search PRIVATE bendcore_core)
bendcore_add_test(search test_search)

# OCCT initialization test (Q3 2027, Task 1, Subtask 1)
if(WIN32 OR OpenCASCADE_FOUND)
  add_executable(test_occt_init tests/test_occt_init.cpp)
  target_include_directories(test_occt_init PRIVATE include)
  if(OpenCASCADE_FOUND)
    target_include_directories(test_occt_init SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(test_occt_init PRIVATE ${OpenCASCADE_LIBRARIES})
    target_compile_definitions(test_occt_init PRIVATE BENDCORE_WITH_OCCT=1)
  else()
    target_link_libraries(test_occt_init PRIVATE bendcore_core)
  endif()
  bendcore_add_test(occt_init test_occt_init)

  # STEP File Reader test (Q3 2027, Task 1, Subtask 2)
  add_executable(test_step_reader tests/test_step_reader.cpp)
  target_include_directories(test_step_reader PRIVATE include)
  target_link_libraries(test_step_reader PRIVATE bendcore_core)
  if(OpenCASCADE_FOUND)
    target_include_directories(test_step_reader SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIRS})
    target_link_libraries(test_step_reader PRIVATE ${OpenCASCADE_LIBRARIES})
    target_compile_definitions(test_step_reader PRIVATE BENDCORE_WITH_OCCT=1)
  endif()
  bendcore_add_test(step_reader test_step_reader)

  # Geometry Analyzer test (Q3 2027, Task 1, Subtask 3)
  add_executable(test_geometry_analyzer tests/test_geometry_analyzer.cpp)
  target_include_directories(test_geometry_analyzer PRIVATE include)
  target_link_libraries(test_geometry_analyzer PRIVATE bendcore_core)
  bendcore_add_test(geometry_analyzer test_geometry_analyzer)

  # Parametric Builder test (Q3 2027, Task 1, Subtask 4)
  add_executable(test_parametric_builder tests/test_parametric_builder.cpp)
  target_include_directories(test_parametric_builder PRIVATE include)
  target_link_libraries(test_parametric_builder PRIVATE bendcore_core)
  bendcore_add_test(parametric_builder test_parametric_builder)

  # Geometry Cache test (Q3 2027, Task 1, Subtask 5)
  add_executable(test_geometry_cache tests/test_geometry_cache.cpp)
  target_include_directories(test_geometry_cache PRIVATE include)
  target_link_libraries(test_geometry_cache PRIVATE bendcore_core)
  bendcore_add_test(geometry_cache test_geometry_cache)

  # DXF Reader test (Q3 2027, Task 1, Subtask 6)
  add_executable(test_dxf_reader tests/test_dxf_reader.cpp)
  target_include_directories(test_dxf_reader PRIVATE include)
  target_link_libraries(test_dxf_reader PRIVATE bendcore_core)
  bendcore_add_test(dxf_reader test_dxf_reader)

  # Import Workflow test (Q3 2027, Task 1, Subtask 7)
  add_executable(test_import_workflow tests/test_import_workflow.cpp)
  target_include_directories(test_import_workflow PRIVATE include)
  target_link_libraries(test_import_workflow PRIVATE bendcore_core)
  bendcore_add_test(import_workflow test_import_workflow)

  # E2E Import test (Q3 2027, Task 1, Subtask 10)
  add_executable(e2e_import tests/e2e_import.cpp)
  target_include_directories(e2e_import PRIVATE include)
  target_link_libraries(e2e_import PRIVATE bendcore_core)
  bendcore_add_test(e2e_import e2e_import)
endif()

# GrowthBook Analytics (Q3 2027, Task 4)
# Subtask 1: GrowthBook SDK Integration
add_executable(test_growthbook_client tests/test_growthbook_client.cpp)
target_link_libraries(test_growthbook_client PRIVATE bendcore_core)
bendcore_add_test(growthbook_client test_growthbook_client)

# Subtask 2: Analytics Event System
add_executable(test_analytics_event tests/test_analytics_event.cpp)
target_link_libraries(test_analytics_event PRIVATE bendcore_core)
bendcore_add_test(analytics_event test_analytics_event)

# Subtask 3: A/B Testing Framework
add_executable(test_ab_testing tests/test_ab_testing.cpp)
target_link_libraries(test_ab_testing PRIVATE bendcore_core)
bendcore_add_test(ab_testing test_ab_testing)

# Subtask 4: Event Batching & Delivery
add_executable(test_event_batch_sender tests/test_event_batch_sender.cpp)
target_link_libraries(test_event_batch_sender PRIVATE bendcore_core)
bendcore_add_test(event_batch_sender test_event_batch_sender)

# Q4 Task 1: Advanced Drawing Features
add_executable(test_drawing_editor tests/test_drawing_editor.cpp)
target_link_libraries(test_drawing_editor PRIVATE bendcore_core)
bendcore_add_test(drawing_editor test_drawing_editor)

add_executable(test_drawing_layers_manager tests/test_drawing_layers_manager.cpp)
target_link_libraries(test_drawing_layers_manager PRIVATE bendcore_core)
bendcore_add_test(drawing_layers_manager test_drawing_layers_manager)

add_executable(test_drawing_undo_redo tests/test_drawing_undo_redo.cpp)
target_link_libraries(test_drawing_undo_redo PRIVATE bendcore_core)
bendcore_add_test(drawing_undo_redo test_drawing_undo_redo)

# M3 Phase: Drawing Extraction (GAP-014)
add_executable(test_drawing_extraction tests/test_drawing_extraction.cpp)
target_link_libraries(test_drawing_extraction PRIVATE bendcore_core)
bendcore_add_test(drawing_extraction test_drawing_extraction)

# M4 Phase: Drawing Workbench Foundation (GAP-016)
add_executable(test_drawing_workbench_m4
  tests/test_drawing_workbench_m4.cpp
  src/drawing/drawing.cpp
  src/drawing/drawing_manager.cpp
)
target_link_libraries(test_drawing_workbench_m4 PRIVATE bendcore_core)
bendcore_add_test(drawing_workbench_m4 test_drawing_workbench_m4)

# M4 Phase: Annotation & Dimensioning (GAP-018)
add_executable(test_annotation_dimension tests/test_annotation_dimension.cpp)
target_link_libraries(test_annotation_dimension PRIVATE bendcore_core)
bendcore_add_test(annotation_dimension test_annotation_dimension)

# Subtask 7: Integration Testing & Documentation
add_executable(e2e_analytics tests/e2e_analytics.cpp)
target_link_libraries(e2e_analytics PRIVATE bendcore_core)
bendcore_add_test(e2e_analytics e2e_analytics)

# Труба из пути как тело (Open CASCADE): опционально, нужен установленный OCCT (см. README).
# Установка OCCT (cmake --install), например /path/to/occt-install. Конфиг OCCT для статической сборки
# содержит нераскрытые переменные, поэтому библиотеки подключаются напрямую из lib/libTK*.a.
set(BENDCORE_OCCT_DIR "" CACHE PATH "Каталог установки Open CASCADE (include/opencascade, lib/libTK*.a)")
option(BENDCORE_WITH_OCCT "Собрать проверку тела трубы на Open CASCADE" OFF)
if(BENDCORE_WITH_OCCT)
  if(NOT BENDCORE_OCCT_DIR)
    message(FATAL_ERROR "BENDCORE_WITH_OCCT=ON: укажите BENDCORE_OCCT_DIR")
  endif()
  file(GLOB BENDCORE_OCCT_LIBS "${BENDCORE_OCCT_DIR}/lib/libTK*.a")
  if(NOT BENDCORE_OCCT_LIBS)
    message(FATAL_ERROR "В ${BENDCORE_OCCT_DIR}/lib нет libTK*.a (только статическая сборка поддержана)")
  endif()
  add_library(bendcore_occt STATIC src/occt/tube_body_occt.cpp)
  target_include_directories(bendcore_occt SYSTEM PRIVATE "${BENDCORE_OCCT_DIR}/include/opencascade")
  target_link_libraries(bendcore_occt PUBLIC bendcore_core
    -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)
  add_executable(test_tube_occt tests/test_tube_occt.cpp)
  target_link_libraries(test_tube_occt PRIVATE bendcore_occt)
  bendcore_add_test(tube_occt test_tube_occt)

  # Сервис тела трубы и HTTP-сервер bendcore_server (без внешних библиотек).
  add_library(bendcore_server_core STATIC src/server/tube_service.cpp)
  target_include_directories(bendcore_server_core PUBLIC include)
  target_link_libraries(bendcore_server_core PUBLIC bendcore_occt)
  add_executable(bendcore_server src/server/main.cpp)
  target_link_libraries(bendcore_server PRIVATE bendcore_server_core)
  add_executable(test_server tests/test_server.cpp)
  target_link_libraries(test_server PRIVATE bendcore_server_core)
  target_compile_definitions(test_server PRIVATE BENDCORE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples")
  bendcore_add_test(server test_server)
endif()

# Phase C: Parameter Store, Dependency Graph, Rebuild Engine
add_executable(test_parameter_store tests/parameters/test_parameter_store.cpp)
target_link_libraries(test_parameter_store PRIVATE bendcore_core)
bendcore_add_test(parameter_store test_parameter_store)

add_executable(test_dependency_graph tests/dependencies/test_dependency_graph.cpp)
target_link_libraries(test_dependency_graph PRIVATE bendcore_core)
bendcore_add_test(dependency_graph test_dependency_graph)

# Disabled - Missing source files
#add_executable(test_rebuild_engine tests/rebuild/test_rebuild_engine.cpp)
#target_link_libraries(test_rebuild_engine PRIVATE bendcore_core)
#bendcore_add_test(rebuild_engine test_rebuild_engine)

# M2 Phase: Rebuild System (GAP-023: Cascading updates with incremental rebuild)
add_executable(test_rebuild_cascade tests/rebuild/test_rebuild_cascade.cpp)
target_link_libraries(test_rebuild_cascade PRIVATE bendcore_core)
bendcore_add_test(rebuild_cascade test_rebuild_cascade)

add_executable(test_phase_c_integration tests/integration/test_phase_c_integration.cpp)
target_link_libraries(test_phase_c_integration PRIVATE bendcore_core)
bendcore_add_test(phase_c_integration test_phase_c_integration)

# Phase D: Geometry & Topology (CAD-CORE-013 to CAD-CORE-016)
add_executable(test_geometry_kernel tests/geometry/test_geometry_kernel.cpp)
target_link_libraries(test_geometry_kernel PRIVATE bendcore_core)
bendcore_add_test(geometry_kernel test_geometry_kernel)

add_executable(test_brep_builder tests/geometry/test_brep_builder.cpp)
target_link_libraries(test_brep_builder PRIVATE bendcore_core)
bendcore_add_test(brep_builder test_brep_builder)

add_executable(test_curve_operations tests/geometry/test_curve_operations.cpp)
target_link_libraries(test_curve_operations PRIVATE bendcore_core)
bendcore_add_test(curve_operations test_curve_operations)

add_executable(test_surface_operations tests/geometry/test_surface_operations.cpp)
target_link_libraries(test_surface_operations PRIVATE bendcore_core)
bendcore_add_test(surface_operations test_surface_operations)

# M2 Phase: Feature Operations (GAP-006, GAP-007, GAP-008)
add_executable(test_m2_feature_operations tests/test_m2_feature_operations.cpp)
target_link_libraries(test_m2_feature_operations PRIVATE bendcore_core)
bendcore_add_test(m2_feature_operations test_m2_feature_operations)

# M5 Phase: Advanced Feature Operations (GAP-020) - Loft, Sweep, Revolve
add_executable(test_advanced_features_m5 tests/features/test_advanced_features_m5.cpp)
target_link_libraries(test_advanced_features_m5 PRIVATE bendcore_core)
bendcore_add_test(advanced_features_m5 test_advanced_features_m5)

add_executable(test_phase_d_integration tests/integration/test_phase_d_integration.cpp)
target_link_libraries(test_phase_d_integration PRIVATE bendcore_core)
bendcore_add_test(phase_d_integration test_phase_d_integration)

# Comprehensive Test Suite for Phase 4 (90%+ coverage target)
add_executable(test_dwfx_parser_comprehensive tests/test_dwfx_parser_comprehensive.cpp)
target_link_libraries(test_dwfx_parser_comprehensive PRIVATE bendcore_core)
bendcore_add_test(dwfx_parser_comprehensive test_dwfx_parser_comprehensive)

add_executable(test_formula_comprehensive tests/test_formula_comprehensive.cpp)
target_link_libraries(test_formula_comprehensive PRIVATE bendcore_formula)
bendcore_add_test(formula_comprehensive test_formula_comprehensive)

add_executable(test_analysis_comprehensive tests/test_analysis_comprehensive.cpp)
target_link_libraries(test_analysis_comprehensive PRIVATE bendcore_core)
bendcore_add_test(analysis_comprehensive test_analysis_comprehensive)

add_executable(test_geometry_analyzer_comprehensive tests/test_geometry_analyzer_comprehensive.cpp)
target_link_libraries(test_geometry_analyzer_comprehensive PRIVATE bendcore_core)
bendcore_add_test(geometry_analyzer_comprehensive test_geometry_analyzer_comprehensive)

add_executable(test_dxf_reader_comprehensive tests/test_dxf_reader_comprehensive.cpp)
target_link_libraries(test_dxf_reader_comprehensive PRIVATE bendcore_core)
bendcore_add_test(dxf_reader_comprehensive test_dxf_reader_comprehensive)

# ============================================================================
# Phase F: Production Launch (CAD-CORE-021 to 024)
# ============================================================================

# CAD-CORE-021: Comprehensive Test Suite
add_executable(phase_f_test_suite src/testing/phase_f_main.cpp)
target_link_libraries(phase_f_test_suite PRIVATE bendcore_core)
bendcore_add_test(phase_f_test_suite phase_f_test_suite)

# CAD-CORE-021: Individual test scenarios
add_executable(test_integration_e2e tests/testing/test_integration_e2e.cpp)
target_link_libraries(test_integration_e2e PRIVATE bendcore_core)
bendcore_add_test(integration_e2e test_integration_e2e)

add_executable(test_stress tests/testing/test_stress.cpp)
target_link_libraries(test_stress PRIVATE bendcore_core)
bendcore_add_test(stress test_stress)

add_executable(test_multi_phase_integration tests/testing/test_multi_phase_integration.cpp)
target_link_libraries(test_multi_phase_integration PRIVATE bendcore_core)
bendcore_add_test(multi_phase_integration test_multi_phase_integration)

# ============================================================================
# Phase I: Integration Testing (CAD-CORE-033 to CAD-CORE-036)
# ============================================================================

# CAD-CORE-033 through CAD-CORE-036: Phase I Integration Testing
add_executable(test_phase_i_main tests/integration/test_phase_i_main.cpp)
bendcore_add_test(phase_i_main test_phase_i_main)

# CAD-CORE-033: E2EWorkflows — полные workflow тесты (Create → Edit → Parameters → Geometry → FEA → Export)
add_executable(test_e2e_workflows tests/integration/test_e2e_workflows.cpp)
target_link_libraries(test_e2e_workflows PRIVATE bendcore_core)
bendcore_add_test(e2e_workflows test_e2e_workflows)

# CAD-CORE-034: CrossPlatformValidation — тесты кроссплатформной совместимости (Windows/Linux/macOS)
add_executable(test_cross_platform_validation tests/integration/test_cross_platform_validation.cpp)
target_link_libraries(test_cross_platform_validation PRIVATE bendcore_core)
bendcore_add_test(cross_platform_validation test_cross_platform_validation)

# CAD-CORE-035: StressTests — тесты производительности (1K, 5K, 10K, 50K элементов)
add_executable(test_stress_tests tests/integration/test_stress_tests.cpp)
target_link_libraries(test_stress_tests PRIVATE bendcore_core)
bendcore_add_test(stress_tests test_stress_tests)

# CAD-CORE-036: RegressionTests — регрессионные тесты всех фаз A-F
add_executable(test_regression_suite tests/integration/test_regression_suite.cpp)
target_link_libraries(test_regression_suite PRIVATE bendcore_core)
bendcore_add_test(regression_suite test_regression_suite)

# Phase H: Advanced Optimization Tests (CAD-CORE-029, 030, 031, 032)
# M5 Phase: Performance Optimization Tests - 30+ unit tests
add_executable(test_m5_optimization src/testing/optimization_test_suite.cpp)
target_link_libraries(test_m5_optimization PRIVATE bendcore_core)
bendcore_add_test(m5_optimization test_m5_optimization)

# ============================================================================
# M6 Phase: FEA Integration (GAP-025)
# ============================================================================

# M6 Phase: FEA Integration - 35+ unit tests
add_executable(test_fea_m6 tests/fea/test_fea_m6.cpp)
target_link_libraries(test_fea_m6 PRIVATE bendcore_core)
bendcore_add_test(fea_m6 test_fea_m6)

# ============================================================================
# M7 Phase: Mesh Optimization (GAP-028) - 40+ unit tests
# ============================================================================

# add_executable(test_mesh_optimization_m7 tests/mesh_optimization/test_mesh_optimization_m7.cpp)
# target_link_libraries(test_mesh_optimization_m7 PRIVATE bendcore_core)
# bendcore_add_test(mesh_optimization_m7 test_mesh_optimization_m7)

# ============================================================================
# Phase J: Manufacturing Integration (CAD-CORE-037 to CAD-CORE-040)
# ============================================================================

# CAD-CORE-037: FEA Integration
add_executable(test_cad_core_037_fea tests/CAD-CORE-037-FEAIntegration.test.cpp)
target_link_libraries(test_cad_core_037_fea PRIVATE bendcore_core)
bendcore_add_test(cad_core_037_fea test_cad_core_037_fea)

# CAD-CORE-038: G-Code Generator
add_executable(test_cad_core_038_gcode tests/CAD-CORE-038-GCodeGenerator.test.cpp)
target_link_libraries(test_cad_core_038_gcode PRIVATE bendcore_core)
bendcore_add_test(cad_core_038_gcode test_cad_core_038_gcode)

# CAD-CORE-039: BOM Generator
add_executable(test_cad_core_039_bom tests/CAD-CORE-039-BOMGenerator.test.cpp)
target_link_libraries(test_cad_core_039_bom PRIVATE bendcore_core)
bendcore_add_test(cad_core_039_bom test_cad_core_039_bom)

# CAD-CORE-040: Manufacturing Workflow
add_executable(test_cad_core_040_workflow tests/CAD-CORE-040-ManufacturingWorkflow.test.cpp)
target_link_libraries(test_cad_core_040_workflow PRIVATE bendcore_core)
bendcore_add_test(cad_core_040_workflow test_cad_core_040_workflow)

# M3 Phase: Manufacturing - BOM Generation (GAP-012)
add_executable(test_bom_simple tests/test_bom_simple.cpp)
target_link_libraries(test_bom_simple PRIVATE bendcore_core)
bendcore_add_test(bom_simple test_bom_simple)

# M3 Phase: Mass Properties (GAP-015)
add_executable(test_mass_properties tests/analysis/test_mass_properties.cpp)
target_link_libraries(test_mass_properties PRIVATE bendcore_core)
bendcore_add_test(mass_properties test_mass_properties)

# M6 Phase: Simulation Engine (GAP-024) - 30+ unit tests
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_simulation_m6 tests/simulation/test_simulation_m6.cpp)
  target_link_libraries(test_simulation_m6 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(simulation_m6 test_simulation_m6)

  # Continue with other GTest tests
  add_executable(test_bom tests/bom.test.cpp)
  target_link_libraries(test_bom PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(bom test_bom)

  # M5 Phase: Direct Editing (GAP-022)
  add_executable(test_direct_editing_manager tests/editing/direct_editing_manager.test.cpp)
  target_link_libraries(test_direct_editing_manager PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(direct_editing_manager test_direct_editing_manager)

  # M6 Phase: Generative Design (GAP-026) - 32+ unit tests
  add_executable(test_generative_m6 tests/generative/test_generative_m6.cpp)
  target_link_libraries(test_generative_m6 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(generative_m6 test_generative_m6)
endif()

# ============================================================================
# Phase K: Deployment & Distribution (CAD-CORE-041 to CAD-CORE-044)
# ============================================================================

# CAD-CORE-041: Production Build Pipeline
add_executable(test_cad_core_041_production_build tests/CAD-CORE-041-ProductionBuild.test.cpp)
target_link_libraries(test_cad_core_041_production_build PRIVATE bendcore_core)
bendcore_add_test(cad_core_041_production_build test_cad_core_041_production_build)

# CAD-CORE-042: Installers (Multi-platform)
add_executable(test_cad_core_042_installers tests/CAD-CORE-042-Installers.test.cpp)
target_link_libraries(test_cad_core_042_installers PRIVATE bendcore_core)
bendcore_add_test(cad_core_042_installers test_cad_core_042_installers)

# CAD-CORE-043: License Management
add_executable(test_cad_core_043_license tests/CAD-CORE-043-LicenseManagement.test.cpp)
target_link_libraries(test_cad_core_043_license PRIVATE bendcore_core)
bendcore_add_test(cad_core_043_license test_cad_core_043_license)

# CAD-CORE-044: Cloud Deployment
add_executable(test_cad_core_044_cloud tests/CAD-CORE-044-CloudDeployment.test.cpp)
target_link_libraries(test_cad_core_044_cloud PRIVATE bendcore_core)
bendcore_add_test(cad_core_044_cloud test_cad_core_044_cloud)

# M8 Phase (GAP-034): Manufacturing Constraints
add_executable(test_manufacturing_constraints tests/CAD-CORE-043-ManufacturingConstraints.test.cpp)
target_link_libraries(test_manufacturing_constraints PRIVATE bendcore_core)
bendcore_add_test(manufacturing_constraints test_manufacturing_constraints)

# M8 Phase: Material Library (GAP-033) - 45+ unit tests
add_executable(test_material_library tests/materials/test_material_library.cpp)
target_link_libraries(test_material_library PRIVATE bendcore_core)
bendcore_add_test(material_library test_material_library)

# M6 Phase: FEA Integration (GAP-025) - 35 comprehensive unit tests
add_executable(test_gap025_fea_comprehensive tests/gap025_fea_comprehensive.cpp)
target_link_libraries(test_gap025_fea_comprehensive PRIVATE bendcore_core)
bendcore_add_test(gap025_fea_comprehensive test_gap025_fea_comprehensive)

# M6 Phase: Generative Design (GAP-026) - 35 comprehensive unit tests
add_executable(test_gap026_generative_comprehensive tests/gap026_generative_comprehensive.cpp)
target_link_libraries(test_gap026_generative_comprehensive PRIVATE bendcore_core)
bendcore_add_test(gap026_generative_comprehensive test_gap026_generative_comprehensive)

# M6 Phase: Parametric AI (GAP-027) - 35 comprehensive unit tests
add_executable(test_gap027_parametric_ai_comprehensive tests/gap027_parametric_ai_comprehensive.cpp)
target_link_libraries(test_gap027_parametric_ai_comprehensive PRIVATE bendcore_core)
bendcore_add_test(gap027_parametric_ai_comprehensive test_gap027_parametric_ai_comprehensive)

# ============================================================================
# M20: Production Hardening (GAP-088 to GAP-093) - 220 tests
# ============================================================================

# GAP-088: ErrorHandling (37 tests)
add_executable(test_gap088_error_handling tests/GAP-088-ErrorHandling.test.cpp src/hardening/error_handling.cpp)
target_link_libraries(test_gap088_error_handling PRIVATE bendcore_core)
target_include_directories(test_gap088_error_handling PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap088_error_handling test_gap088_error_handling)

# GAP-089: CrashRecovery (35 tests)
add_executable(test_gap089_crash_recovery tests/GAP-089-CrashRecovery.test.cpp src/hardening/crash_recovery.cpp)
target_link_libraries(test_gap089_crash_recovery PRIVATE bendcore_core)
target_include_directories(test_gap089_crash_recovery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap089_crash_recovery test_gap089_crash_recovery)

# GAP-090: LoggingMonitoring (34 tests)
add_executable(test_gap090_logging_monitoring tests/GAP-090-LoggingMonitoring.test.cpp src/hardening/logging_monitoring.cpp)
target_link_libraries(test_gap090_logging_monitoring PRIVATE bendcore_core)
target_include_directories(test_gap090_logging_monitoring PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap090_logging_monitoring test_gap090_logging_monitoring)

# GAP-091: PerformanceTuning (36 tests)
add_executable(test_gap091_performance_tuning tests/GAP-091-PerformanceTuning.test.cpp src/hardening/performance_tuning.cpp)
target_link_libraries(test_gap091_performance_tuning PRIVATE bendcore_core)
target_include_directories(test_gap091_performance_tuning PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap091_performance_tuning test_gap091_performance_tuning)

# GAP-092: LoadTesting (38 tests)
add_executable(test_gap092_load_testing tests/GAP-092-LoadTesting.test.cpp src/hardening/load_testing.cpp)
target_link_libraries(test_gap092_load_testing PRIVATE bendcore_core)
target_include_directories(test_gap092_load_testing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap092_load_testing test_gap092_load_testing)

# GAP-093: ProductionDeployment (40 tests)
add_executable(test_gap093_production_deployment tests/GAP-093-ProductionDeployment.test.cpp src/hardening/production_deployment.cpp)
target_link_libraries(test_gap093_production_deployment PRIVATE bendcore_core)
target_include_directories(test_gap093_production_deployment PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(gap093_production_deployment test_gap093_production_deployment)

# ============================================================================
# M17 Phase: Advanced Analytics (GAP-074 to GAP-078) - 170 tests
# ============================================================================

# GAP-074: DesignAnalytics (35 tests) - Metrics collection, pattern analysis
# TODO: Implement analytics_design.test.cpp

# GAP-075: PerformanceMonitoring (33 tests) - System tracking, profiling
# TODO: Implement analytics_performance.test.cpp

# GAP-076: UsageTracking (32 tests) - Event tracking, user behavior
# TODO: Implement analytics_usage.test.cpp

# GAP-077: ReportGeneration (36 tests) - PDF/HTML reports, aggregation
# TODO: Implement analytics_report.test.cpp

# GAP-078: DataVisualization (34 tests) - Charts, dashboards, real-time display
# TODO: Implement analytics_visualization.test.cpp

# ============================================================================
# M19 Phase: Security & Compliance (GAP-083 to GAP-087) - 175 tests
# ============================================================================

# GAP-083: SecurityAudit (36 tests) - Vulnerability scanner, penetration testing
add_executable(test_gap083_security_audit tests/GAP-083-SecurityAudit.test.cpp)
target_link_libraries(test_gap083_security_audit PRIVATE bendcore_core)
bendcore_add_test(gap083_security_audit test_gap083_security_audit)

# GAP-084: EncryptionSupport (38 tests) - AES-256, TLS/SSL, key management
add_executable(test_gap084_encryption_support tests/GAP-084-EncryptionSupport.test.cpp)
target_link_libraries(test_gap084_encryption_support PRIVATE bendcore_core)
bendcore_add_test(gap084_encryption_support test_gap084_encryption_support)

# GAP-085: ComplianceStandards (34 tests) - GDPR, ISO 27001, PCI-DSS
add_executable(test_gap085_compliance_standards tests/GAP-085-ComplianceStandards.test.cpp)
target_link_libraries(test_gap085_compliance_standards PRIVATE bendcore_core)
bendcore_add_test(gap085_compliance_standards test_gap085_compliance_standards)

# GAP-086: AccessLogging (32 tests) - Audit trails, event tracking
add_executable(test_gap086_access_logging tests/GAP-086-AccessLogging.test.cpp)
target_link_libraries(test_gap086_access_logging PRIVATE bendcore_core)
bendcore_add_test(gap086_access_logging test_gap086_access_logging)

# GAP-087: DataPrivacy (35 tests) - Anonymization, consent management
add_executable(test_gap087_data_privacy tests/GAP-087-DataPrivacy.test.cpp)
target_link_libraries(test_gap087_data_privacy PRIVATE bendcore_core)
bendcore_add_test(gap087_data_privacy test_gap087_data_privacy)

# WebAssembly-мост для TypeScript-интерфейса (embind). Только для Emscripten.
# Результат: web/bendcore.js + bendcore.wasm; страница для проверки в браузере — web/index.html.
if(EMSCRIPTEN)
  add_executable(bendcore_wasm src/wasm/bindings.cpp)
  target_link_libraries(bendcore_wasm PRIVATE bendcore_sketch2d bendcore_core)
  set_target_properties(bendcore_wasm PROPERTIES
    OUTPUT_NAME bendcore
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)
  target_link_options(bendcore_wasm PRIVATE
    --bind --no-entry
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcore
    -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sNO_EXIT_RUNTIME=1)
  # Страница и JS-обёртка копируются рядом с модулем (открывать через http-сервер, не file://).
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/web/ DESTINATION ${CMAKE_BINARY_DIR}/web)
endif()
