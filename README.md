# AV2 Tools

Tools developed within the AOMedia Storage and Transport Formats (STF) Working Group for parsing, analyzing, manipulating, and packaging AV2 bitstreams.

## Related repositories

- [av2-isobmff](https://github.com/AOMediaCodec/av2-isobmff) - the AV2-in-ISOBMFF specification these tools implement.
- [av2-interop-private](https://github.com/AOMediaCodec/av2-interop-private) - AV2 interop stream recipes and catalog that consume these tools.

## How to Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_CONTAINER_TOOLS=ON -DBUILD_EXAMPLES=ON
make -j
```

## Library

The project builds `libav2_obu.a`, a reusable C++ library for parsing AV2 OBU bitstreams.

## Applications

The project builds several applications that use the `libav2_obu.a`:

- [**av2_obu_tool**](./apps/av2_obu_tool/) - Parse, dump, and analyze AV2 bitstreams (JSON export, statistics)
- [**av2_obu_switcher**](./apps/av2_obu_switcher/) - Bitstream switching experiments
- [**av2_mux**](./apps/av2_mux/) - Mux AV2 elementary stream into MP4/ISOBMFF (requires `-DBUILD_CONTAINER_TOOLS=ON`)
- [**av2_demux**](./apps/av2_demux/) - Demux MP4/ISOBMFF back to an AV2 elementary stream (requires `-DBUILD_CONTAINER_TOOLS=ON`)
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
