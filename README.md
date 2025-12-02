# AV2 Tools

Toolkit for parsing, analyzing, manipulating, and packaging AV2 bitstreams.

## Quick Start

```bash
# Build (parallel compilation with -j)
cmake -S . -B mybuild && cmake --build mybuild -j

# Parse and dump OBUs
./mybuild/apps/av2_obu_tool/av2_obu_tool dump <file.bin>

# Export to JSON
./mybuild/apps/av2_obu_tool/av2_obu_tool -j dump <file.bin> -o output.json

# Show statistics
./mybuild/apps/av2_obu_tool/av2_obu_tool stats <file.bin>
```

## Build Options

Configure with CMake options to enable additional features:

```bash
# Enable examples
cmake -S . -B mybuild -DBUILD_EXAMPLES=ON

# Enable packager (will fetch MPEG's libisomedia)
cmake -S . -B mybuild -DBUILD_PACKAGER=ON

# Disable tests
cmake -S . -B mybuild -DBUILD_TESTS=OFF

# Combine options
cmake -S . -B mybuild -DBUILD_EXAMPLES=ON -DBUILD_PACKAGER=ON

# Then build
cmake --build mybuild -j
```

## Applications

- **av2_obu_tool** - Parse, dump, and analyze AV2 bitstreams (JSON export, statistics)
- **av2_obu_switcher** - Bitstream switching experiments
- **av2_obu_packager** - Package AV2 into MP4 containers (requires `-DBUILD_PACKAGER=ON`)
- **av2_obu_channel_sim** - Simulate packet loss and network conditions

## Library

The project builds `libav2_obu.a`, a reusable C++ library for parsing AV2 OBU bitstreams.

### Usage Example

```cpp
#include <av2_obu/av2_obu.h>

using namespace av2_obu;

OBUParser parser;
if (parser.parse_file("bitstream.bin")) {
    auto json = parser.to_json();
    std::cout << json.dump(2);
}
```

## Requirements

- CMake 3.16+
- C++17 compiler
- Dependencies auto-fetched: CLI11, spdlog, nlohmann/json, libisomedia, Google Test

## License

See [LICENSE](LICENSE) file.
