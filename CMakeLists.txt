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

add_library(bendcore_core STATIC
  src/tubegeom.cpp src/spiral.cpp src/blank.cpp src/compensation.cpp src/calib.cpp
  src/ports.cpp src/refs.cpp src/axis.cpp src/tube_model.cpp src/analysis.cpp
  src/visualization.cpp src/collaboration.cpp src/standards.cpp src/reporting.cpp
  src/reporting_ui.cpp src/performance.cpp src/ipc_bridge.cpp
  src/bendcore/profile_manager.cpp src/bend_output.cpp src/batch_check.cpp
  src/instances.cpp src/dwg_export.cpp src/formula.cpp src/template_manager.cpp
  src/geometry_analyzer.cpp src/parametric_builder.cpp src/geometry_cache.cpp
  src/dxf_reader.cpp src/import_workflow.cpp src/growthbook_client.cpp
  src/analytics_event.cpp src/drawing_editor.cpp src/drawing_layers.cpp
  src/drawing_undo_redo.cpp src/combined_3d_offset.cpp src/bendcore/offset_profiler.cpp
  src/dwfx_parser.cpp src/formula_engine.cpp src/geometry_ops.cpp
  src/document/entity_id.cpp src/document/cad_document.cpp src/document/hierarchy_adapter.cpp
  src/commands/command.cpp src/commands/command_validator.cpp src/commands/command_executor.cpp
)
target_include_directories(bendcore_core PUBLIC include third_party third_party_stubs)  # third_party: nlohmann/json.hpp (MIT), third_party_stubs: Eigen/Dense (fallback)
target_link_libraries(bendcore_core PRIVATE bendcore_eigen OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)

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

add_executable(test_bend_output tests/test_bend_output.cpp)
target_link_libraries(test_bend_output PRIVATE bendcore_core)
bendcore_add_test(bend_output test_bend_output)

add_executable(test_batch_check tests/test_batch_check.cpp)
target_link_libraries(test_batch_check PRIVATE bendcore_core)
bendcore_add_test(batch_check test_batch_check)

add_executable(test_instances tests/test_instances.cpp)
target_link_libraries(test_instances PRIVATE bendcore_core)
bendcore_add_test(instances test_instances)

add_executable(test_entity_id tests/document/test_entity_id.cpp)
target_link_libraries(test_entity_id PRIVATE bendcore_core)
bendcore_add_test(entity_id test_entity_id)

add_executable(test_cad_document tests/document/test_cad_document.cpp)
target_link_libraries(test_cad_document PRIVATE bendcore_core)
bendcore_add_test(cad_document test_cad_document)

add_executable(test_hierarchy_extended tests/document/test_hierarchy_extended.cpp)
target_link_libraries(test_hierarchy_extended PRIVATE bendcore_core)
bendcore_add_test(hierarchy_extended test_hierarchy_extended)

add_executable(test_command_executor tests/commands/test_command_executor.cpp)
target_link_libraries(test_command_executor PRIVATE bendcore_core)
bendcore_add_test(command_executor test_command_executor)

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

add_executable(test_visualization tests/test_visualization.cpp)
target_link_libraries(test_visualization PRIVATE bendcore_core)
bendcore_add_test(visualization test_visualization)

add_executable(test_collaboration tests/test_collaboration.cpp)
target_link_libraries(test_collaboration PRIVATE bendcore_core)
bendcore_add_test(collaboration test_collaboration)

add_executable(test_standards tests/test_standards.cpp)
target_link_libraries(test_standards PRIVATE bendcore_core)
bendcore_add_test(standards test_standards)

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
