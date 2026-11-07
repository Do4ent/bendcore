# bendcore_modeling_document: part document (feature tree, expressions, JSON) and topological
# naming (docs/modeling/PART_DOCUMENT.md, docs/modeling/TOPOLOGICAL_NAMING.md).
# Included from the BENDCORE_WITH_OCCT block of the root CMakeLists.txt.

if(NOT TARGET bendcore_kernel)
  message(WARNING "bendcore_modeling_document: bendcore_kernel is not defined - module skipped")
  return()
endif()

set(BENDCORE_MODELING_DOC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_modeling_document STATIC
  ${BENDCORE_MODELING_DOC_ROOT}/src/modeling/naming/signature.cpp
  ${BENDCORE_MODELING_DOC_ROOT}/src/modeling/naming/element_map.cpp
  ${BENDCORE_MODELING_DOC_ROOT}/src/modeling/document/part_document.cpp
  ${BENDCORE_MODELING_DOC_ROOT}/src/modeling/document/serialization.cpp
  ${BENDCORE_MODELING_DOC_ROOT}/src/modeling/document/basic_features.cpp)
target_compile_features(bendcore_modeling_document PUBLIC cxx_std_20)
target_include_directories(bendcore_modeling_document PUBLIC "${BENDCORE_MODELING_DOC_ROOT}/include")
target_include_directories(bendcore_modeling_document PRIVATE "${BENDCORE_MODELING_DOC_ROOT}/third_party")
# Expressions: bendcore_formula (src/formula.cpp, tested by test_formula_comprehensive).
target_link_libraries(bendcore_modeling_document PUBLIC bendcore_kernel PRIVATE bendcore_formula)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(WARNING "bendcore_modeling_document: GoogleTest not found - tests/modeling are not built")
  return()
endif()

foreach(_t naming tree io survival)
  add_executable(test_modeling_document_${_t}
    ${BENDCORE_MODELING_DOC_ROOT}/tests/modeling/test_modeling_document_${_t}.cpp)
  target_include_directories(test_modeling_document_${_t} PRIVATE "${BENDCORE_MODELING_DOC_ROOT}/third_party")
  target_link_libraries(test_modeling_document_${_t} PRIVATE bendcore_modeling_document GTest::gtest GTest::gtest_main)
  bendcore_add_test(modeling_document_${_t} test_modeling_document_${_t})
endforeach()
