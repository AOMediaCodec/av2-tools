cmake_minimum_required(VERSION 3.16)
project(obu_tool VERSION 1.0.0 LANGUAGES CXX)

# ---- Global settings ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Dependencies ----
# find_package(OpenSSL REQUIRED) # for hashing

include(FetchContent)

# CLI11 (header-only)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.5.0
)

# spdlog (logging library)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.16.0
)

# nlohmann/json (JSON library, header-only)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.12.0
)

FetchContent_MakeAvailable(cli11 spdlog nlohmann_json)

# ---- Library (core parser) ----
add_library(av2_obu STATIC)

target_sources(av2_obu
  PRIVATE
    # Core parsing infrastructure
    src/core/obu_parser.cpp
    src/core/base_obu.cpp
    src/core/bitstream_reader.cpp
    src/core/av2_sequence_header.cpp
    src/core/av2_types.cpp
    src/core/temporal_unit.cpp

    # Specific OBU implementations
    src/obus/sequence_header_obu.cpp
    src/obus/temporal_delimiter_obu.cpp
    src/obus/multi_frame_header_obu.cpp
    src/obus/clk_obu.cpp
    src/obus/olk_obu.cpp
    src/obus/leading_tile_group_obu.cpp
    src/obus/regular_tile_group_obu.cpp
    src/obus/tile_group_obu.cpp
    src/obus/metadata_obu.cpp
    src/obus/metadata_group_obu.cpp
    src/obus/metadata_unit.cpp
    src/obus/switch_obu.cpp
    src/obus/leading_sef_obu.cpp
    src/obus/regular_sef_obu.cpp
    src/obus/leading_tip_obu.cpp
    src/obus/regular_tip_obu.cpp
    src/obus/buffer_removal_timing_obu.cpp
    src/obus/layer_configuration_record_obu.cpp
    src/obus/atlas_segment_obu.cpp
    src/obus/operating_point_set_obu.cpp
    src/obus/bridge_frame_obu.cpp
    src/obus/msdo_obu.cpp
    src/obus/ras_frame_obu.cpp
    src/obus/qm_obu.cpp
    src/obus/fgm_obu.cpp
    src/obus/content_interpretation_obu.cpp
    src/obus/padding_obu.cpp
    src/obus/unknown_obu.cpp
  PUBLIC
    # Master header
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/av2_obu.h>

    # Core headers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/obu_parser.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/base_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/bitstream_reader.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/av2_sequence_header.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/av2_types.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/temporal_unit.h>

    # OBU headers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/sequence_header_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/temporal_delimiter_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/multi_frame_header_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/clk_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/olk_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/leading_tile_group_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/regular_tile_group_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/tile_group_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/metadata_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/metadata_group_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/metadata_unit.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/switch_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/leading_sef_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/regular_sef_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/leading_tip_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/regular_tip_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/buffer_removal_timing_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/layer_configuration_record_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/atlas_segment_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/operating_point_set_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/bridge_frame_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/msdo_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/ras_frame_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/qm_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/fgm_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/content_interpretation_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/padding_obu.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/obus/unknown_obu.h>
)

# Public headers are in include/, private headers (if any) in src/
target_include_directories(av2_obu
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(av2_obu
  PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    # OpenSSL::SSL
    # OpenSSL::Crypto
)

target_compile_options(av2_obu PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# If this static lib might be linked into a shared lib elsewhere
set_target_properties(av2_obu PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- Applications ----
add_subdirectory(apps/av2_obu_tool)
add_subdirectory(apps/av2_obu_switcher)

# av2_obu_packager - Optional (requires libisomedia)
option(BUILD_PACKAGER "Build av2_obu_packager with libisomedia support" OFF)
if(BUILD_PACKAGER)
  message(STATUS "Building av2_obu_packager with libisomedia")

  # Fetch libisomedia from MPEG Group repo (pcm_sniffer branch)
  FetchContent_Declare(
    libisomedia
    GIT_REPOSITORY https://github.com/MPEGGroup/isobmff.git
    GIT_TAG        origin/pcm_sniffer
    GIT_SHALLOW    FALSE  # Disable shallow clone for branch
    GIT_SUBMODULES ""     # Don't initialize any submodules during fetch
    UPDATE_DISCONNECTED TRUE  # Don't update on every build
  )

  # Configure isobmff to build library only (no submodules, no examples)
  set(ISOBMFF_BUILD_LIB_ONLY ON CACHE BOOL "Build only libisomediafile" FORCE)

  # Make libisomedia available
  FetchContent_MakeAvailable(libisomedia)

  add_subdirectory(apps/av2_obu_packager)
else()
  message(STATUS "Skipping av2_obu_packager (use -DBUILD_PACKAGER=ON to enable)")
endif()

add_subdirectory(apps/av2_obu_channel_sim)

# ---- Install rules ----
include(GNUInstallDirs)

install(TARGETS av2_obu
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

# ---- Tests ----
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---- Examples (optional) ----
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
