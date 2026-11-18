# av2_demux

Demux an MP4/ISOBMFF file containing an AV2 video track back into an AV2 elementary bitstream (`.obu`, Annex B framed).

## Build

Same flag as `av2_mux`:

```bash
cmake -S . -B mybuild -DBUILD_CONTAINER_TOOLS=ON
cmake --build mybuild -j
```

## Usage

```bash
./av2_demux input.mp4 -o output.obu
./av2_demux input.mp4 -o output.obu -v   # show every meaningful demuxing step
```

## What it does

1. Opens the `.mp4` and finds the first track whose sample entry is `'av2E'` or `'av2e'`.
2. For `'av2E'`, pulls the `av2C` configuration box from that sample entry and extracts its `config_obu[]` (everything after the 2-byte header). For `'av2e'`, validates the 2-byte `av2c` configuration box (configuration OBUs are already in-band in samples).
3. For every sample in decode order, writes a Temporal Delimiter (`{0x01, 0x08}`), injects `config_obu[]` at random access points (and the start of the stream) for `'av2E'`, and writes the sample OBUs.

The result is a parseable AV2 elementary bitstream that round-trips through `av2_obu_tool`.
