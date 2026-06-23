add_executable(av2_mux
  main.cpp
  mux_strategy.cpp
  av2_codec_config.cpp
  mp4_writer.cpp
  av2_muxer.cpp
  display_order_lifter.cpp
  frame_header_helpers.cpp
  colr_info.cpp
  mux_log.cpp
  ref_frame_buffer.cpp
)

target_link_libraries(av2_mux
  PRIVATE
    av2_obu       # Brings spdlog and nlohmann_json transitively
    CLI11::CLI11
    libisomediafile   # ISOBMFF reference SW from MPEG
)

target_compile_options(av2_mux PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

install(TARGETS av2_mux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
