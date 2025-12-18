#!/usr/bin/env python3
"""
Common utilities for generating AV2 OBU test files.
"""

# ============================================================================
# Constants (from AV2 spec)
# ============================================================================

OBU_SEQUENCE_HEADER = 1
OBU_TEMPORAL_DELIMITER = 2
OBU_MULTI_FRAME_HEADER = 3
OBU_CLK = 4
OBU_OLK = 5
OBU_LEADING_TILE_GROUP = 6
OBU_REGULAR_TILE_GROUP = 7
OBU_METADATA_SHORT = 8
OBU_METADATA_GROUP = 9
OBU_SWITCH = 10
OBU_LEADING_SEF = 11
OBU_REGULAR_SEF = 12
OBU_LEADING_TIP = 13
OBU_REGULAR_TIP = 14
OBU_BUFFER_REMOVAL_TIMING = 15
OBU_LAYER_CONFIGURATION_RECORD = 16
OBU_ATLAS_SEGMENT = 17
OBU_OPERATING_POINT_SET = 18
OBU_BRIDGE_FRAME = 19
OBU_MSDO = 20
OBU_RAS_FRAME = 21
OBU_QM = 22
OBU_FGM = 23
OBU_CONTENT_INTERPRETATION = 24
OBU_PADDING = 25

# Metadata Types
METADATA_TYPE_HDR_CLL = 1
METADATA_TYPE_HDR_MDCV = 2
METADATA_TYPE_SCALABILITY = 3
METADATA_TYPE_ITUT_T35 = 4
METADATA_TYPE_TIMECODE = 5
METADATA_TYPE_DECODED_FRAME_HASH = 6
METADATA_TYPE_BANDING_HINTS = 7
METADATA_TYPE_ICC_PROFILE = 8
METADATA_TYPE_SCAN_TYPE = 9
METADATA_TYPE_TEMPORAL_POINT_INFO = 10

METADATA_TYPE_NAMES = {
    METADATA_TYPE_HDR_CLL: "HDR_CLL",
    METADATA_TYPE_HDR_MDCV: "HDR_MDCV",
    METADATA_TYPE_SCALABILITY: "SCALABILITY",
    METADATA_TYPE_ITUT_T35: "ITUT_T35",
    METADATA_TYPE_TIMECODE: "TIMECODE",
    METADATA_TYPE_DECODED_FRAME_HASH: "DECODED_FRAME_HASH",
    METADATA_TYPE_BANDING_HINTS: "BANDING_HINTS",
    METADATA_TYPE_ICC_PROFILE: "ICC_PROFILE",
    METADATA_TYPE_SCAN_TYPE: "SCAN_TYPE",
    METADATA_TYPE_TEMPORAL_POINT_INFO: "TEMPORAL_POINT_INFO",
}

def write_leb128(value):
    bytes_out = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value != 0:
            byte |= 0x80
        bytes_out.append(byte)
        if value == 0:
            break
    return bytes(bytes_out)


def leb128_size(value):
    return len(write_leb128(value))


# ============================================================================
# BitWriter Class
# ============================================================================

class BitWriter:
    """Helper class for writing bit-level syntax elements"""

    def __init__(self):
        self.bits = []

    def write_bits(self, value, num_bits):
        for i in range(num_bits - 1, -1, -1):
            self.bits.append((value >> i) & 1)

    def write_f(self, num_bits, value):
        """f(n)"""
        self.write_bits(value, num_bits)

    def write_uvlc(self, value):
        """uvlc() from AV2 spec"""
        if value == 0:
            self.bits.append(1)
            return

        # Calculate leading zeros
        leading_zeros = 0
        temp = value + 1
        while temp > (1 << (leading_zeros + 1)):
            leading_zeros += 1

        # Write leading zeros
        for _ in range(leading_zeros):
            self.bits.append(0)

        # Write done bit
        self.bits.append(1)

        # Write value
        actual_value = value - ((1 << leading_zeros) - 1)
        self.write_bits(actual_value, leading_zeros)

    def write_rg(self, n, value):
        """rg(n) from AV2 spec"""
        q = value >> n
        remainder = value & ((1 << n) - 1)

        # Write q 1-bits
        for _ in range(q):
            self.bits.append(1)

        # Write terminating 0-bit
        self.bits.append(0)

        # Write remainder
        self.write_bits(remainder, n)

    def write_tu(self, mx, value):
        """tu(mx) from AV2 spec"""
        for _ in range(value):
            self.bits.append(1)

        if value < mx:
            self.bits.append(0)

    def write_leb128(self, value):
        """leb128() from AV2 spec   """
        # Spec requirement: "will only be present when bitstream position is byte aligned"
        self.byte_alignment()

        leb_bytes = write_leb128(value)

        # Write bytes as bits
        for byte in leb_bytes:
            self.write_f(8, byte)

    def byte_alignment(self):
        """Pad with 0s to byte boundary"""
        bits_to_add = (8 - (len(self.bits) % 8)) % 8
        if bits_to_add > 0:
            self.write_bits(0, bits_to_add)

    def bit_count(self):
        return len(self.bits)

    def to_bytes(self):
        """Convert bit buffer to bytes, padding to byte boundary if needed"""
        # Pad to byte boundary
        while len(self.bits) % 8 != 0:
            self.bits.append(0)

        payload = bytearray()
        for i in range(0, len(self.bits), 8):
            byte = 0
            for j in range(8):
                byte = (byte << 1) | self.bits[i + j]
            payload.append(byte)

        return bytes(payload)


# ============================================================================
# Helpers
# ============================================================================

def obu_header(obu_type, obu_tlayer_id=0, obu_extension_flag=0,
               obu_mlayer_id=0, obu_xlayer_id=0):
    """Create OBU header """
    bw = BitWriter()

    bw.write_f(1, obu_extension_flag)
    bw.write_f(5, obu_type)
    bw.write_f(2, obu_tlayer_id)  # TLAYER_BITS = 2

    if obu_extension_flag == 1:
        bw.write_f(3, obu_mlayer_id)  # MLAYER_BITS = 3
        bw.write_f(5, obu_xlayer_id)

    return bw.to_bytes()


def create_obu(obu_type, payload, **header_kwargs):
    """Create complete OBU with Annex B framing (LEB128 size prefix)"""
    header = obu_header(obu_type, **header_kwargs)

    obu_data = bytearray(header)
    obu_data.extend(payload)

    size = write_leb128(len(obu_data))
    return bytes(size) + bytes(obu_data)


def write_obu_file(filename, obu_data, description=""):
    """Write OBU data to file.

    Args:
        filename: Output file path
        obu_data: OBU bytes to write
        description: Optional description to print
    """
    with open(filename, 'wb') as f:
        f.write(obu_data)
    print(f"- {filename}: {len(obu_data)} bytes")
    if description:
        print(f"  {description}\n")
