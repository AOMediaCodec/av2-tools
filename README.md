# AV2 Tools

Tools for parsing, analyzing, manipulating, and packaging AV2 bitstreams.

## How to Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_PACKAGER=ON -DBUILD_EXAMPLES=ON
make -j
```

## Applications

- [**av2_obu_tool**](./apps/av2_obu_tool/) - Parse, dump, and analyze AV2 bitstreams (JSON export, statistics)
- [**av2_obu_switcher**](./apps/av2_obu_switcher/) - Bitstream switching experiments
- [**av2_obu_packager**](./apps/av2_obu_packager/) - Package AV2 into MP4 containers (requires `-DBUILD_PACKAGER=ON`)
- [**av2_obu_channel_sim**](./apps/av2_obu_channel_sim/) - Simulate packet loss and network conditions

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
- Dependencies (auto-fetched): 
  - [CLI11](https://github.com/CLIUtils/CLI11)
  - [spdlog](https://github.com/gabime/spdlog.git)
  - [nlohmann/json](https://github.com/nlohmann/json.git)
  - [libisomedia](https://github.com/MPEGGroup/isobmff.git)
  - Google Test

## License

See [LICENSE](LICENSE) file.
