# Документ сборки на геометрии OCCT (docs/assembly/ASSEMBLY_DOCUMENT.md): компоненты,
# сопряжения по граням/рёбрам/вершинам, пересечения, массивы, BOM, разнесённый вид, STEP.
# Подключается из корневого CMakeLists.txt после cmake/assembly.cmake внутри BENDCORE_WITH_OCCT.

foreach(_dep bendcore_assembly_v2 bendcore_kernel bendcore_kernel_healing bendcore_kernel_xcaf
             bendcore_modeling_document)
  if(NOT TARGET ${_dep})
    message(WARNING "bendcore_assembly_document: ${_dep} is not defined - module skipped")
    return()
  endif()
endforeach()

set(BENDCORE_ASSEMBLY_DOC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_assembly_document STATIC
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/occt_entities.cpp
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/assembly_document.cpp
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/patterns.cpp
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/exchange.cpp
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/interference.cpp
  ${BENDCORE_ASSEMBLY_DOC_ROOT}/src/assembly_v2/document/bom.cpp)
target_compile_features(bendcore_assembly_document PUBLIC cxx_std_20)
target_include_directories(bendcore_assembly_document PUBLIC "${BENDCORE_ASSEMBLY_DOC_ROOT}/include")
target_link_libraries(bendcore_assembly_document PUBLIC
  bendcore_assembly_v2 bendcore_modeling_document bendcore_kernel_xcaf bendcore_kernel_healing bendcore_kernel)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(WARNING "bendcore_assembly_document: GoogleTest not found - tests are not built")
  return()
endif()

foreach(_t mates patterns interference bom exchange)
  add_executable(test_assembly_document_${_t}
    ${BENDCORE_ASSEMBLY_DOC_ROOT}/tests/assembly_v2/test_assembly_document_${_t}.cpp)
  target_link_libraries(test_assembly_document_${_t} PRIVATE bendcore_assembly_document GTest::gtest GTest::gtest_main)
  bendcore_add_test(assembly_document_${_t} test_assembly_document_${_t})
endforeach()
