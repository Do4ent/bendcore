# bendcore_kernel_analysis: materials, mass properties, STL/3MF export, measurements and
# planar sections (docs/kernel/ANALYSIS.md). Included from the BENDCORE_WITH_OCCT block of the
# root CMakeLists.txt after cmake/kernel_core.cmake (needs bendcore_kernel).

if(NOT TARGET bendcore_kernel)
  message(STATUS "bendcore_kernel is not defined - bendcore_kernel_analysis skipped")
  return()
endif()

set(BENDCORE_KERNEL_ANALYSIS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# zlib (deflate and CRC-32 of 3MF packages): the system library, ZLIB::ZLIB.
if(NOT TARGET ZLIB::ZLIB)
  if(EMSCRIPTEN)
    # No system zlib when cross-compiling; the module has no WASM consumer yet, so it is
    # left out of WASM builds until zlib is provided for them (docs/kernel/ANALYSIS.md).
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
      message(STATUS "zlib not found for Emscripten - bendcore_kernel_analysis skipped")
      return()
    endif()
  else()
    find_package(ZLIB REQUIRED)
  endif()
endif()

add_library(bendcore_kernel_analysis STATIC
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/material.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/mass_properties.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/mesh_export.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/measure.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/section.cpp)
target_compile_features(bendcore_kernel_analysis PUBLIC cxx_std_20)
target_include_directories(bendcore_kernel_analysis PUBLIC "${BENDCORE_KERNEL_ANALYSIS_ROOT}/include")
# nlohmann/json.hpp (MIT) for the material JSON.
target_include_directories(bendcore_kernel_analysis SYSTEM PRIVATE "${BENDCORE_KERNEL_ANALYSIS_ROOT}/third_party")
target_link_libraries(bendcore_kernel_analysis PUBLIC bendcore_kernel PRIVATE ZLIB::ZLIB)

if(GTest_FOUND)
  # The 3MF test parses the model XML with expat (independent of the writer).
  find_package(EXPAT QUIET)
  set(analysis_tests materials mass_properties measure section)
  if(EXPAT_FOUND)
    list(APPEND analysis_tests mesh_export)
  else()
    message(STATUS "expat not found - test_kernel_mesh_export skipped")
  endif()
  foreach(analysis_test IN LISTS analysis_tests)
    add_executable(test_kernel_${analysis_test}
      ${BENDCORE_KERNEL_ANALYSIS_ROOT}/tests/kernel/test_kernel_${analysis_test}.cpp)
    target_link_libraries(test_kernel_${analysis_test} PRIVATE
      bendcore_kernel_analysis GTest::gtest GTest::gtest_main)
    bendcore_add_test(kernel_${analysis_test} test_kernel_${analysis_test})
  endforeach()
  if(EXPAT_FOUND)
    target_link_libraries(test_kernel_mesh_export PRIVATE EXPAT::EXPAT)
  endif()
else()
  message(STATUS "GTest not found - bendcore_kernel_analysis tests skipped")
endif()
