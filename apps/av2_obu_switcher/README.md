# av2_obu_switcher

Stitch multiple AV2 bitstreams together for adaptive streaming experiments.

## Usage

```bash
# Switch from stream1 to stream2 at 2.5 seconds (30 fps)
./av2_obu_switcher -o output.bin -f 30 -t 2.5 stream1.bin stream2.bin

# Multiple switch points
./av2_obu_switcher -o output.bin -f 30 -t 1.0 -t 3.5 -t 5.0 low.bin mid.bin high.bin

# Verbose mode to see switching details
./av2_obu_switcher -o output.bin -f 30 -t 2.0 -v stream1.bin stream2.bin

# Switch on different OBU type
./av2_obu_switcher -o output.bin -f 30 -t 2.0 --obu-type 7 stream1.bin stream2.bin
```

## Options

- `-f, --fps` - Frame rate in fps (default: 30)
- `-t, --timestamps` - Switch timestamps in seconds (can specify multiple)
- `--obu-type` - OBU type to switch on (default: 4)

## Status

Work in progress - basic switching logic implemented.
