add_executable(av2_obu_packager
  main.cpp
  packaging_strategy.cpp
  temporal_unit_builder.cpp
)

target_link_libraries(av2_obu_packager
  PRIVATE
    av2_obu       # Brings spdlog and nlohmann_json transitively
    CLI11::CLI11
    libisomediafile   # ISOBMFF reference SW from MPEG
)

target_compile_options(av2_obu_packager PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Install the executable
install(TARGETS av2_obu_packager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
