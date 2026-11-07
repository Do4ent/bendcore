# bendcore_modeling_features: операции детали над OCCT (docs/modeling/FEATURES.md).
# Подключается из корневого CMakeLists.txt внутри if(BENDCORE_WITH_OCCT) после kernel_core и
# kernel_healing.

set(BENDCORE_MODELING_FEATURES_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_modeling_features STATIC
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/feature_common.cpp
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/extrude.cpp
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/revolve.cpp
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/hole.cpp
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/dress_up.cpp
  ${BENDCORE_MODELING_FEATURES_ROOT}/src/modeling/features/pattern.cpp)
target_compile_features(bendcore_modeling_features PUBLIC cxx_std_20)
target_include_directories(bendcore_modeling_features PUBLIC "${BENDCORE_MODELING_FEATURES_ROOT}/include")
target_link_libraries(bendcore_modeling_features PUBLIC bendcore_kernel bendcore_kernel_healing)

find_package(GTest QUIET)
if(GTest_FOUND)
  foreach(features_test extrude revolve hole dressup pattern history)
    add_executable(test_modeling_features_${features_test}
      ${BENDCORE_MODELING_FEATURES_ROOT}/tests/modeling/test_modeling_features_${features_test}.cpp)
    target_include_directories(test_modeling_features_${features_test} PRIVATE
      ${BENDCORE_MODELING_FEATURES_ROOT}/tests/modeling)
    target_link_libraries(test_modeling_features_${features_test} PRIVATE
      bendcore_modeling_features GTest::gtest GTest::gtest_main)
    bendcore_add_test(modeling_features_${features_test} test_modeling_features_${features_test})
  endforeach()
else()
  message(STATUS "GTest not found - bendcore_modeling_features tests skipped")
endif()
