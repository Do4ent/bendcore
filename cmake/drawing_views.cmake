# Виды чертежа по модели (docs/drawing/VIEWS.md): ГОСТ 2.305 / 2.317, HLR OCCT, разрезы,
# сечения, выносные элементы, осевые линии, размещение на листе по ГОСТ 2.302.
# Подключается из блока BENDCORE_WITH_OCCT корневого CMakeLists.txt после drawing.cmake.

if(NOT TARGET bendcore_drawing_v2 OR NOT TARGET bendcore_kernel)
  message(STATUS "bendcore_drawing_views skipped: needs bendcore_drawing_v2 and bendcore_kernel")
  return()
endif()

add_library(bendcore_drawing_views STATIC
  src/drawing_v2/views/frames.cpp
  src/drawing_v2/views/hlr.cpp
  src/drawing_v2/views/overlap.cpp
  src/drawing_v2/views/sections.cpp
  src/drawing_v2/views/section_cuts.cpp
  src/drawing_v2/views/center_lines.cpp
  src/drawing_v2/views/sheet_layout.cpp
  src/drawing_v2/views/render.cpp
)
target_include_directories(bendcore_drawing_views PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bendcore_drawing_views PUBLIC bendcore_drawing_v2 bendcore_kernel)
target_compile_features(bendcore_drawing_views PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(bendcore_drawing_views PRIVATE -Wall -Wextra)
endif()

find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_drawing_views
    tests/drawing_v2/test_support.cpp
    tests/drawing_v2/test_drawing_views_support.cpp
    tests/drawing_v2/test_drawing_views_frames.cpp
    tests/drawing_v2/test_drawing_views_hlr.cpp
    tests/drawing_v2/test_drawing_views_sections.cpp
    tests/drawing_v2/test_drawing_views_local_sections.cpp
    tests/drawing_v2/test_drawing_views_sheet.cpp
    tests/drawing_v2/test_drawing_views_export.cpp
  )
  target_include_directories(test_drawing_views PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/drawing_v2)
  target_link_libraries(test_drawing_views PRIVATE bendcore_drawing_views GTest::gtest GTest::gtest_main)
  set(_drawing_views_out "${CMAKE_CURRENT_BINARY_DIR}/drawing_views_out")
  file(MAKE_DIRECTORY "${_drawing_views_out}")
  target_compile_definitions(test_drawing_views PRIVATE BENDCORE_DRAWING_V2_OUT="${_drawing_views_out}")
  foreach(_tool pdftotext pdfinfo)
    string(TOUPPER ${_tool} _up)
    find_program(BENDCORE_${_up}_EXE ${_tool})
    if(BENDCORE_${_up}_EXE)
      target_compile_definitions(test_drawing_views PRIVATE BENDCORE_${_up}="${BENDCORE_${_up}_EXE}")
    endif()
  endforeach()
  bendcore_add_test(drawing_views test_drawing_views)

  # Замер производительности HLR на модели среднего размера (docs/drawing/VIEWS.md).
  add_executable(bench_drawing_views_hlr tests/drawing_v2/bench_drawing_views_hlr.cpp)
  target_link_libraries(bench_drawing_views_hlr PRIVATE bendcore_drawing_views)
else()
  message(STATUS "GTest not found - drawing_views tests skipped")
endif()

# Независимая проверка DXF пакетом ezdxf (MIT), если он установлен у python3.
if(GTest_FOUND)
  find_program(BENDCORE_VIEWS_PYTHON python3)
  if(BENDCORE_VIEWS_PYTHON)
    execute_process(COMMAND ${BENDCORE_VIEWS_PYTHON} -c "import ezdxf"
                    RESULT_VARIABLE _ezdxf_rc OUTPUT_QUIET ERROR_QUIET)
    if(_ezdxf_rc EQUAL 0 AND TEST drawing_views)
      set_tests_properties(drawing_views PROPERTIES ENVIRONMENT "BENDCORE_PYTHON_EZDXF=${BENDCORE_VIEWS_PYTHON}")
    endif()
  endif()
endif()
