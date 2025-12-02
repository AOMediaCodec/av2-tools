# av2_obu_packager

Package AV2 elementary streams into MP4/ISOBMFF containers.

## Build

Requires enabling packager support during CMake configuration:

```bash
cmake -S . -B mybuild -DBUILD_PACKAGER=ON
cmake --build mybuild -j
```

This will fetch MPEG's libisomedia reference implementation automatically.

## Usage

```bash
# Package AV2 bitstream into MP4
./av2_obu_packager package input.bin -o output.mp4 --fps 30
```

## Status

Work in progress - basic setup implemented.
