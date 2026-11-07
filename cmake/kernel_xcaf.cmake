# bendcore_kernel_xcaf: STEP with assembly structure, names, colours and layers through
# XCAF, in memory (docs/kernel/XCAF_IO.md). Included from the BENDCORE_WITH_OCCT block of
# the root CMakeLists.txt after cmake/kernel_core.cmake (needs bendcore_kernel).

if(NOT TARGET bendcore_kernel)
  message(STATUS "bendcore_kernel is not defined - bendcore_kernel_xcaf skipped")
  return()
endif()

set(BENDCORE_KERNEL_XCAF_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_kernel_xcaf STATIC ${BENDCORE_KERNEL_XCAF_ROOT}/src/kernel/xcaf_io.cpp)
target_compile_features(bendcore_kernel_xcaf PUBLIC cxx_std_20)
target_include_directories(bendcore_kernel_xcaf PUBLIC "${BENDCORE_KERNEL_XCAF_ROOT}/include")
target_include_directories(bendcore_kernel_xcaf SYSTEM PUBLIC "${BENDCORE_OCCT_DIR}/include/opencascade")
# TKXCAF needs TKVCAF/TKV3d/TKService at link time (docs/kernel/OCCT_BUILD.md); all are in
# BENDCORE_OCCT_LIBS.
target_link_libraries(bendcore_kernel_xcaf PUBLIC bendcore_kernel
  -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)

if(GTest_FOUND)
  foreach(xcaf_test xcaf xcaf_roundtrip)
    add_executable(test_kernel_${xcaf_test}
      ${BENDCORE_KERNEL_XCAF_ROOT}/tests/kernel/test_kernel_${xcaf_test}.cpp)
    target_link_libraries(test_kernel_${xcaf_test} PRIVATE bendcore_kernel_xcaf GTest::gtest GTest::gtest_main)
    target_include_directories(test_kernel_${xcaf_test} SYSTEM PRIVATE
      "${BENDCORE_KERNEL_XCAF_ROOT}/third_party")
    target_compile_definitions(test_kernel_${xcaf_test} PRIVATE
      BENDCORE_CORPUS_DIR="${BENDCORE_KERNEL_XCAF_ROOT}/tests/corpus/generated")
    bendcore_add_test(kernel_${xcaf_test} test_kernel_${xcaf_test})
  endforeach()
else()
  message(STATUS "GTest not found - bendcore_kernel_xcaf tests skipped")
endif()
