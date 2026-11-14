# bendcore_assembly_wasm: the assembly document (bendcore_assembly_document with undo/redo,
# mates, solver, interference, BOM, exploded views, STEP) and the fastener library
# (bendcore_fasteners) as an ES6 WebAssembly module for the assembly UI (docs/ui/ASSEMBLY_UI.md).
# Included from cmake/wasm_kernel.cmake (Emscripten only).
#
# Output directory ${CMAKE_BINARY_DIR}/wasm_assembly:
#   bendcore_assembly.mjs / bendcore_assembly.wasm   Emscripten glue (MODULARIZE, EXPORT_ES6) and binary
#   bendcore_assembly_api.mjs / .d.mts              JS API: documents by handle, KernelError
#   bendcore_kernel_api.mjs / .d.mts                shared helpers (KernelError, faceOfTriangle)

if(NOT EMSCRIPTEN)
  return()
endif()
if(NOT TARGET bendcore_fasteners)
  message(WARNING "cmake/wasm_assembly.cmake: bendcore_fasteners is not defined - bendcore_assembly_wasm skipped")
  return()
endif()

set(BENDCORE_WASM_ASSEMBLY_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(BENDCORE_WASM_ASSEMBLY_OUT "${CMAKE_BINARY_DIR}/wasm_assembly")
set(BENDCORE_WASM_ASSEMBLY_OPT "Oz" CACHE STRING "Optimisation level of bendcore_assembly_wasm (O2, O3, Os, Oz)")

add_executable(bendcore_assembly_wasm ${BENDCORE_WASM_ASSEMBLY_ROOT}/src/wasm/assembly_bindings.cpp)
target_link_libraries(bendcore_assembly_wasm PRIVATE bendcore_fasteners bendcore_kernel_analysis)  # IGES/3MF, mass, docs/ui/DATA_EXCHANGE.md
target_compile_options(bendcore_assembly_wasm PRIVATE -${BENDCORE_WASM_ASSEMBLY_OPT})
set_target_properties(bendcore_assembly_wasm PROPERTIES
  OUTPUT_NAME bendcore_assembly
  SUFFIX ".mjs"
  RUNTIME_OUTPUT_DIRECTORY "${BENDCORE_WASM_ASSEMBLY_OUT}")
# Booleans of the interference check and of part documents recurse deeply: 8 MB stack.
target_link_options(bendcore_assembly_wasm PRIVATE
  -lembind --no-entry -${BENDCORE_WASM_ASSEMBLY_OPT}
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcoreAssembly
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB
  -sSTACK_SIZE=8MB -sNO_EXIT_RUNTIME=1)

add_custom_command(TARGET bendcore_assembly_wasm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${BENDCORE_WASM_ASSEMBLY_ROOT}/src/wasm/bendcore_assembly_api.mjs"
          "${BENDCORE_WASM_ASSEMBLY_ROOT}/src/wasm/bendcore_assembly_api.d.mts"
          "${BENDCORE_WASM_ASSEMBLY_ROOT}/src/wasm/bendcore_kernel_api.mjs"
          "${BENDCORE_WASM_ASSEMBLY_ROOT}/src/wasm/bendcore_kernel_api.d.mts"
          "${BENDCORE_WASM_ASSEMBLY_OUT}/"
  VERBATIM)

# Assembly API under node: shaft in a bushing and a plate with bolts against formulas.
if(BENDCORE_NODE)
  add_test(NAME wasm_assembly_document
    COMMAND ${BENDCORE_NODE} "${BENDCORE_WASM_ASSEMBLY_ROOT}/tests/wasm/assembly_document.test.mjs"
            --module "${BENDCORE_WASM_ASSEMBLY_OUT}")
  set_tests_properties(wasm_assembly_document PROPERTIES TIMEOUT 600)
endif()
