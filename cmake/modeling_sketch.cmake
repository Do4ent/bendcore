# bendcore_modeling_sketch: sketch placement, profiles (contours, regions), projection of model
# edges and reference geometry (docs/modeling/SKETCH_PROFILE.md, docs/modeling/REFERENCE_GEOMETRY.md).
# Included from the BENDCORE_WITH_OCCT block of the root CMakeLists.txt.

set(BENDCORE_MODELING_SKETCH_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_modeling_sketch STATIC
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/curves.cpp
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/placement.cpp
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/profile.cpp
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/bound_sketch.cpp
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/projection.cpp
  ${BENDCORE_MODELING_SKETCH_ROOT}/src/modeling/sketch/reference_geometry.cpp)
target_compile_features(bendcore_modeling_sketch PUBLIC cxx_std_20)
target_include_directories(bendcore_modeling_sketch PUBLIC "${BENDCORE_MODELING_SKETCH_ROOT}/include")
target_link_libraries(bendcore_modeling_sketch PUBLIC bendcore_kernel bendcore_sketch2d)

find_package(GTest QUIET)
if(GTest_FOUND)
  foreach(sketch_test profile projection reference)
    add_executable(test_modeling_sketch_${sketch_test}
      ${BENDCORE_MODELING_SKETCH_ROOT}/tests/modeling/test_modeling_sketch_${sketch_test}.cpp)
    target_link_libraries(test_modeling_sketch_${sketch_test}
      PRIVATE bendcore_modeling_sketch GTest::gtest GTest::gtest_main)
    bendcore_add_test(modeling_sketch_${sketch_test} test_modeling_sketch_${sketch_test})
  endforeach()
else()
  message(STATUS "GTest not found - bendcore_modeling_sketch tests skipped")
endif()
