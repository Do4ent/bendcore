# Чертежи v2 (docs/drawing/): 2D-модель листа, рамка и основная надпись ГОСТ 2.104,
# экспорт DXF/SVG/PDF. Не зависит от OCCT и от старого src/drawing.

add_library(bendcore_drawing_v2 STATIC
  src/drawing_v2/utf8.cpp
  src/drawing_v2/sheet.cpp
  src/drawing_v2/line_types.cpp
  src/drawing_v2/model.cpp
  src/drawing_v2/truetype_font.cpp
  src/drawing_v2/render_util.cpp
  src/drawing_v2/title_block.cpp
  src/drawing_v2/dxf_writer.cpp
  src/drawing_v2/svg_writer.cpp
  src/drawing_v2/pdf_writer.cpp
)
target_include_directories(bendcore_drawing_v2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(bendcore_drawing_v2 PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(bendcore_drawing_v2 PRIVATE -Wall -Wextra)
endif()

find_package(GTest QUIET)
if(GTest_FOUND)
  add_executable(test_drawing_v2
    tests/drawing_v2/test_support.cpp
    tests/drawing_v2/test_sheet_linetypes.cpp
    tests/drawing_v2/test_model.cpp
    tests/drawing_v2/test_font.cpp
    tests/drawing_v2/test_title_block.cpp
    tests/drawing_v2/test_dxf.cpp
    tests/drawing_v2/test_svg.cpp
    tests/drawing_v2/test_pdf.cpp
  )
  target_link_libraries(test_drawing_v2 PRIVATE bendcore_drawing_v2 GTest::gtest GTest::gtest_main)
  set(_drawing_v2_out "${CMAKE_CURRENT_BINARY_DIR}/drawing_v2_out")
  file(MAKE_DIRECTORY "${_drawing_v2_out}")
  target_compile_definitions(test_drawing_v2 PRIVATE BENDCORE_DRAWING_V2_OUT="${_drawing_v2_out}")
  # Независимые проверки сторонними программами (poppler, libxml2), если они установлены.
  foreach(_tool pdftotext pdfinfo pdffonts xmllint)
    string(TOUPPER ${_tool} _up)
    find_program(BENDCORE_${_up}_EXE ${_tool})
    if(BENDCORE_${_up}_EXE)
      target_compile_definitions(test_drawing_v2 PRIVATE BENDCORE_${_up}="${BENDCORE_${_up}_EXE}")
    endif()
  endforeach()
  bendcore_add_test(drawing_v2 test_drawing_v2)
else()
  message(STATUS "GTest not found - drawing_v2 tests skipped")
endif()
