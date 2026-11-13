# Документ чертежа: ссылка на модель, листы, виды, аннотации, обновление, формат файла,
# спецификация как отдельный документ, экспорт комплекта (docs/drawing/DRAWING_DOCUMENT.md).
# Подключается из блока BENDCORE_WITH_OCCT корневого CMakeLists.txt после drawing_annotations.cmake.

foreach(_dep bendcore_drawing_annotations bendcore_modeling_document bendcore_assembly_document)
  if(NOT TARGET ${_dep})
    message(STATUS "bendcore_drawing_document skipped: ${_dep} is not defined")
    return()
  endif()
endforeach()

add_library(bendcore_drawing_document STATIC
  src/drawing_v2/document/model_snapshot.cpp
  src/drawing_v2/document/drawing_document.cpp
  src/drawing_v2/document/serialization.cpp
  src/drawing_v2/document/specification_document.cpp
)
target_include_directories(bendcore_drawing_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# nlohmann/json (MIT) — только для формата файла (serialization.cpp).
target_include_directories(bendcore_drawing_document PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(bendcore_drawing_document PUBLIC bendcore_drawing_annotations)
target_compile_features(bendcore_drawing_document PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(bendcore_drawing_document PRIVATE -Wall -Wextra)
endif()

find_package(GTest QUIET)
if(NOT GTest_FOUND OR NOT TARGET bendcore_fasteners)
  message(STATUS "drawing_document tests skipped (GTest or bendcore_fasteners missing)")
  return()
endif()

add_executable(test_drawing_document
  tests/drawing_v2/test_support.cpp
  tests/drawing_v2/test_drawing_document_support.cpp
  tests/drawing_v2/test_drawing_document_parts.cpp
  tests/drawing_v2/test_drawing_document_assembly.cpp
  tests/drawing_v2/test_drawing_document_io.cpp
  tests/drawing_v2/test_drawing_threads.cpp
)
target_include_directories(test_drawing_document PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/drawing_v2
                           ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(test_drawing_document PRIVATE bendcore_drawing_document bendcore_fasteners
                      GTest::gtest GTest::gtest_main)
set(_drawing_document_out "${CMAKE_CURRENT_BINARY_DIR}/drawing_document_out")
file(MAKE_DIRECTORY "${_drawing_document_out}")
target_compile_definitions(test_drawing_document PRIVATE BENDCORE_DRAWING_V2_OUT="${_drawing_document_out}")
foreach(_tool pdftotext pdfinfo)
  string(TOUPPER ${_tool} _up)
  find_program(BENDCORE_${_up}_EXE ${_tool})
  if(BENDCORE_${_up}_EXE)
    target_compile_definitions(test_drawing_document PRIVATE BENDCORE_${_up}="${BENDCORE_${_up}_EXE}")
  endif()
endforeach()
bendcore_add_test(drawing_document test_drawing_document)
set_tests_properties(drawing_document PROPERTIES TIMEOUT 900)

find_program(BENDCORE_DOC_PYTHON python3)
if(BENDCORE_DOC_PYTHON)
  execute_process(COMMAND ${BENDCORE_DOC_PYTHON} -c "import ezdxf"
                  RESULT_VARIABLE _ezdxf_doc_rc OUTPUT_QUIET ERROR_QUIET)
  if(_ezdxf_doc_rc EQUAL 0 AND TEST drawing_document)
    set_tests_properties(drawing_document PROPERTIES ENVIRONMENT "BENDCORE_PYTHON_EZDXF=${BENDCORE_DOC_PYTHON}")
  endif()
endif()
