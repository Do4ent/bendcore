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

# Eigen (MPL-2.0, header-only) — только настоящий Eigen >= 3.3. Порядок поиска:
#   1. каталог BENDCORE_EIGEN_DIR (переменная CMake или окружения; внутри — папка Eigen/);
#   2. пакет Eigen3 (find_package; при кросс-компиляции Emscripten системный пакет не виден);
#   3. системные каталоги заголовков /usr/include/eigen3, /usr/local/include/eigen3,
#      /opt/homebrew/include/eigen3 (заголовки не зависят от платформы, годятся и для WASM);
#   4. при BENDCORE_FETCH_EIGEN=ON — архив Eigen 3.4.0 с gitlab.com с проверкой SHA-256.
# Если ничего не найдено — ошибка конфигурации (заглушки Eigen в проекте нет).
set(BENDCORE_EIGEN_DIR "$ENV{BENDCORE_EIGEN_DIR}" CACHE PATH "Каталог Eigen (внутри папка Eigen/)")
option(BENDCORE_FETCH_EIGEN "Загрузить Eigen 3.4.0 (MPL-2.0) с gitlab.com, если он не найден" OFF)
# Прежняя переменная кэша могла указывать на удалённую заглушку third_party/eigen-5.0.0.
unset(BENDCORE_EIGEN_INCLUDE CACHE)

