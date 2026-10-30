cmake_minimum_required(VERSION 3.20)

# Add acceptance tests
enable_testing()

# Find Google Test
find_package(GTest REQUIRED)

# Acceptance test source files
set(ACCEPT_TEST_SOURCES
    ACCEPT-001-SketchExtrude.test.cpp
    ACCEPT-002-AssemblyConstraints.test.cpp
    ACCEPT-008-UndoRedo.test.cpp
)

# Create individual test executables
foreach(test_file ${ACCEPT_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name}
        PRIVATE
        bendcore_core
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(${test_name}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# Master acceptance test runner
add_executable(acceptance_test_runner
    AcceptanceTestRunner.cpp
    ACCEPT-001-SketchExtrude.test.cpp
    ACCEPT-002-AssemblyConstraints.test.cpp
    ACCEPT-008-UndoRedo.test.cpp
)

target_link_libraries(acceptance_test_runner
    PRIVATE
    bendcore_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(acceptance_test_runner
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Add the runner as a test
add_test(NAME acceptance_tests COMMAND acceptance_test_runner)

# Performance benchmarks (optional)
add_executable(acceptance_benchmarks
    AcceptanceBenchmarks.cpp
)

target_link_libraries(acceptance_benchmarks
    PRIVATE
    bendcore_core
)

target_include_directories(acceptance_benchmarks
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
