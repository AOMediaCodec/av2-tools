# AV2 Tools

Tools developed within the AOMedia Storage and Transport Formats (STF) Working Group for parsing, analyzing, manipulating, and packaging AV2 bitstreams.

## How to Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_PACKAGER=ON -DBUILD_EXAMPLES=ON
make -j
```

## Library

The project builds `libav2_obu.a`, a reusable C++ library for parsing AV2 OBU bitstreams.

## Applications

The project builds several applications that use the `libav2_obu.a`:

- [**av2_obu_tool**](./apps/av2_obu_tool/) - Parse, dump, and analyze AV2 bitstreams (JSON export, statistics)
- [**av2_obu_switcher**](./apps/av2_obu_switcher/) - Bitstream switching experiments
- [**av2_obu_packager**](./apps/av2_obu_packager/) - Package AV2 into MP4 containers (requires `-DBUILD_PACKAGER=ON`)
- [**av2_obu_channel_sim**](./apps/av2_obu_channel_sim/) - Simulate packet loss and network conditions

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
