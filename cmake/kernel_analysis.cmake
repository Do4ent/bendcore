# bendcore_kernel_analysis: materials, mass properties, STL/3MF export, measurements and
# planar sections (docs/kernel/ANALYSIS.md). Included from the BENDCORE_WITH_OCCT block of the
# root CMakeLists.txt after cmake/kernel_core.cmake (needs bendcore_kernel).

if(NOT TARGET bendcore_kernel)
  message(STATUS "bendcore_kernel is not defined - bendcore_kernel_analysis skipped")
  return()
endif()

set(BENDCORE_KERNEL_ANALYSIS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# zlib (deflate of 3MF packages): the system library natively. Emscripten has no system zlib
# and its zlib port is downloaded from the network at build time, so without a zlib for WASM
# (ZLIB_ROOT) the module is built without it: ZIP entries are stored and CRC-32 is the
# module's own implementation in every build (docs/kernel/ANALYSIS.md, "ZIP и zlib").
set(BENDCORE_ANALYSIS_HAS_ZLIB 0)
if(TARGET ZLIB::ZLIB)
  set(BENDCORE_ANALYSIS_HAS_ZLIB 1)
elseif(EMSCRIPTEN)
  find_package(ZLIB QUIET)
  if(ZLIB_FOUND)
    set(BENDCORE_ANALYSIS_HAS_ZLIB 1)
  else()
    message(STATUS "zlib not found for Emscripten - bendcore_kernel_analysis writes stored ZIP entries")
  endif()
else()
  find_package(ZLIB REQUIRED)
  set(BENDCORE_ANALYSIS_HAS_ZLIB 1)
endif()

add_library(bendcore_kernel_analysis STATIC
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/material.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/mass_properties.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/mesh_export.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/zip_archive.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/measure.cpp
  ${BENDCORE_KERNEL_ANALYSIS_ROOT}/src/kernel/analysis/section.cpp)
target_compile_features(bendcore_kernel_analysis PUBLIC cxx_std_20)
target_include_directories(bendcore_kernel_analysis PUBLIC "${BENDCORE_KERNEL_ANALYSIS_ROOT}/include")
# nlohmann/json.hpp (MIT) for the material JSON.
target_include_directories(bendcore_kernel_analysis SYSTEM PRIVATE "${BENDCORE_KERNEL_ANALYSIS_ROOT}/third_party")
target_link_libraries(bendcore_kernel_analysis PUBLIC bendcore_kernel)
target_compile_definitions(bendcore_kernel_analysis PRIVATE BENDCORE_ANALYSIS_HAS_ZLIB=${BENDCORE_ANALYSIS_HAS_ZLIB})
if(BENDCORE_ANALYSIS_HAS_ZLIB)
  target_link_libraries(bendcore_kernel_analysis PRIVATE ZLIB::ZLIB)
endif()

if(GTest_FOUND)
  # The 3MF test parses the model XML with expat (independent of the writer).
  find_package(EXPAT QUIET)
  set(analysis_tests materials mass_properties measure section zip_archive)
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
  # The CRC-32 and deflate checks compare against zlib itself where the build links it.
  target_compile_definitions(test_kernel_zip_archive PRIVATE BENDCORE_ANALYSIS_HAS_ZLIB=${BENDCORE_ANALYSIS_HAS_ZLIB})
  if(BENDCORE_ANALYSIS_HAS_ZLIB)
    target_link_libraries(test_kernel_zip_archive PRIVATE ZLIB::ZLIB)
  endif()
else()
  message(STATUS "GTest not found - bendcore_kernel_analysis tests skipped")
endif()
