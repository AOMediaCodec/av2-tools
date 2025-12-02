# av2_obu_channel_sim

Simulate network conditions and channel errors for AV2 bitstreams.

## Usage

### Bit Corruption

Inject random bit errors with configurable bit error rate (BER):

```bash
# 0.1% bit error rate
./av2_obu_channel_sim corrupt input.bin -o corrupted.bin --ber 0.001

# Corrupt config OBUs as well
./av2_obu_channel_sim corrupt input.bin -o corrupted.bin --ber 0.01 --no-protect-config

# Reproducible errors
./av2_obu_channel_sim corrupt input.bin -o corrupted.bin --ber 0.001 --seed 42
```

### Packet Loss

Simulate packet-based transmission with configurable packet loss rate (PLR):

```bash
# 5% packet loss with 1200-byte packets
./av2_obu_channel_sim packetloss input.bin -o lossy.bin --plr 0.05

# Custom packet size
./av2_obu_channel_sim packetloss input.bin -o lossy.bin --packet-size 500 --plr 0.1

# Verbose mode (shows protected packets)
./av2_obu_channel_sim -v packetloss input.bin -o lossy.bin --plr 0.05
```

## Protected OBU Types

By default, these configuration OBUs are protected (not corrupted/dropped):

- `SEQUENCE_HEADER`
- `MULTI_FRAME_HEADER`
- `LAYER_CONFIGURATION_RECORD`
- `OPERATING_POINT_SET`

This simulates real-world scenarios where config data is sent out-of-band or with FEC.
