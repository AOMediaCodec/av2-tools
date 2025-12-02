# CMakeLists.txt for examples
# These are optional demonstration programs showing library usage

add_executable(simple_dump simple_dump.cpp)

target_link_libraries(simple_dump
  PRIVATE
    av2_obu
)

target_compile_options(simple_dump PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
