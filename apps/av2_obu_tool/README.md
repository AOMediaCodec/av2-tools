# av2_obu_tool

Parse, analyze, and dump AV2 OBU bitstreams.

## Usage

```bash
# Dump OBU information
./av2_obu_tool dump input.bin

# Export to JSON
./av2_obu_tool -j dump input.bin -o output.json

# Show statistics
./av2_obu_tool stats input.bin

# Verbose logging
./av2_obu_tool -v dump input.bin
```

## Features

- JSON export with preserved key ordering
- Detailed statistics (OBU counts, sizes, type distribution)
- Human-readable and machine-readable output
- Stubs for a bunch of OBUs (WIP) we can implement some but the syntax of AV2 is still changing a lot
