# WebAssembly target for browser-based AV2 analysis

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "WASM target requires Emscripten. Use: emcmake cmake -DBUILD_WASM=ON ...")
endif()

# Create WASM library
add_executable(av2_wasm
  av2_wasm_api.cpp
)

target_link_libraries(av2_wasm
  PRIVATE av2_obu
)

# Emscripten-specific settings
set_target_properties(av2_wasm PROPERTIES
  OUTPUT_NAME "av2-parser"
  SUFFIX ".js"
)

# Export functions and configure WASM settings
target_link_options(av2_wasm PRIVATE
  "SHELL:-s EXPORTED_FUNCTIONS=['_av2_parse_to_json','_av2_free_json','_av2_get_last_error','_malloc','_free']"
  "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPU8']"
  "SHELL:-s MODULARIZE=1"
  "SHELL:-s EXPORT_NAME='createAV2Module'"
  "SHELL:-s ALLOW_MEMORY_GROWTH=1"
  "SHELL:-s INITIAL_MEMORY=32MB"
  "SHELL:-s ENVIRONMENT=web"
  "SHELL:-s FILESYSTEM=1"
  "SHELL:-s ASSERTIONS=1"
  -O3
)

# Install WASM artifacts to web/public/wasm
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/av2-parser.js
  ${CMAKE_CURRENT_BINARY_DIR}/av2-parser.wasm
  DESTINATION ${CMAKE_SOURCE_DIR}/web/public/wasm
)

message(STATUS "WASM target configured. Build artifacts: av2-parser.js, av2-parser.wasm")
message(STATUS "After build, run: cmake --install . (copies to web/public/wasm)")
