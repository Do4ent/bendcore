# bendcore_modeling_wasm: the part document (bendcore_modeling_document: feature tree, sketches,
# operations, topological naming, undo/redo, JSON) as an ES6 WebAssembly module for the part
# modeling UI (docs/ui/PART_MODELING_UI.md). Included from cmake/wasm_kernel.cmake (Emscripten only).
#
# Output directory ${CMAKE_BINARY_DIR}/wasm_modeling:
#   bendcore_modeling.mjs / bendcore_modeling.wasm   Emscripten glue (MODULARIZE, EXPORT_ES6) and binary
#   bendcore_modeling_api.mjs / .d.mts              JS API: documents by handle, KernelError
#   bendcore_kernel_api.mjs / .d.mts                shared helpers (KernelError, faceOfTriangle)

if(NOT EMSCRIPTEN)
  return()
endif()
if(NOT TARGET bendcore_modeling_document)
  message(WARNING "cmake/wasm_modeling.cmake: bendcore_modeling_document is not defined - "
                  "bendcore_modeling_wasm skipped")
  return()
endif()

set(BENDCORE_WASM_MODELING_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(BENDCORE_WASM_MODELING_OUT "${CMAKE_BINARY_DIR}/wasm_modeling")
set(BENDCORE_WASM_MODELING_OPT "Oz" CACHE STRING "Optimisation level of bendcore_modeling_wasm (O2, O3, Os, Oz)")

# sketch_editor_bindings.cpp: solve and editing tools of the sketch editor (docs/ui/SKETCH_EDITOR.md).
add_executable(bendcore_modeling_wasm
  ${BENDCORE_WASM_MODELING_ROOT}/src/wasm/modeling_bindings.cpp
  ${BENDCORE_WASM_MODELING_ROOT}/src/wasm/sketch_editor_bindings.cpp)
target_link_libraries(bendcore_modeling_wasm PRIVATE bendcore_modeling_document bendcore_modeling_sketch bendcore_formula
  bendcore_kernel_xcaf bendcore_kernel_analysis)  # export and mass properties, docs/ui/DATA_EXCHANGE.md
target_compile_options(bendcore_modeling_wasm PRIVATE -${BENDCORE_WASM_MODELING_OPT})
set_target_properties(bendcore_modeling_wasm PROPERTIES
  OUTPUT_NAME bendcore_modeling
  SUFFIX ".mjs"
  RUNTIME_OUTPUT_DIRECTORY "${BENDCORE_WASM_MODELING_OUT}")
# Booleans, fillets and offsets recurse deeper than STEP reading: 8 MB stack.
target_link_options(bendcore_modeling_wasm PRIVATE
  -lembind --no-entry -${BENDCORE_WASM_MODELING_OPT}
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcoreModeling
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB
  -sSTACK_SIZE=8MB -sNO_EXIT_RUNTIME=1)

add_custom_command(TARGET bendcore_modeling_wasm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${BENDCORE_WASM_MODELING_ROOT}/src/wasm/bendcore_modeling_api.mjs"
          "${BENDCORE_WASM_MODELING_ROOT}/src/wasm/bendcore_modeling_api.d.mts"
          "${BENDCORE_WASM_MODELING_ROOT}/src/wasm/bendcore_kernel_api.mjs"
          "${BENDCORE_WASM_MODELING_ROOT}/src/wasm/bendcore_kernel_api.d.mts"
          "${BENDCORE_WASM_MODELING_OUT}/"
  VERBATIM)

# Part document API under node: three reference parts (docs/modeling/REFERENCE_PARTS.md) against
# their analytic volumes, undo/redo, save/load, errors.
if(BENDCORE_NODE)
  add_test(NAME wasm_modeling_document
    COMMAND ${BENDCORE_NODE} "${BENDCORE_WASM_MODELING_ROOT}/tests/wasm/modeling_document.test.mjs"
            --module "${BENDCORE_WASM_MODELING_OUT}")
  set_tests_properties(wasm_modeling_document PROPERTIES TIMEOUT 600)
  # Sketch editor API (docs/ui/SKETCH_EDITOR.md): definition, reference dimensions, solve, tools.
  add_test(NAME wasm_sketch_editor
    COMMAND ${BENDCORE_NODE} "${BENDCORE_WASM_MODELING_ROOT}/tests/wasm/sketch_editor.test.mjs"
            --module "${BENDCORE_WASM_MODELING_OUT}")
  set_tests_properties(wasm_sketch_editor PROPERTIES TIMEOUT 300)
endif()
