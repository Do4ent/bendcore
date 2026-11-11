# Fuzz-проверка загрузчиков файлов (docs/qa/QA.md, раздел 7). Подключается из cmake/qa.cmake.
#
# Всегда (native, OCCT): test_fuzz_loaders — детерминированный мутационный тест с фиксированным
# зерном (ctest fuzz_loaders). С -DBENDCORE_FUZZ=ON (только clang): фаззеры libFuzzer
# fuzz_<цель>, затравочный корпус fuzz_seeds и прогон фаззеров по корпусу в ctest.
# Санитайзеры задаются для всей сборки флагами компилятора (конфигурация build-fuzz):
#   CC=clang CXX=clang++ cmake -S . -B build-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#     -DBENDCORE_WITH_OCCT=ON -DBENDCORE_OCCT_DIR=... -DBENDCORE_FUZZ=ON \
#     -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
#     -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined,fuzzer-no-link -fno-sanitize-recover=undefined -fno-omit-frame-pointer"

option(BENDCORE_FUZZ "Фаззеры libFuzzer загрузчиков файлов (clang, -fsanitize=fuzzer)" OFF)

set(_qa_fuzz_libs bendcore_modeling_document bendcore_assembly_document bendcore_drawing_document
                  bendcore_drawing_v2 bendcore_dxf bendcore_kernel)
foreach(_l ${_qa_fuzz_libs})
  if(NOT TARGET ${_l})
    message(STATUS "qa: цели ${_l} нет — fuzz-тесты загрузчиков не собираются")
    return()
  endif()
endforeach()

add_library(bendcore_qa_fuzz_targets STATIC tests/regression/fuzz/fuzz_targets.cpp)
target_compile_features(bendcore_qa_fuzz_targets PUBLIC cxx_std_20)
target_include_directories(bendcore_qa_fuzz_targets PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_link_libraries(bendcore_qa_fuzz_targets PUBLIC ${_qa_fuzz_libs})

set(_qa_fuzz_artifacts "${CMAKE_CURRENT_BINARY_DIR}/fuzz-artifacts")

find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_fuzz_loaders tests/regression/fuzz/test_fuzz_loaders.cpp)
  target_compile_definitions(test_fuzz_loaders PRIVATE BENDCORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(test_fuzz_loaders PRIVATE bendcore_qa_fuzz_targets GTest::gtest GTest::gtest_main)
  add_test(NAME fuzz_loaders COMMAND test_fuzz_loaders)
  # Утечки памяти OCCT (статические кэши) — не предмет проверки; ASan проверяет доступ к памяти.
  set_tests_properties(fuzz_loaders PROPERTIES TIMEOUT 1800 LABELS fuzz
    ENVIRONMENT "BENDCORE_FUZZ_ARTIFACTS=${_qa_fuzz_artifacts};ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=print_stacktrace=1")
endif()

if(NOT BENDCORE_FUZZ)
  return()
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "BENDCORE_FUZZ=ON требует clang (libFuzzer, -fsanitize=fuzzer)")
endif()

add_executable(fuzz_seeds tests/regression/fuzz/fuzz_seeds.cpp)
target_link_libraries(fuzz_seeds PRIVATE bendcore_qa_fuzz_targets)
set(_qa_fuzz_corpus "${CMAKE_CURRENT_BINARY_DIR}/fuzz-corpus")
add_test(NAME fuzz_seeds COMMAND fuzz_seeds "${CMAKE_CURRENT_SOURCE_DIR}" "${_qa_fuzz_corpus}")
set_tests_properties(fuzz_seeds PROPERTIES FIXTURES_SETUP fuzz_corpus LABELS fuzz
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

foreach(_t part_json assembly_json drawing_json step dxf brep_binary brep_text)
  add_executable(fuzz_${_t} tests/regression/fuzz/fuzz_main.cpp)
  target_compile_definitions(fuzz_${_t} PRIVATE BENDCORE_FUZZ_TARGET="${_t}")
  target_link_libraries(fuzz_${_t} PRIVATE bendcore_qa_fuzz_targets)
  target_link_options(fuzz_${_t} PRIVATE -fsanitize=fuzzer)
  # Прогон фаззера по затравочному корпусу (-runs=0: только сами файлы) — регрессия в ctest.
  # Длительный поиск — отдельным шагом CI (-max_total_time).
  add_test(NAME fuzz_corpus_${_t}
    COMMAND fuzz_${_t} -runs=0 -timeout=60 -rss_limit_mb=4096
            -artifact_prefix=${_qa_fuzz_artifacts}/ "${_qa_fuzz_corpus}/${_t}")
  set_tests_properties(fuzz_corpus_${_t} PROPERTIES FIXTURES_REQUIRED fuzz_corpus LABELS fuzz TIMEOUT 600
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endforeach()
file(MAKE_DIRECTORY "${_qa_fuzz_artifacts}")
