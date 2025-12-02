add_executable(av2_obu_channel_sim
  main.cpp
  channel_simulator.cpp
  bit_corruptor.cpp
  packet_loss_simulator.cpp
)

target_link_libraries(av2_obu_channel_sim
  PRIVATE
    av2_obu       # Brings spdlog and nlohmann_json transitively
    CLI11::CLI11
)

target_compile_options(av2_obu_channel_sim PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Install the executable
install(TARGETS av2_obu_channel_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
