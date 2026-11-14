# Размеры, допуски ISO 286, аннотации ЕСКД/ISO, спецификация ГОСТ 2.106, фоновый расчёт видов
# с кэшем (docs/drawing/ANNOTATIONS.md, docs/drawing/SPECIFICATION.md, docs/drawing/VIEWS.md).
# Подключается из блока BENDCORE_WITH_OCCT корневого CMakeLists.txt после drawing_views.cmake.

foreach(_dep bendcore_drawing_views bendcore_modeling_document bendcore_assembly_document)
  if(NOT TARGET ${_dep})
    message(STATUS "bendcore_drawing_annotations skipped: ${_dep} is not defined")
    return()
  endif()
endforeach()

find_package(Threads REQUIRED)

add_library(bendcore_drawing_annotations STATIC
  src/drawing_v2/annotations/style.cpp
  src/drawing_v2/annotations/tolerances.cpp
  src/drawing_v2/annotations/dimensions.cpp
  src/drawing_v2/annotations/symbols.cpp
  src/drawing_v2/annotations/threads.cpp
  src/drawing_v2/annotations/specification.cpp
  src/drawing_v2/annotations/view_cache.cpp
)
target_include_directories(bendcore_drawing_annotations PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bendcore_drawing_annotations PUBLIC
  bendcore_drawing_views bendcore_modeling_document bendcore_assembly_document Threads::Threads)
target_compile_features(bendcore_drawing_annotations PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(bendcore_drawing_annotations PRIVATE -Wall -Wextra)
endif()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GTest not found - drawing_annotations tests skipped")
  return()
endif()

add_executable(test_drawing_annotations
  tests/drawing_v2/test_support.cpp
  tests/drawing_v2/test_drawing_views_support.cpp
  tests/drawing_v2/test_annotations_support.cpp
  tests/drawing_v2/test_annotations_tolerances.cpp
  tests/drawing_v2/test_annotations_dimensions.cpp
  tests/drawing_v2/test_annotations_symbols.cpp
  tests/drawing_v2/test_annotations_specification.cpp
  tests/drawing_v2/test_annotations_export.cpp
  tests/drawing_v2/test_annotations_view_cache.cpp
)
target_include_directories(test_drawing_annotations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/drawing_v2)
target_link_libraries(test_drawing_annotations PRIVATE bendcore_drawing_annotations GTest::gtest GTest::gtest_main)
set(_drawing_annotations_out "${CMAKE_CURRENT_BINARY_DIR}/drawing_annotations_out")
file(MAKE_DIRECTORY "${_drawing_annotations_out}")
target_compile_definitions(test_drawing_annotations PRIVATE BENDCORE_DRAWING_V2_OUT="${_drawing_annotations_out}")
foreach(_tool pdftotext pdfinfo)
  string(TOUPPER ${_tool} _up)
  find_program(BENDCORE_${_up}_EXE ${_tool})
  if(BENDCORE_${_up}_EXE)
    target_compile_definitions(test_drawing_annotations PRIVATE BENDCORE_${_up}="${BENDCORE_${_up}_EXE}")
  endif()
endforeach()
bendcore_add_test(drawing_annotations test_drawing_annotations)

# Независимая проверка DXF пакетом ezdxf (MIT), если он установлен у python3.
find_program(BENDCORE_ANNOT_PYTHON python3)
if(BENDCORE_ANNOT_PYTHON)
  execute_process(COMMAND ${BENDCORE_ANNOT_PYTHON} -c "import ezdxf"
                  RESULT_VARIABLE _ezdxf_rc OUTPUT_QUIET ERROR_QUIET)
  if(_ezdxf_rc EQUAL 0 AND TEST drawing_annotations)
    set_tests_properties(drawing_annotations PROPERTIES ENVIRONMENT "BENDCORE_PYTHON_EZDXF=${BENDCORE_ANNOT_PYTHON}")
  endif()
endif()

# Замер фонового расчёта видов и кэша (docs/drawing/VIEWS.md, раздел «Фоновый расчёт и кэш»).
add_executable(bench_drawing_view_cache tests/drawing_v2/bench_drawing_view_cache.cpp)
target_link_libraries(bench_drawing_view_cache PRIVATE bendcore_drawing_annotations)
