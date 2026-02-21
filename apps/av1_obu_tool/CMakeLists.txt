add_executable(av1_obu_tool main.cpp)

target_link_libraries(av1_obu_tool
  PRIVATE
    CLI11::CLI11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_compile_options(av1_obu_tool PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

install(TARGETS av1_obu_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
