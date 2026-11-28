# Изделие уровня «редуктор» (критерий готовности v1.0): генератор деталей, сборки, чертежей и
# спецификации одноступенчатого цилиндрического редуктора РЦ-80 (examples/gearbox/README.md) и
# сквозной тест изделия tests/e2e-product/. Подключается из блока BENDCORE_WITH_OCCT корневого
# CMakeLists.txt после drawing_document.cmake.

foreach(_dep bendcore_drawing_document bendcore_fasteners bendcore_modeling_document bendcore_assembly_document)
  if(NOT TARGET ${_dep})
    message(STATUS "gearbox example skipped: ${_dep} is not defined")
    return()
  endif()
endforeach()

add_library(bendcore_gearbox_example STATIC
  examples/gearbox/gearbox_support.cpp
  examples/gearbox/gearbox_parts.cpp
  examples/gearbox/gearbox_assembly.cpp
  examples/gearbox/gearbox_drawings.cpp
  examples/gearbox/gearbox_kit.cpp
)
target_include_directories(bendcore_gearbox_example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/examples/gearbox)
target_link_libraries(bendcore_gearbox_example PUBLIC bendcore_drawing_document bendcore_fasteners)
target_compile_features(bendcore_gearbox_example PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(bendcore_gearbox_example PRIVATE -Wall -Wextra)
endif()

# Генератор комплекта: gearbox_generate <каталог> — пересобирает examples/gearbox/.
add_executable(gearbox_generate examples/gearbox/generate_gearbox.cpp)
target_link_libraries(gearbox_generate PRIVATE bendcore_gearbox_example)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "gearbox product test skipped (GTest missing)")
  return()
endif()

add_executable(test_e2e_product_gearbox tests/e2e-product/test_gearbox.cpp)
target_link_libraries(test_e2e_product_gearbox PRIVATE bendcore_gearbox_example GTest::gtest GTest::gtest_main)
set(_gearbox_out "${CMAKE_CURRENT_BINARY_DIR}/e2e_product_out")
file(MAKE_DIRECTORY "${_gearbox_out}")
target_compile_definitions(test_e2e_product_gearbox PRIVATE BENDCORE_GEARBOX_OUT="${_gearbox_out}"
                           BENDCORE_GEARBOX_COMMITTED="${CMAKE_CURRENT_SOURCE_DIR}/examples/gearbox")
foreach(_tool pdfinfo pdftotext)
  string(TOUPPER ${_tool} _up)
  find_program(BENDCORE_${_up}_EXE ${_tool})
  if(BENDCORE_${_up}_EXE)
    target_compile_definitions(test_e2e_product_gearbox PRIVATE BENDCORE_${_up}="${BENDCORE_${_up}_EXE}")
  endif()
endforeach()
bendcore_add_test(e2e_product_gearbox test_e2e_product_gearbox)
set_tests_properties(e2e_product_gearbox PROPERTIES TIMEOUT 1800)
