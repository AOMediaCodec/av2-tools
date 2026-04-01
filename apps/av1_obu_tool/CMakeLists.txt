add_executable(av1_obu_tool main.cpp)

target_link_libraries(av1_obu_tool
  PRIVATE
    CLI11::CLI11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Optional MP4 input support (requires libisomedia, enabled via BUILD_PACKAGER)
if(TARGET libisomediafile)
  target_link_libraries(av1_obu_tool PRIVATE libisomediafile)
  target_compile_definitions(av1_obu_tool PRIVATE AV1_TOOL_WITH_MP4)
  message(STATUS "av1_obu_tool: MP4 input enabled")
endif()

target_compile_options(av1_obu_tool PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

install(TARGETS av1_obu_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
