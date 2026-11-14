# Нагрузочные тесты фазы 5 (docs/qa/PERFORMANCE.md). Подключается из cmake/qa.cmake.
#
# ctest: stress_part_300, stress_assembly_300, stress_step_generate → stress_step_large,
# stress_hlr_1000 (метка stress). Пороги времени — в тестах, множитель —
# BENDCORE_STRESS_TIME_SCALE; отчёт с метками времени — build-occt/stress-report.jsonl.
# В CI идут отдельным последовательным шагом вместе с performance/offset_performance: под
# параллельной нагрузкой ctest замеры времени не показательны.
#
# BENDCORE_STRESS_STEP_MB — минимальный размер сгенерированного STEP (план фазы 5: ≥ 100 МБ на
# бета-сборке; по умолчанию 60 МБ — ≥ 50 МБ, укладывается в диск и время машины CI).

if(EMSCRIPTEN)
  return()
endif()
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "qa: GoogleTest не найден — нагрузочные тесты не собираются")
  return()
endif()

set(BENDCORE_STRESS_STEP_MB "60" CACHE STRING "Минимальный размер STEP нагрузочного теста, МБ")
set(_qa_stress_report "${CMAKE_CURRENT_BINARY_DIR}/stress-report.jsonl")

function(_qa_stress_test name source)
  add_executable(test_${name} tests/regression/stress/${source})
  target_compile_features(test_${name} PRIVATE cxx_std_20)
  target_include_directories(test_${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
  target_link_libraries(test_${name} PRIVATE ${ARGN} GTest::gtest)
endfunction()

set(_qa_stress_tests "")
if(TARGET bendcore_modeling_document)
  _qa_stress_test(stress_part test_stress_part.cpp bendcore_modeling_document GTest::gtest_main)
  add_test(NAME stress_part_300 COMMAND test_stress_part)
  list(APPEND _qa_stress_tests stress_part_300)
endif()
if(TARGET bendcore_assembly_document)
  _qa_stress_test(stress_assembly test_stress_assembly.cpp bendcore_assembly_document GTest::gtest_main)
  add_test(NAME stress_assembly_300 COMMAND test_stress_assembly)
  list(APPEND _qa_stress_tests stress_assembly_300)
endif()
if(TARGET bendcore_drawing_annotations)
  _qa_stress_test(stress_hlr test_stress_hlr.cpp bendcore_drawing_annotations GTest::gtest_main)
  add_test(NAME stress_hlr_1000 COMMAND test_stress_hlr)
  list(APPEND _qa_stress_tests stress_hlr_1000)
endif()
if(TARGET bendcore_kernel AND TARGET generate_corpus)
  _qa_stress_test(stress_step test_stress_step.cpp bendcore_kernel)
  set(_qa_large_step "${CMAKE_CURRENT_BINARY_DIR}/stress-large/large.step")
  add_test(NAME stress_step_generate COMMAND generate_corpus --large "${_qa_large_step}" ${BENDCORE_STRESS_STEP_MB})
  set_tests_properties(stress_step_generate PROPERTIES FIXTURES_SETUP stress_large_step TIMEOUT 1200 LABELS stress)
  add_test(NAME stress_step_large COMMAND test_stress_step "${_qa_large_step}")
  set_tests_properties(stress_step_large PROPERTIES FIXTURES_REQUIRED stress_large_step)
  list(APPEND _qa_stress_tests stress_step_large)
endif()
if(_qa_stress_tests)
  set_tests_properties(${_qa_stress_tests} PROPERTIES TIMEOUT 1800 LABELS stress
    ENVIRONMENT "BENDCORE_STRESS_REPORT=${_qa_stress_report}")
endif()
