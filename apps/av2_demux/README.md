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

1. Opens the `.mp4` and finds the first track whose sample entry is `'av02'`.
2. Pulls the `av2C` configuration box from that sample entry and extracts its `configOBUs[]` (everything after the 9-byte prefix).
3. Writes those configOBUs at the start of the output — they are already Annex B framed, so they go in verbatim.
4. For every sample in decode order, writes a Temporal Delimiter (`{0x01, 0x08}`) followed by the raw sample bytes.

The result is a parseable AV2 elementary bitstream that round-trips through `av2_obu_tool`.
