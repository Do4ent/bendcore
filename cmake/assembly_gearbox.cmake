foreach(_dep bendcore_drawing_document bendcore_fasteners)
  if(NOT TARGET ${_dep})
    return()
  endif()
endforeach()
add_library(bendcore_gearbox_example STATIC
  examples/gearbox/gearbox_support.cpp
  examples/gearbox/gearbox_parts.cpp
  examples/gearbox/gearbox_assembly.cpp
)
target_include_directories(bendcore_gearbox_example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/examples/gearbox)
target_link_libraries(bendcore_gearbox_example PUBLIC bendcore_drawing_document bendcore_fasteners)
target_compile_features(bendcore_gearbox_example PUBLIC cxx_std_20)
target_compile_options(bendcore_gearbox_example PRIVATE -Wall -Wextra)
add_executable(gearbox_dev /tmp/claude-0/-home-user-bendcore/2c4413cd-9c6b-5dc9-b2b2-411f82498631/scratchpad/dev.cpp)
target_link_libraries(gearbox_dev PRIVATE bendcore_gearbox_example)
