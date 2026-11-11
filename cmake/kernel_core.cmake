# bendcore_kernel: Shape, BRep/STEP exchange in memory, tessellation (docs/kernel/KERNEL_CORE.md).
# Included from the BENDCORE_WITH_OCCT block of the root CMakeLists.txt, where
# BENDCORE_OCCT_DIR and BENDCORE_OCCT_LIBS are defined.

set(BENDCORE_KERNEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(bendcore_kernel STATIC
  ${BENDCORE_KERNEL_ROOT}/src/kernel/shape.cpp
  ${BENDCORE_KERNEL_ROOT}/src/kernel/brep_io.cpp
  ${BENDCORE_KERNEL_ROOT}/src/kernel/occt_fault_guard.cpp
  ${BENDCORE_KERNEL_ROOT}/src/kernel/step_io.cpp
  ${BENDCORE_KERNEL_ROOT}/src/kernel/tessellation.cpp)
target_compile_features(bendcore_kernel PUBLIC cxx_std_20)
target_include_directories(bendcore_kernel PUBLIC "${BENDCORE_KERNEL_ROOT}/include")
target_include_directories(bendcore_kernel SYSTEM PUBLIC "${BENDCORE_OCCT_DIR}/include/opencascade")
target_link_libraries(bendcore_kernel PUBLIC
  -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)

find_package(GTest QUIET)
if(GTest_FOUND)
  foreach(kernel_test shape brep_io step_io tessellation)
    add_executable(test_kernel_${kernel_test}
      ${BENDCORE_KERNEL_ROOT}/tests/kernel/test_kernel_${kernel_test}.cpp)
    target_link_libraries(test_kernel_${kernel_test} PRIVATE bendcore_kernel GTest::gtest GTest::gtest_main)
    bendcore_add_test(kernel_${kernel_test} test_kernel_${kernel_test})
  endforeach()
else()
  message(STATUS "GTest not found - bendcore_kernel tests skipped")
endif()
