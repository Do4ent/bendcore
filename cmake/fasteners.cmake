# Библиотека стандартного крепежа ГОСТ/ISO (docs/assembly/FASTENERS.md): таблицы размеров,
# модели OCCT, свойства для BOM, именованные грани, постановка пакета крепежа в отверстия.
# Подключается из корневого CMakeLists.txt после cmake/assembly_document.cmake.

foreach(_dep bendcore_assembly_document bendcore_modeling_document bendcore_kernel)
  if(NOT TARGET ${_dep})
    message(WARNING "bendcore_fasteners: ${_dep} is not defined - module skipped")
    return()
  endif()
endforeach()

set(BENDCORE_FASTENERS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_fasteners STATIC
  ${BENDCORE_FASTENERS_ROOT}/src/assembly_v2/fasteners/tables.cpp
  ${BENDCORE_FASTENERS_ROOT}/src/assembly_v2/fasteners/models.cpp
  ${BENDCORE_FASTENERS_ROOT}/src/assembly_v2/fasteners/placement.cpp)
target_compile_features(bendcore_fasteners PUBLIC cxx_std_20)
target_include_directories(bendcore_fasteners PUBLIC "${BENDCORE_FASTENERS_ROOT}/include")
target_link_libraries(bendcore_fasteners PUBLIC bendcore_assembly_document)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(WARNING "bendcore_fasteners: GoogleTest not found - tests are not built")
  return()
endif()

foreach(_t models placement)
  add_executable(test_fasteners_${_t} ${BENDCORE_FASTENERS_ROOT}/tests/assembly_v2/test_fasteners_${_t}.cpp)
  target_link_libraries(test_fasteners_${_t} PRIVATE bendcore_fasteners GTest::gtest GTest::gtest_main)
  bendcore_add_test(fasteners_${_t} test_fasteners_${_t})
endforeach()
