# Решатель сопряжений сборки (docs/assembly/SOLVER.md). Подключается из корневого CMakeLists.txt
# через include(cmake/assembly.cmake OPTIONAL) внутри BENDCORE_WITH_OCCT. От OCCT не зависит.

if(NOT TARGET Eigen3::Eigen AND BENDCORE_EIGEN_INCLUDE MATCHES "third_party_stubs")
  message(WARNING "bendcore_assembly_v2: Eigen из third_party_stubs не содержит нужных модулей — модуль пропущен")
  return()
endif()

add_library(bendcore_assembly_v2 STATIC
  src/assembly_v2/geometry.cpp
  src/assembly_v2/mate.cpp
  src/assembly_v2/assembly.cpp
  src/assembly_v2/solver.cpp
)
target_include_directories(bendcore_assembly_v2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bendcore_assembly_v2 PUBLIC bendcore_eigen)
target_compile_features(bendcore_assembly_v2 PUBLIC cxx_std_20)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(WARNING "bendcore_assembly_v2: GoogleTest не найден — тесты tests/assembly_v2 не собираются")
  return()
endif()

foreach(_t jacobian solver performance)
  add_executable(test_assembly_v2_${_t} tests/assembly_v2/test_${_t}.cpp)
  target_link_libraries(test_assembly_v2_${_t} PRIVATE bendcore_assembly_v2 GTest::gtest GTest::gtest_main)
  bendcore_add_test(assembly_v2_${_t} test_assembly_v2_${_t})
endforeach()
