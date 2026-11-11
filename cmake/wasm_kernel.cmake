# bendcore_kernel_wasm: the CAD kernel (bendcore_kernel) as an ES6 WebAssembly module for the
# browser viewport (docs/ui/KERNEL_VIEWPORT.md). Included from the BENDCORE_WITH_OCCT block of
# the root CMakeLists.txt after cmake/kernel_core.cmake; only meaningful under Emscripten with
# BENDCORE_OCCT_DIR pointing to the WASM OCCT install (third_party/build_occt_wasm.sh).
#
# Output directory ${CMAKE_BINARY_DIR}/wasm_kernel:
#   bendcore_kernel.mjs / bendcore_kernel.wasm  Emscripten glue (MODULARIZE, EXPORT_ES6) and binary
#   bendcore_kernel_api.mjs / .d.mts           JS API: handles, typed arrays, KernelError

if(NOT EMSCRIPTEN)
  return()
endif()
if(NOT TARGET bendcore_kernel)
  message(FATAL_ERROR "cmake/wasm_kernel.cmake needs bendcore_kernel (cmake/kernel_core.cmake)")
endif()

set(BENDCORE_WASM_KERNEL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(BENDCORE_WASM_KERNEL_OUT "${CMAKE_BINARY_DIR}/wasm_kernel")
# Link-time optimisation level of the module: Oz (smallest) by default, O2/O3 for speed.
set(BENDCORE_WASM_KERNEL_OPT "Oz" CACHE STRING "Optimisation level of bendcore_kernel_wasm (O2, O3, Os, Oz)")

add_executable(bendcore_kernel_wasm ${BENDCORE_WASM_KERNEL_ROOT}/src/wasm/kernel_bindings.cpp)
target_link_libraries(bendcore_kernel_wasm PRIVATE bendcore_kernel)
target_compile_options(bendcore_kernel_wasm PRIVATE -${BENDCORE_WASM_KERNEL_OPT})
set_target_properties(bendcore_kernel_wasm PROPERTIES
  OUTPUT_NAME bendcore_kernel
  SUFFIX ".mjs"
  RUNTIME_OUTPUT_DIRECTORY "${BENDCORE_WASM_KERNEL_OUT}")
target_link_options(bendcore_kernel_wasm PRIVATE
  -lembind --no-entry -${BENDCORE_WASM_KERNEL_OPT}
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcoreKernel
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
  -sSTACK_SIZE=4MB -sNO_EXIT_RUNTIME=1)

# The JS API sits next to the glue so that both the browser page and node import it from one place.
add_custom_command(TARGET bendcore_kernel_wasm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${BENDCORE_WASM_KERNEL_ROOT}/src/wasm/bendcore_kernel_api.mjs"
          "${BENDCORE_WASM_KERNEL_ROOT}/src/wasm/bendcore_kernel_api.d.mts"
          "${BENDCORE_WASM_KERNEL_OUT}/"
  VERBATIM)

# Corpus check under node: volumes, areas, bounding boxes, face counts and types against the
# formulas in tests/corpus/generated/manifest.json, plus tessellation invariants and errors.
if(BENDCORE_NODE)
  add_test(NAME wasm_kernel_corpus
    COMMAND ${BENDCORE_NODE} "${BENDCORE_WASM_KERNEL_ROOT}/tests/wasm/kernel_corpus.test.mjs"
            --module "${BENDCORE_WASM_KERNEL_OUT}"
            --corpus "${BENDCORE_WASM_KERNEL_ROOT}/tests/corpus/generated")
  set_tests_properties(wasm_kernel_corpus PROPERTIES TIMEOUT 300)
else()
  message(STATUS "node not found - wasm_kernel_corpus test skipped")
endif()

# Part document module (bendcore_modeling_wasm, docs/ui/PART_MODELING_UI.md).
include(${CMAKE_CURRENT_LIST_DIR}/wasm_modeling.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/wasm_assembly.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/wasm_drawing.cmake OPTIONAL)  # bendcore_drawing_wasm, docs/ui/DRAWING_UI.md
