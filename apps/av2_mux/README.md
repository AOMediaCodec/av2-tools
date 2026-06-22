# av2_mux

Mux AV2 elementary bitstreams into MP4/ISOBMFF containers.

## Build

Requires enabling the container tools during CMake configuration:

```bash
cmake -S . -B mybuild -DBUILD_CONTAINER_TOOLS=ON
cmake --build mybuild -j
```

This fetches MPEG's libisomedia reference implementation automatically.

## Usage

```bash
# Mux an AV2 elementary stream into MP4
./av2_mux input.obu -o output.mp4

# Override frame rate
./av2_mux input.obu -o output.mp4 --fps 60

# Force colour metadata when the bitstream has no CI/LCR/OPS color info
./av2_mux input.obu -o output.mp4 --colr-override 1:13:1:0

# Control samples-per-chunk in stsc
./av2_mux input.obu -o output.mp4 --samples-per-chunk 60

# Verbose mode prints every meaningful muxing step
./av2_mux input.obu -o output.mp4 -v
```

See `--help` for the full flag list.
