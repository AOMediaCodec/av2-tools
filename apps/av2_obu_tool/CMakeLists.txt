add_executable(av2_obu_tool main.cpp)

target_link_libraries(av2_obu_tool
  PRIVATE
    av2_obu       # Brings spdlog and nlohmann_json transitively
    CLI11::CLI11
)

target_compile_options(av2_obu_tool PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Install the executable
install(TARGETS av2_obu_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