# Версия Eigen по заголовкам каталога dir (Eigen 3.x: Macros.h; Eigen 5: Eigen/Version).
function(bendcore_eigen_version dir out)
  set(_ver "")
  foreach(_f "${dir}/Eigen/Version" "${dir}/Eigen/src/Core/util/Macros.h")
    if(EXISTS "${_f}" AND _ver STREQUAL "")
      file(STRINGS "${_f}" _lines REGEX "^#define EIGEN_(WORLD|MAJOR|MINOR)_VERSION[ \t]+[0-9]+")
      if(_lines)
        string(REGEX REPLACE ".*EIGEN_WORLD_VERSION[ \t]+([0-9]+).*" "\\1" _w "${_lines}")
        string(REGEX REPLACE ".*EIGEN_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _ma "${_lines}")
        string(REGEX REPLACE ".*EIGEN_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _mi "${_lines}")
        set(_ver "${_w}.${_ma}.${_mi}")
      endif()
    endif()
  endforeach()
  set(${out} "${_ver}" PARENT_SCOPE)
endfunction()

set(_bendcore_eigen_inc "")
if(BENDCORE_EIGEN_DIR)
  bendcore_eigen_version("${BENDCORE_EIGEN_DIR}" _bendcore_eigen_ver)
  if(NOT EXISTS "${BENDCORE_EIGEN_DIR}/Eigen/Dense" OR _bendcore_eigen_ver VERSION_LESS 3.3)
    message(FATAL_ERROR "BENDCORE_EIGEN_DIR='${BENDCORE_EIGEN_DIR}' is not an Eigen >= 3.3 include "
                        "directory (expected Eigen/Dense and a version header, found version "
                        "'${_bendcore_eigen_ver}').")
  endif()
  set(_bendcore_eigen_inc "${BENDCORE_EIGEN_DIR}")
else()
  find_package(Eigen3 3.3 QUIET NO_MODULE)
endif()

if(NOT _bendcore_eigen_inc AND TARGET Eigen3::Eigen)
  add_library(bendcore_eigen INTERFACE)
  target_link_libraries(bendcore_eigen INTERFACE Eigen3::Eigen)
  message(STATUS "Eigen ${Eigen3_VERSION}: package Eigen3 (${Eigen3_DIR})")
else()
  if(NOT _bendcore_eigen_inc)
    foreach(_d /usr/include/eigen3 /usr/local/include/eigen3 /opt/homebrew/include/eigen3)
      bendcore_eigen_version("${_d}" _bendcore_eigen_ver)
      if(NOT _bendcore_eigen_inc AND EXISTS "${_d}/Eigen/Dense" AND _bendcore_eigen_ver VERSION_GREATER_EQUAL 3.3)
        set(_bendcore_eigen_inc "${_d}")
      endif()
    endforeach()
  endif()
  if(NOT _bendcore_eigen_inc AND BENDCORE_FETCH_EIGEN)
    include(FetchContent)
    # Только загрузка заголовков: CMakeLists.txt Eigen не подключается (SOURCE_SUBDIR).
    FetchContent_Declare(bendcore_eigen_src
      URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
      URL_HASH SHA256=8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72
      SOURCE_SUBDIR bendcore-headers-only)
    FetchContent_MakeAvailable(bendcore_eigen_src)
    set(_bendcore_eigen_inc "${bendcore_eigen_src_SOURCE_DIR}")
  endif()
  if(NOT _bendcore_eigen_inc)
    message(FATAL_ERROR "Eigen >= 3.3 not found. Install it (apt install libeigen3-dev / "
                        "brew install eigen), pass -DBENDCORE_EIGEN_DIR=<dir containing Eigen/>, "
                        "or configure with -DBENDCORE_FETCH_EIGEN=ON to download Eigen 3.4.0.")
  endif()
  bendcore_eigen_version("${_bendcore_eigen_inc}" _bendcore_eigen_ver)
  message(STATUS "Eigen ${_bendcore_eigen_ver}: headers in ${_bendcore_eigen_inc}")
  add_library(bendcore_eigen INTERFACE)
  target_include_directories(bendcore_eigen SYSTEM INTERFACE "${_bendcore_eigen_inc}")
endif()

# OpenSSL: ядру не нужен (SHA-256 чертежей — своя реализация в drawing.cpp). Ищется для модулей вне ядра.
find_package(OpenSSL QUIET)

# zlib (сжатие истории и сериализации документа).
# Для WASM: zlib не требуется (не используется в bindings.cpp; только dwfx_parser,
# который не экспортируется в WASM). Используем пустую INTERFACE library для совместимости.
if(EMSCRIPTEN)
  add_library(bendcore_zlib INTERFACE)
else()
  find_package(ZLIB REQUIRED)
  add_library(bendcore_zlib INTERFACE)
  target_link_libraries(bendcore_zlib INTERFACE ZLIB::ZLIB)
endif()

# BC-015: LibDWG для экспорта в формат DWG
find_package(LibDWG QUIET)
if(NOT LibDWG_FOUND)
  # Попытка найти через pkg-config
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LibDWG QUIET libdwg)
  endif()
endif()

# OpenMP: только для parallel_geometry (вне ядра); необязателен.
if(NOT EMSCRIPTEN)
  find_package(OpenMP QUIET)
endif()

# OCCT (Open Cascade Technology) для импорта STEP файлов (Q3 2027, Task 1)
# Find OCCT on all platforms (Windows, Linux, macOS)
# Default install: the one with OCCT's Release checks (third_party/build_occt.sh and
# build_occt_wasm.sh with OCCT_RELEASE_CHECKS=1, docs/kernel/OCCT_BUILD.md, раздел 6), when it
# exists next to the sources.
if(EMSCRIPTEN)
  set(_bendcore_occt_default_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/occt-install-wasm-checked")
else()
  set(_bendcore_occt_default_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/occt-install-checked")
endif()
if(NOT EXISTS "${_bendcore_occt_default_dir}/include/opencascade/Standard_Version.hxx")
  set(_bendcore_occt_default_dir "")
endif()
set(BENDCORE_OCCT_DIR "${_bendcore_occt_default_dir}" CACHE PATH
  "OCCT install directory (build with: third_party/build_occt.sh)")
option(BENDCORE_OCCT_REQUIRE_CHECKS
  "Refuse an OCCT install built with -DNo_Exception (OCCT index/argument checks compiled out)" ON)

# Whether an OCCT install was built with -DNo_Exception (BUILD_RELEASE_DISABLE_EXCEPTIONS=ON):
# OCCT's installed CMake config records the Release flags. Without the checks a corrupted
# BRep/STEP file reads out of bounds inside OCCT instead of raising Standard_OutOfRange.
function(bendcore_check_occt_release_checks dir)
  set(_flags_file "${dir}/lib/cmake/opencascade/OpenCASCADECompileDefinitionsAndFlags-release.cmake")
  if(NOT EXISTS "${_flags_file}")
    message(STATUS "OCCT Release checks: unknown (${_flags_file} not found)")
    return()
  endif()
  file(STRINGS "${_flags_file}" _no_exc REGEX "No_Exception")
  if(_no_exc)
    set(_msg "OCCT at ${dir} is built with -DNo_Exception: its index and argument checks are "
             "compiled out and corrupted BRep/STEP input crashes inside OCCT. Rebuild it with "
             "third_party/build_occt.sh (or build_occt_wasm.sh), OCCT_RELEASE_CHECKS=1, into "
             "third_party/occt-install-checked (occt-install-wasm-checked), or configure with "
             "-DBENDCORE_OCCT_REQUIRE_CHECKS=OFF to accept it (docs/kernel/OCCT_BUILD.md, раздел 6).")
    string(CONCAT _msg ${_msg})
    if(BENDCORE_OCCT_REQUIRE_CHECKS)
      message(FATAL_ERROR "${_msg}")
    endif()
    message(WARNING "${_msg}")
    set(BENDCORE_OCCT_RELEASE_CHECKS OFF CACHE INTERNAL "OCCT built without No_Exception")
  else()
    message(STATUS "OCCT Release checks: on (no -DNo_Exception in ${_flags_file})")
    set(BENDCORE_OCCT_RELEASE_CHECKS ON CACHE INTERNAL "OCCT built without No_Exception")
  endif()
endfunction()

# Explicit OCCT toolkit set (docs/kernel/OCCT_BUILD.md). The list lives in
# third_party/occt_toolkits.txt so that the native link line here and the WASM
# OCCT build (third_party/build_occt_wasm.sh) use the same toolkits.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/occt_toolkits.txt" _bendcore_occt_tk_lines REGEX "^TK")
# Editing the list re-runs the configuration.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/occt_toolkits.txt")
set(BENDCORE_OCCT_TOOLKITS "")
foreach(_line IN LISTS _bendcore_occt_tk_lines)
  string(REGEX MATCH "^TK[A-Za-z0-9_]+" _tk "${_line}")
  list(APPEND BENDCORE_OCCT_TOOLKITS ${_tk})
endforeach()
if(NOT BENDCORE_OCCT_TOOLKITS)
  message(FATAL_ERROR "third_party/occt_toolkits.txt lists no OCCT toolkits")
endif()

# Resolves BENDCORE_OCCT_TOOLKITS to static libraries under <dir>/lib; a missing
# toolkit is a configuration error (no silent partial link).
function(bendcore_resolve_occt_toolkits dir out_var)
  set(_libs "")
  foreach(_tk IN LISTS BENDCORE_OCCT_TOOLKITS)
    set(_lib "${dir}/lib/lib${_tk}.a")
    if(NOT EXISTS "${_lib}")
      message(FATAL_ERROR "OCCT toolkit ${_tk} is required (third_party/occt_toolkits.txt) "
                          "but ${_lib} does not exist. Rebuild OCCT with third_party/build_occt.sh "
                          "(static libraries only).")
    endif()
    list(APPEND _libs "${_lib}")
  endforeach()
  set(${out_var} "${_libs}" PARENT_SCOPE)
endfunction()

if(BENDCORE_OCCT_DIR)
  # Use manually built OCCT
  set(OpenCASCADE_FOUND TRUE)
  set(OpenCASCADE_INCLUDE_DIRS "${BENDCORE_OCCT_DIR}/include/opencascade")
  bendcore_resolve_occt_toolkits("${BENDCORE_OCCT_DIR}" OpenCASCADE_LIBRARIES)
  bendcore_check_occt_release_checks("${BENDCORE_OCCT_DIR}")
  message(STATUS "Using OCCT from: ${BENDCORE_OCCT_DIR}")
  message(STATUS "  Headers: ${OpenCASCADE_INCLUDE_DIRS}")
  message(STATUS "  Libraries: ${OpenCASCADE_LIBRARIES}")
else()
  # Try to find OCCT via system paths
  # NO_CMAKE_PACKAGE_REGISTRY: an OCCT build tree registers itself in
  # ~/.cmake/packages; a half-built tree there breaks configuration.
  find_package(OpenCASCADE 7.7 QUIET NO_CMAKE_PACKAGE_REGISTRY)
  if(NOT OpenCASCADE_FOUND)
    # Try config mode
    find_package(OpenCASCADE CONFIG QUIET NO_CMAKE_PACKAGE_REGISTRY)
  endif()
  if(NOT OpenCASCADE_FOUND)
    # Try pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(OpenCASCADE QUIET OpenCASCADE)
    endif()
  endif()
  if(OpenCASCADE_FOUND)
    message(STATUS "Found system OCCT")
  else()
    message(STATUS "OCCT not found (optional). Build with: cd third_party && ./build_occt.sh")
  endif()
endif()

# M1 Core Components (CAD Foundation)
set(BENDCORE_SOURCES
  # Ядро трубы и API (возвращено: выпало из сборки при слиянии b169ccb)
  src/analysis.cpp
  src/axis.cpp
  src/batch_check.cpp
  src/bend_output.cpp
  src/bendcore/profile_manager.cpp
  src/blank.cpp
  src/calib.cpp
  src/compensation.cpp
  src/core_api.cpp
  src/drawing.cpp
  src/ipc_bridge.cpp
  src/performance.cpp
  src/ports.cpp
  src/refs.cpp
  src/reporting.cpp
  src/reporting_ui.cpp
  src/sketch.cpp
  src/spiral.cpp
  src/standards.cpp
  src/tube_model.cpp
  src/tubegeom.cpp
  src/visualization.cpp
  src/dwg_export.cpp
  src/drawing_editor.cpp
  src/geometry_analyzer.cpp
  # Идентификатор сущности (используют src/features, src/editing) и система операций
  src/entity_id.cpp
  src/feature_system.cpp


  # Constraint solver
  src/constraint_system.cpp

  # M2 Phase: Advanced Sketcher (GAP-002, GAP-003, GAP-004)
  src/sketch/constraint_diagnostics.cpp
  src/sketch/primitives.cpp
  src/sketch/edit_tools.cpp

  # M5 Phase: Advanced Feature Operations (GAP-020) - Loft, Sweep, Revolve
  src/features/loft.cpp
  src/features/sweep.cpp
  src/features/revolve.cpp
  src/features/feature_system.cpp

  src/instances.cpp

  # M3 Phase: Manufacturing - BOM Generation (GAP-012)
  src/manufacturing/bom.cpp

  # M3 Phase: Mass Properties Analysis (GAP-015)
  src/analysis/mass_properties.cpp

  # M5 Phase: Direct Editing (GAP-022)
  src/editing/direct_editing_manager.cpp



  # M6 Phase: Simulation Engine (GAP-024)
  src/simulation/cfd_solver.cpp
  src/simulation/structural_solver.cpp
  src/simulation/thermal_solver.cpp
  src/simulation/simulation_manager.cpp
  src/simulation/result_visualization.cpp




  # M7 Phase: Real-time Solver (GAP-030)
  src/solver/real_time_solver.cpp
  src/solver/iterative_solvers.cpp
  src/solver/preconditioners.cpp
  src/solver/reduced_order_model.cpp
  src/solver/solver_scheduler.cpp
  src/solver/adaptive_refinement.cpp
  src/solver/performance_metrics.cpp

  # Совместная работа над моделью трубы: изменения, версии, комментарии
  src/collaboration.cpp

  # M8 Phase: Cost Analysis (GAP-035)
  src/cost_analysis.cpp

  # M17 Phase: Advanced Analytics (GAP-074 to GAP-078)
  src/analytics/design_analytics.cpp
  src/analytics/performance_monitoring.cpp
  src/analytics/usage_tracking.cpp
  src/analytics/report_generator.cpp
  src/analytics/data_visualization.cpp

  # P1-02: Security & Reliability
  src/security.cpp
)

add_library(bendcore_core STATIC
  ${BENDCORE_SOURCES}
)
target_include_directories(bendcore_core PUBLIC include third_party)  # third_party: nlohmann/json.hpp (MIT)
target_link_libraries(bendcore_core PRIVATE bendcore_eigen bendcore_zlib)

# BC-015: Link LibDWG if available
if(LibDWG_FOUND)
  target_include_directories(bendcore_core SYSTEM PRIVATE ${LibDWG_INCLUDE_DIRS})
  target_link_libraries(bendcore_core PRIVATE ${LibDWG_LIBRARIES})
  target_compile_definitions(bendcore_core PRIVATE BENDCORE_WITH_LIBDWG=1)
endif()

# bendcore_core does not use OCCT; OCCT-dependent code lives in
# bendcore_import (below) and bendcore_occt (BENDCORE_WITH_OCCT).

enable_testing()

# Тест: нативно — запуск исполняемого файла; WebAssembly — запуск через node (код возврата = результат теста).
find_program(BENDCORE_NODE node)
function(bendcore_add_test name target)
  # Тесты проверяют через assert(): в Release (NDEBUG) они бы молча отключались.
  if(MSVC)
    target_compile_options(${target} PRIVATE /UNDEBUG)
  else()
    target_compile_options(${target} PRIVATE -UNDEBUG)
  endif()
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

add_executable(test_bend_output tests/test_bend_output.cpp)
target_link_libraries(test_bend_output PRIVATE bendcore_core)
bendcore_add_test(bend_output test_bend_output)

add_executable(test_batch_check tests/test_batch_check.cpp)
target_link_libraries(test_batch_check PRIVATE bendcore_core)
bendcore_add_test(batch_check test_batch_check)

add_executable(test_instances tests/test_instances.cpp)
target_link_libraries(test_instances PRIVATE bendcore_core)
bendcore_add_test(instances test_instances)

add_executable(test_drawing tests/test_drawing.cpp)
target_link_libraries(test_drawing PRIVATE bendcore_core)
bendcore_add_test(drawing test_drawing)

# Ручные размеры и 2D-эскизы на листе трубы (src/drawing.cpp): раньше не входили в сборку.
foreach(t manual_dimensions drawing_sketches)
  add_executable(test_${t} tests/test_${t}.cpp)
  target_link_libraries(test_${t} PRIVATE bendcore_core)
  bendcore_add_test(${t} test_${t})
endforeach()

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

# Тесты на Google Test собираются, только если GTest установлен.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GTest not found - GoogleTest-based tests skipped")
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

add_executable(test_standards tests/test_standards.cpp)
target_link_libraries(test_standards PRIVATE bendcore_core)
bendcore_add_test(standards test_standards)

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

add_executable(test_reporting tests/test_reporting.cpp)
target_link_libraries(test_reporting PRIVATE bendcore_core)
bendcore_add_test(reporting test_reporting)

add_executable(test_reporting_ui tests/test_reporting_ui.cpp)
target_link_libraries(test_reporting_ui PRIVATE bendcore_core)
bendcore_add_test(reporting_ui test_reporting_ui)

add_library(bendcore_formula STATIC src/formula.cpp)
target_include_directories(bendcore_formula PUBLIC include third_party)

# Disabled - uses deprecated AST node types
#add_executable(test_formulas tests/test_formulas.cpp)
#target_link_libraries(test_formulas PRIVATE bendcore_formula)
#bendcore_add_test(formulas test_formulas)

# Template tests (GoogleTest)
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_templates tests/test_templates.cpp src/template_manager.cpp)
  target_link_libraries(test_templates PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  target_compile_definitions(test_templates PRIVATE BENDCORE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
  bendcore_add_test(templates test_templates)
endif()

# test_performance написан на GoogleTest: собирается, только если GTest установлен.
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_performance tests/test_performance.cpp)
  target_link_libraries(test_performance PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(performance test_performance)

  # Offset modules performance benchmarks
  add_executable(test_offset_performance tests/test_offset_performance.cpp
    src/combined_3d_offset.cpp
    src/bendcore/combined_3d_offset_optimized.cpp
    src/bendcore/parallel_offset.cpp
    src/bendcore/parallel_offset_optimized.cpp
    src/bendcore/offset_profiler.cpp)
  target_link_libraries(test_offset_performance PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(offset_performance test_offset_performance)

  # M2 Phase: Advanced Sketcher (GAP-002, GAP-003, GAP-004)
  add_executable(test_sketch_m2 tests/sketch/sketch_m2_tests.cpp)
  target_link_libraries(test_sketch_m2 PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(sketch_m2 test_sketch_m2)

endif()

# PlaneGCS (LGPL-2.1-or-later, извлечён из FreeCAD Sketcher, см. third_party/planegcs/NOTICE.md) — 2D-эскизы.
set(PGCS_DIR third_party/planegcs)
if(EMSCRIPTEN)
  set(PGCS_LIB_TYPE STATIC)
else()
  set(PGCS_LIB_TYPE SHARED)
endif()
add_library(planegcs ${PGCS_LIB_TYPE}
  ${PGCS_DIR}/Geo.cpp ${PGCS_DIR}/Constraints.cpp ${PGCS_DIR}/SubSystem.cpp
  ${PGCS_DIR}/GCS.cpp ${PGCS_DIR}/qp_eq.cpp)
target_include_directories(planegcs PUBLIC ${PGCS_DIR})
target_link_libraries(planegcs PUBLIC bendcore_eigen)
set_target_properties(planegcs PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(MSVC)
  target_compile_options(planegcs PRIVATE /fp:precise /fp:contract-)
else()
  target_compile_options(planegcs PRIVATE -ffp-contract=off -fno-fast-math -Wall -Wextra)
endif()

add_executable(test_planegcs tests/test_planegcs.cpp)
target_link_libraries(test_planegcs PRIVATE planegcs)
bendcore_add_test(planegcs test_planegcs)

# 2D-эскиз чертежа (7.9) поверх PlaneGCS: отдельная библиотека, ядро без LGPL-зависимости.
add_library(bendcore_sketch2d STATIC src/sketch2d.cpp)
target_include_directories(bendcore_sketch2d PUBLIC include)
target_link_libraries(bendcore_sketch2d PUBLIC planegcs)

# Sketch tests - depends on sketch2d/planegcs
add_executable(test_sketch tests/test_sketch.cpp)
target_link_libraries(test_sketch PRIVATE bendcore_core)
bendcore_add_test(sketch test_sketch)

add_executable(test_sketch2d tests/test_sketch2d.cpp)
target_link_libraries(test_sketch2d PRIVATE bendcore_sketch2d)
bendcore_add_test(sketch2d test_sketch2d)

# Комментарии и замечания (Q2 2027, Task 4)
if(GTest_FOUND)
  add_executable(test_comments tests/test_comments.cpp)
  target_link_libraries(test_comments PRIVATE bendcore_core GTest::gtest GTest::gtest_main)
  bendcore_add_test(comments test_comments)
endif()

# Слои и управление видимостью (Q2 2027, Task 3)
add_executable(test_layers tests/test_layers.cpp)
target_link_libraries(test_layers PRIVATE bendcore_core)
bendcore_add_test(layers test_layers)

# OCCT initialization test (Q3 2027, Task 1, Subtask 1)
# Анализ формы трубы по B-rep (бэкенд OCCT подключается в bendcore_occt; ядро от OCCT не зависит).
add_executable(test_geometry_analyzer tests/test_geometry_analyzer.cpp)
target_link_libraries(test_geometry_analyzer PRIVATE bendcore_core)
bendcore_add_test(geometry_analyzer test_geometry_analyzer)

# Чтение DXF ASCII (docs/drawing/DXF_IMPORT.md): без OCCT, во всех конфигурациях.
add_library(bendcore_dxf STATIC src/dxf_reader.cpp)
target_include_directories(bendcore_dxf PUBLIC include)
target_compile_features(bendcore_dxf PUBLIC cxx_std_20)
find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_dxf_reader tests/test_dxf_reader.cpp)
  target_link_libraries(test_dxf_reader PRIVATE bendcore_dxf GTest::gtest GTest::gtest_main)
  bendcore_add_test(dxf_reader test_dxf_reader)
endif()

# Import pipeline (Q3 2027, Task 1): STEP/IGES through the OCCT XCAF readers,
# DXF, geometry cache, parametric dimensions, import workflow.
if(OpenCASCADE_FOUND)
  add_library(bendcore_import STATIC
    src/occt_init.cpp
    src/step_reader.cpp
    src/import_cache.cpp
    src/parametric_builder.cpp
    src/import_workflow.cpp)
  target_include_directories(bendcore_import PUBLIC include)
  target_include_directories(bendcore_import SYSTEM PUBLIC ${OpenCASCADE_INCLUDE_DIRS})
  target_compile_definitions(bendcore_import PRIVATE BENDCORE_WITH_OCCT=1)
  target_link_libraries(bendcore_import PUBLIC bendcore_core bendcore_formula bendcore_dxf
    -Wl,--start-group ${OpenCASCADE_LIBRARIES} -Wl,--end-group pthread dl m)

  # With BENDCORE_WITH_OCCT the import workflow uses the real B-Rep
  # analyser from bendcore_occt (defined further below).
  if(BENDCORE_WITH_OCCT)
    target_link_libraries(bendcore_import PUBLIC bendcore_occt)
    target_compile_definitions(bendcore_import PRIVATE BENDCORE_HAS_OCCT_GEOMETRY_BACKEND=1)
  endif()

  foreach(t occt_init step_reader parametric_builder geometry_cache import_workflow)
    add_executable(test_${t} tests/test_${t}.cpp)
    target_link_libraries(test_${t} PRIVATE bendcore_import)
    bendcore_add_test(${t} test_${t})
  endforeach()
  target_compile_definitions(test_occt_init PRIVATE BENDCORE_WITH_OCCT=1)

  add_executable(e2e_import tests/e2e_import.cpp)
  target_link_libraries(e2e_import PRIVATE bendcore_import)
  if(BENDCORE_WITH_OCCT)
    target_compile_definitions(e2e_import PRIVATE BENDCORE_HAS_OCCT_GEOMETRY_BACKEND=1)
  endif()
  bendcore_add_test(e2e_import e2e_import)
endif()

# Subtask 3: A/B Testing Framework
add_executable(test_ab_testing tests/test_ab_testing.cpp src/ab_testing.cpp)
target_include_directories(test_ab_testing PRIVATE include)
bendcore_add_test(ab_testing test_ab_testing)

# Q4 Task 1: Advanced Drawing Features
add_executable(test_drawing_editor tests/test_drawing_editor.cpp)
target_link_libraries(test_drawing_editor PRIVATE bendcore_core bendcore_formula)
bendcore_add_test(drawing_editor test_drawing_editor)

add_executable(test_drawing_layers_manager tests/test_drawing_layers_manager.cpp src/drawing_layers.cpp)
target_link_libraries(test_drawing_layers_manager PRIVATE bendcore_core)
bendcore_add_test(drawing_layers_manager test_drawing_layers_manager)

# Труба из пути как тело (Open CASCADE): опционально, нужен установленный OCCT (см. README).
# Установка OCCT (cmake --install), например /path/to/occt-install. Конфиг OCCT для статической сборки
# содержит нераскрытые переменные, поэтому библиотеки подключаются напрямую из lib/libTK<имя>.a
# по явному списку BENDCORE_OCCT_TOOLKITS (third_party/occt_toolkits.txt).
set(BENDCORE_OCCT_DIR "" CACHE PATH "Каталог установки Open CASCADE (include/opencascade, lib/libTK*.a)")
option(BENDCORE_WITH_OCCT "Собрать проверку тела трубы на Open CASCADE" OFF)
if(BENDCORE_WITH_OCCT)
  if(NOT BENDCORE_OCCT_DIR)
    message(FATAL_ERROR "BENDCORE_WITH_OCCT=ON: укажите BENDCORE_OCCT_DIR")
  endif()
  # Тот же явный набор тулкитов (BENDCORE_OCCT_TOOLKITS), что и для bendcore_import;
  # отсутствие любого из них — ошибка конфигурации.
  bendcore_resolve_occt_toolkits("${BENDCORE_OCCT_DIR}" BENDCORE_OCCT_LIBS)
  add_library(bendcore_occt STATIC src/occt/tube_body_occt.cpp src/occt/geometry_backend_occt.cpp)
  target_include_directories(bendcore_occt SYSTEM PRIVATE "${BENDCORE_OCCT_DIR}/include/opencascade")
  target_link_libraries(bendcore_occt PUBLIC bendcore_core
    -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)
  add_executable(test_tube_occt tests/test_tube_occt.cpp)
  target_link_libraries(test_tube_occt PRIVATE bendcore_occt)
  bendcore_add_test(tube_occt test_tube_occt)
  add_executable(test_geometry_analyzer_occt tests/test_geometry_analyzer_occt.cpp)
  target_link_libraries(test_geometry_analyzer_occt PRIVATE bendcore_occt)
  bendcore_add_test(geometry_analyzer_occt test_geometry_analyzer_occt)

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

  # Ядро CAD на OCCT (docs/ROADMAP_GENERAL_CAD.md). Каждый модуль подключается своим
  # файлом, чтобы параллельные ветки не конфликтовали (docs/PROJECT_PROCESS.md, раздел 5).
  include(cmake/kernel_core.cmake OPTIONAL)
  include(cmake/kernel_healing.cmake OPTIONAL)
  include(cmake/kernel_xcaf.cmake OPTIONAL)
  include(cmake/kernel_analysis.cmake OPTIONAL)
  include(cmake/modeling.cmake OPTIONAL)
  include(cmake/modeling_sketch.cmake OPTIONAL)
  include(cmake/modeling_features.cmake OPTIONAL)
  include(cmake/modeling_document.cmake OPTIONAL)
  include(cmake/assembly.cmake OPTIONAL)
  include(cmake/assembly_document.cmake OPTIONAL)
  include(cmake/fasteners.cmake OPTIONAL)
  include(cmake/drawing.cmake OPTIONAL)
  include(cmake/drawing_views.cmake OPTIONAL)
  include(cmake/drawing_annotations.cmake OPTIONAL)
  include(cmake/drawing_document.cmake OPTIONAL)
  include(cmake/qa.cmake OPTIONAL)
  include(cmake/wasm_kernel.cmake OPTIONAL)
  # Читатель DXF против писателя bendcore_drawing_v2 и (при BENDCORE_PYTHON_EZDXF) ezdxf.
  if(TARGET bendcore_drawing_v2 AND GTest_FOUND)
    add_executable(test_dxf_reader_roundtrip
      tests/dxf/test_dxf_reader_roundtrip.cpp
      tests/drawing_v2/test_support.cpp)
    target_link_libraries(test_dxf_reader_roundtrip PRIVATE bendcore_dxf bendcore_drawing_v2
      GTest::gtest GTest::gtest_main)
    set(_dxf_rt_out "${CMAKE_CURRENT_BINARY_DIR}/dxf_reader_out")
    file(MAKE_DIRECTORY "${_dxf_rt_out}")
    target_compile_definitions(test_dxf_reader_roundtrip PRIVATE
      BENDCORE_DRAWING_V2_OUT="${_dxf_rt_out}"
      BENDCORE_EZDXF_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/dxf/ezdxf_fixture.py")
    bendcore_add_test(dxf_reader_roundtrip test_dxf_reader_roundtrip)
  endif()
endif()

# M5 Phase: Advanced Feature Operations (GAP-020) - Loft, Sweep, Revolve
add_executable(test_advanced_features_m5 tests/features/test_advanced_features_m5.cpp)
target_link_libraries(test_advanced_features_m5 PRIVATE bendcore_core)
bendcore_add_test(advanced_features_m5 test_advanced_features_m5)

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

# ============================================================================
# Phase I: Integration Testing (CAD-CORE-033 to CAD-CORE-036)
# ============================================================================

# CAD-CORE-033 through CAD-CORE-036: Phase I Integration Testing
add_executable(test_phase_i_main tests/integration/test_phase_i_main.cpp)
bendcore_add_test(phase_i_main test_phase_i_main)

# ============================================================================
# M6 Phase: FEA Integration (GAP-025)
# ============================================================================


# ============================================================================
# BOM, массовые свойства, расчётные модули
# ============================================================================

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

endif()

# ============================================================================
# P1-02: Security & Reliability (80+ tests)
# ============================================================================
# Comprehensive security hardening: input validation, serialization, memory safety,
# network security, vulnerability assessment, secret management, crash recovery, fuzz testing

add_executable(test_security tests/test_security.cpp)
target_link_libraries(test_security PRIVATE bendcore_core)
target_include_directories(test_security PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
bendcore_add_test(security test_security)

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
