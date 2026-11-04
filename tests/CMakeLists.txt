# Use FetchContent to get Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Core tests
add_executable(test_obu_parser
  core/test_obu_parser.cpp
)

# Link libraries
target_link_libraries(test_obu_parser
  PRIVATE
    av2_obu
    gtest_main
    gtest
)

# Include directories
target_include_directories(test_obu_parser
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Add test to CTest
add_test(NAME OBUParserTests COMMAND test_obu_parser)

# Set C++ standard
set_property(TARGET test_obu_parser PROPERTY CXX_STANDARD 17)
set_property(TARGET test_obu_parser PROPERTY CXX_STANDARD_REQUIRED ON)

# Compiler flags for tests
target_compile_options(test_obu_parser PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -g -O0>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /Od>
)

if(BUILD_CONTAINER_TOOLS)
  add_executable(test_av2_isobmff
    core/test_av2_isobmff.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/av2_codec_config.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/av2_muxer.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/mp4_writer.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/mux_strategy.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/display_order_lifter.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/frame_header_helpers.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/colr_info.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/mux_log.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_mux/ref_frame_buffer.cpp
    ${CMAKE_SOURCE_DIR}/apps/av2_demux/av2_demuxer.cpp
  )

  target_link_libraries(test_av2_isobmff
    PRIVATE
      av2_obu
      libisomediafile
      gtest_main
      gtest
  )

  target_include_directories(test_av2_isobmff
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/apps/av2_mux
      ${CMAKE_SOURCE_DIR}/apps/av2_demux
  )

  add_test(NAME AV2ISOBMFFTests COMMAND test_av2_isobmff)

  set_property(TARGET test_av2_isobmff PROPERTY CXX_STANDARD 17)
  set_property(TARGET test_av2_isobmff PROPERTY CXX_STANDARD_REQUIRED ON)

  target_compile_options(test_av2_isobmff PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -g -O0>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /Od>
  )
endif()
