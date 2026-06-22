cmake_minimum_required(VERSION 3.16)
project(av2_obu VERSION 0.1.0 LANGUAGES CXX)

# ---- Versioning ----
include(cmake/GitVersion.cmake)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/av2_obu/version.h @ONLY)
configure_file(cmake/web_version.ts.in ${CMAKE_SOURCE_DIR}/web/src/generated/version.ts @ONLY)

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
    src/core/bitstream_writer.cpp
    src/core/av2_sequence_header.cpp
    src/core/av2_types.cpp
    src/core/temporal_unit.cpp
    src/core/frame_header_info.cpp
    src/core/tile_group_header.cpp

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/av2_obu/core/bitstream_writer.h>
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

# Public headers are in include/, generated headers in build dir, private headers in src/
target_include_directories(av2_obu
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Alias so FetchContent/add_subdirectory consumers can use av2_obu::av2_obu
# (same target name as find_package consumers get via install NAMESPACE)
add_library(av2_obu::av2_obu ALIAS av2_obu)

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

# Container tools (av2_mux + av2_demux) - Optional (require libisomedia).
# Legacy alias BUILD_PACKAGER is honoured to keep old build invocations working.
option(BUILD_CONTAINER_TOOLS "Build av2_mux / av2_demux with libisomedia support" OFF)
if(BUILD_PACKAGER AND NOT BUILD_CONTAINER_TOOLS)
  message(STATUS "BUILD_PACKAGER is deprecated; treating as BUILD_CONTAINER_TOOLS=ON")
  set(BUILD_CONTAINER_TOOLS ON)
endif()
if(BUILD_CONTAINER_TOOLS)
  message(STATUS "Building container tools (av2_mux / av2_demux) with libisomedia")

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

  add_subdirectory(apps/av2_mux)
  add_subdirectory(apps/av2_demux)
else()
  message(STATUS "Skipping container tools (use -DBUILD_CONTAINER_TOOLS=ON to enable)")
endif()

add_subdirectory(apps/av2_obu_channel_sim)
add_subdirectory(apps/av1_obu_tool)

# ---- Install rules (not for WASM/Emscripten builds) ----
if(NOT EMSCRIPTEN)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(TARGETS av2_obu spdlog nlohmann_json
    EXPORT av2_obuTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
          FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
  install(FILES ${CMAKE_BINARY_DIR}/generated/av2_obu/version.h
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/av2_obu)

  # Export targets for find_package consumers
  install(EXPORT av2_obuTargets
    NAMESPACE av2_obu::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/av2_obu
  )

  # Package config file (relocatable)
  configure_package_config_file(
    cmake/av2_obuConfig.cmake.in
    ${CMAKE_BINARY_DIR}/av2_obuConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/av2_obu
  )

  # Package version file
  write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/av2_obuConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion
  )

  install(FILES
    ${CMAKE_BINARY_DIR}/av2_obuConfig.cmake
    ${CMAKE_BINARY_DIR}/av2_obuConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/av2_obu
  )
endif()

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

# ---- WebAssembly target ----
option(BUILD_WASM "Build WebAssembly target (requires Emscripten)" OFF)
if(BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "BUILD_WASM requires Emscripten. Use: emcmake cmake -DBUILD_WASM=ON ...")
  endif()
  add_subdirectory(src/wasm)
endif()
