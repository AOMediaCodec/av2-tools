add_executable(av2_demux
  main.cpp
  av2_demuxer.cpp
)

target_link_libraries(av2_demux
  PRIVATE
    av2_obu
    CLI11::CLI11
    libisomediafile
)

target_compile_options(av2_demux PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

install(TARGETS av2_demux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
