# Инженер качества (docs/PROJECT_PROCESS.md, разделы 2 и 5): проверка заглушек,
# корпус STEP и регрессия импорта. Подключается из корневого CMakeLists.txt
# внутри if(BENDCORE_WITH_OCCT) — переменные BENDCORE_OCCT_DIR и BENDCORE_OCCT_LIBS заданы там.

# --- Проверка «никаких заглушек» --------------------------------------------------------
find_program(BENDCORE_BASH bash)
find_program(BENDCORE_PERL perl)
find_program(BENDCORE_GIT git)
if(BENDCORE_BASH AND BENDCORE_PERL AND BENDCORE_GIT AND NOT EMSCRIPTEN)
  # Самотест скрипта на фикстурах tests/qa/fixtures (нарушения, законный код, режим --diff).
  add_test(NAME qa_check_no_stubs_selftest
    COMMAND ${BENDCORE_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/qa/test_check_no_stubs.sh)
  # Полная проверка каталогов нового кода (src/kernel, src/modeling, ...).
  add_test(NAME qa_check_no_stubs_tree
    COMMAND ${BENDCORE_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_no_stubs.sh
            --root ${CMAKE_CURRENT_SOURCE_DIR})
else()
  message(STATUS "qa: bash/perl/git не найдены — тесты check_no_stubs не зарегистрированы")
endif()

# --- Корпус STEP ----------------------------------------------------------------------
# Генератор моделей с аналитически известными величинами (реальная программа на OCCT).
add_executable(generate_corpus tests/corpus/generate_corpus.cpp)
target_compile_features(generate_corpus PRIVATE cxx_std_20)
target_include_directories(generate_corpus SYSTEM PRIVATE
  "${BENDCORE_OCCT_DIR}/include/opencascade" "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_link_libraries(generate_corpus PRIVATE
  -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)

# Регрессия импорта: читатель bendcore_import (src/step_reader.cpp) против манифеста.
if(TARGET bendcore_import)
  add_executable(test_step_corpus tests/corpus/test_step_corpus.cpp)
  target_compile_features(test_step_corpus PRIVATE cxx_std_20)
  target_include_directories(test_step_corpus SYSTEM PRIVATE
    "${BENDCORE_OCCT_DIR}/include/opencascade" "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
  target_link_libraries(test_step_corpus PRIVATE bendcore_import)

  # 1) Закоммиченный корпус tests/corpus/generated: ловит изменения читателя и OCCT
  #    на неизменных файлах.
  add_test(NAME corpus_step_committed
    COMMAND test_step_corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/generated)

  # 2) Свежий корпус: генерация текущим писателем OCCT и импорт (цикл запись -> чтение).
  set(_qa_fresh "${CMAKE_CURRENT_BINARY_DIR}/corpus-fresh")
  add_test(NAME corpus_generate COMMAND generate_corpus ${_qa_fresh})
  set_tests_properties(corpus_generate PROPERTIES FIXTURES_SETUP corpus_fresh)
  add_test(NAME corpus_step_fresh COMMAND test_step_corpus ${_qa_fresh})
  set_tests_properties(corpus_step_fresh PROPERTIES FIXTURES_REQUIRED corpus_fresh)

  # 3) Внешние модели (tests/corpus/external) — только если каталог с манифестом есть.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/external/manifest.json")
    add_test(NAME corpus_step_external
      COMMAND test_step_corpus ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/external)
  endif()
else()
  message(STATUS "qa: цели bendcore_import нет — test_step_corpus не собирается")
endif()

# --- Нагрузочные тесты и fuzz-проверка загрузчиков (фаза 5, docs/qa/PERFORMANCE.md, QA.md) ---
include(${CMAKE_CURRENT_LIST_DIR}/qa_stress.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/qa_fuzz.cmake)
