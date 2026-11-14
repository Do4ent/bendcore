# bendcore_drawing_wasm: the drawing document (bendcore_drawing_document: sheets, views, HLR,
# sections, dimensions, annotations, specification, PDF/DXF/SVG export) together with the part and
# assembly documents it is made from, as an ES6 WebAssembly module for the drawing UI
# (docs/ui/DRAWING_UI.md). Included from cmake/wasm_kernel.cmake (Emscripten only).
#
# Output directory ${CMAKE_BINARY_DIR}/wasm_drawing:
#   bendcore_drawing.mjs / bendcore_drawing.wasm   Emscripten glue (MODULARIZE, EXPORT_ES6) and binary
#   bendcore_drawing_api.mjs / .d.mts             JS API: drawings by handle, KernelError
#   bendcore_kernel_api.mjs / .d.mts              shared helpers (KernelError)
#   DejaVuSans.ttf, DejaVuSans-LICENSE.txt         font passed to the module by the page (setFont)

if(NOT EMSCRIPTEN)
  return()
endif()
if(NOT TARGET bendcore_drawing_document)
  message(WARNING "cmake/wasm_drawing.cmake: bendcore_drawing_document is not defined - "
                  "bendcore_drawing_wasm skipped")
  return()
endif()

set(BENDCORE_WASM_DRAWING_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(BENDCORE_WASM_DRAWING_OUT "${CMAKE_BINARY_DIR}/wasm_drawing")
set(BENDCORE_WASM_DRAWING_OPT "Oz" CACHE STRING "Optimisation level of bendcore_drawing_wasm (O2, O3, Os, Oz)")

# The module has no file system: the page fetches a TrueType font and passes its bytes to
# setFont(). DejaVu Sans (Bitstream Vera license + public domain changes: redistribution allowed)
# is copied from the build host next to the module together with its license text.
find_file(BENDCORE_DRAWING_FONT_FILE DejaVuSans.ttf
  PATHS /usr/share/fonts/truetype/dejavu /usr/share/fonts/dejavu /usr/share/fonts/TTF
        /usr/local/share/fonts /opt/homebrew/share/fonts
  NO_CMAKE_FIND_ROOT_PATH
  DOC "TrueType font served next to bendcore_drawing_wasm (DejaVu Sans)")
find_file(BENDCORE_DRAWING_FONT_LICENSE copyright
  PATHS /usr/share/doc/fonts-dejavu-core /usr/share/doc/fonts-dejavu
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
  DOC "License text of the font (Bitstream Vera / DejaVu)")

add_executable(bendcore_drawing_wasm ${BENDCORE_WASM_DRAWING_ROOT}/src/wasm/drawing_bindings.cpp)
target_link_libraries(bendcore_drawing_wasm PRIVATE bendcore_drawing_document)
# nlohmann/json (MIT): JSON text across the JS boundary.
target_include_directories(bendcore_drawing_wasm PRIVATE ${BENDCORE_WASM_DRAWING_ROOT}/third_party)
target_compile_options(bendcore_drawing_wasm PRIVATE -${BENDCORE_WASM_DRAWING_OPT})
set_target_properties(bendcore_drawing_wasm PROPERTIES
  OUTPUT_NAME bendcore_drawing
  SUFFIX ".mjs"
  RUNTIME_OUTPUT_DIRECTORY "${BENDCORE_WASM_DRAWING_OUT}")
# HLR, sections (booleans) and part rebuilds recurse deeply: 8 MB stack, as bendcore_modeling_wasm.
target_link_options(bendcore_drawing_wasm PRIVATE
  -lembind --no-entry -${BENDCORE_WASM_DRAWING_OPT}
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createBendcoreDrawing
  -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB
  -sSTACK_SIZE=8MB -sNO_EXIT_RUNTIME=1)

add_custom_command(TARGET bendcore_drawing_wasm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${BENDCORE_WASM_DRAWING_ROOT}/src/wasm/bendcore_drawing_api.mjs"
          "${BENDCORE_WASM_DRAWING_ROOT}/src/wasm/bendcore_drawing_api.d.mts"
          "${BENDCORE_WASM_DRAWING_ROOT}/src/wasm/bendcore_kernel_api.mjs"
          "${BENDCORE_WASM_DRAWING_ROOT}/src/wasm/bendcore_kernel_api.d.mts"
          "${BENDCORE_WASM_DRAWING_OUT}/"
  VERBATIM)
if(BENDCORE_DRAWING_FONT_FILE)
  add_custom_command(TARGET bendcore_drawing_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BENDCORE_DRAWING_FONT_FILE}"
            "${BENDCORE_WASM_DRAWING_OUT}/DejaVuSans.ttf"
    VERBATIM)
  if(BENDCORE_DRAWING_FONT_LICENSE)
    add_custom_command(TARGET bendcore_drawing_wasm POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${BENDCORE_DRAWING_FONT_LICENSE}"
              "${BENDCORE_WASM_DRAWING_OUT}/DejaVuSans-LICENSE.txt"
      VERBATIM)
  endif()
else()
  message(WARNING "bendcore_drawing_wasm: DejaVuSans.ttf not found - set BENDCORE_DRAWING_FONT_FILE; "
                  "the drawing page and the node test need a TrueType font next to the module")
endif()

# Drawing of the reference bracket through the JS API under node: views, dimensions against the
# model parameters, update after a parameter change, PDF/DXF/SVG export, save/load, errors.
if(BENDCORE_NODE)
  add_test(NAME wasm_drawing_document
    COMMAND ${BENDCORE_NODE} "${BENDCORE_WASM_DRAWING_ROOT}/tests/wasm/drawing_document.test.mjs"
            --module "${BENDCORE_WASM_DRAWING_OUT}")
  set_tests_properties(wasm_drawing_document PROPERTIES TIMEOUT 600)
endif()
