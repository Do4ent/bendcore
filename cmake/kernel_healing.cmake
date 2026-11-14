# Лечение геометрии и устойчивые булевы операции (docs/kernel/GEOMETRY_HEALING.md).
# Подключается из корневого CMakeLists.txt внутри if(BENDCORE_WITH_OCCT).

add_library(bendcore_kernel_healing STATIC
  src/kernel/healing.cpp
  src/kernel/boolean.cpp)
target_include_directories(bendcore_kernel_healing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Заголовки ядра включают заголовки OCCT (TopoDS_Shape), поэтому путь к ним публичный.
target_include_directories(bendcore_kernel_healing SYSTEM PUBLIC "${BENDCORE_OCCT_DIR}/include/opencascade")
target_link_libraries(bendcore_kernel_healing PUBLIC
  -Wl,--start-group ${BENDCORE_OCCT_LIBS} -Wl,--end-group pthread dl m)

if(GTest_FOUND)
  add_executable(test_kernel_healing tests/kernel/test_healing.cpp)
  target_link_libraries(test_kernel_healing PRIVATE bendcore_kernel_healing GTest::gtest GTest::gtest_main)
  bendcore_add_test(kernel_healing test_kernel_healing)

  add_executable(test_kernel_boolean tests/kernel/test_boolean.cpp)
  target_link_libraries(test_kernel_boolean PRIVATE bendcore_kernel_healing GTest::gtest GTest::gtest_main)
  bendcore_add_test(kernel_boolean test_kernel_boolean)
else()
  message(STATUS "GTest not found - tests for bendcore_kernel_healing skipped")
endif()
