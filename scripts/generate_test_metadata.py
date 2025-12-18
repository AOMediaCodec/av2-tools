#!/usr/bin/env python3
"""
Generate AVM bitstream files with various metadata types for testing.

Supports all metadata types from AV2 spec and can generate:
- Individual metadata OBUs (short format)
- Metadata group OBUs (with one or multiple units)
- Byte-aligned and non-byte-aligned variants for testing
"""

import argparse
import os
import sys

from av2_obu_writer import (
    BitWriter,
    OBU_METADATA_SHORT,
    OBU_METADATA_GROUP,
    METADATA_TYPE_HDR_CLL,
    METADATA_TYPE_HDR_MDCV,
    METADATA_TYPE_ITUT_T35,
    METADATA_TYPE_TIMECODE,
    METADATA_TYPE_DECODED_FRAME_HASH,
    METADATA_TYPE_BANDING_HINTS,
    METADATA_TYPE_ICC_PROFILE,
    METADATA_TYPE_SCAN_TYPE,
    METADATA_TYPE_TEMPORAL_POINT_INFO,
    METADATA_TYPE_NAMES,
    leb128_size,
    create_obu,
    write_obu_file,
)


# ============================================================================
# Metadata Unit Generators
# ============================================================================

def metadata_timecode(full_timestamp=False, time_offset_length=0,
                      counting_type=0, n_frames=0,
                      seconds=0, minutes=0, hours=0,
                      skip_byte_alignment=False):
    """Create metadata_timecode(). Naturally non-byte-aligned (23 bits minimum)."""
    bw = BitWriter()

    bw.write_f(5, counting_type)
    bw.write_f(1, 1 if full_timestamp else 0)
    bw.write_f(1, 0)  # discontinuity_flag
    bw.write_f(1, 0)  # cnt_dropped_flag
    bw.write_f(9, n_frames)

    if full_timestamp:
        bw.write_f(6, seconds)
        bw.write_f(6, minutes)
        bw.write_f(5, hours)
    else:
        bw.write_f(1, 0)  # seconds_flag

    bw.write_f(5, time_offset_length)
    if time_offset_length > 0:
        bw.write_f(time_offset_length, 0)

    bits_before = bw.bit_count()
    if not skip_byte_alignment:
        bw.byte_alignment()

    return bw.to_bytes(), bits_before


def metadata_hdr_cll(max_cll=1000, max_fall=400):
    """Create metadata_hdr_cll(). Always byte-aligned (32 bits)."""
    bw = BitWriter()
    bw.write_f(16, max_cll)
    bw.write_f(16, max_fall)
    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


def metadata_hdr_mdcv(primaries=None, white_point=None,
                      luminance_max=10000000, luminance_min=50):
    """Create metadata_hdr_mdcv(). Always byte-aligned (192 bits)."""
    bw = BitWriter()

    if primaries is None:
        primaries = [(34000, 16000), (13250, 34500), (7500, 3000)]  # Rec. 2020
    if white_point is None:
        white_point = (15635, 16450)  # D65

    for i in range(3):
        bw.write_f(16, primaries[i][0])
        bw.write_f(16, primaries[i][1])

    bw.write_f(16, white_point[0])
    bw.write_f(16, white_point[1])
    bw.write_f(32, luminance_max)
    bw.write_f(32, luminance_min)

    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


def metadata_itut_t35(country_code=0xB5, payload=b"Test T.35 payload"):
    """Create metadata_itut_t35(). Always byte-aligned."""
    bw = BitWriter()

    bw.write_f(8, country_code)
    if country_code == 0xFF:
        bw.write_f(8, 0x00)

    for byte in payload:
        bw.write_f(8, byte)

    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


def metadata_banding_hints(coding_present=True, source_present=False,
                           hints_flag=False, three_components=False,
                           with_band_units=False, skip_byte_alignment=False):
    """Create metadata_banding_hints().

    Args:
        coding_present: coding_banding_present_flag
        source_present: source_banding_present_flag
        hints_flag: banding_hints_flag (enables full structure)
        three_components: three_color_components (1=3 components, 0=1 component)
        with_band_units: Include band_units_information (2x3 grid with varying sizes)
        skip_byte_alignment: Skip byte alignment (for testing)
    """
    bw = BitWriter()

    bw.write_f(1, 1 if coding_present else 0)
    bw.write_f(1, 1 if source_present else 0)

    if coding_present:
        bw.write_f(1, 1 if hints_flag else 0)

        if hints_flag:
            bw.write_f(1, 1 if three_components else 0)
            num_components = 3 if three_components else 1

            # Component info
            for i in range(num_components):
                if i < 2:  # First two components have banding info
                    bw.write_f(1, 1)  # banding_in_component_present_flag
                    bw.write_f(6, 10 + i * 2)  # max_band_width_minus4
                    bw.write_f(4, 5 + i * 2)   # max_band_step_minus1
                else:
                    bw.write_f(1, 0)  # banding_in_component_present_flag

            # Band units information
            bw.write_f(1, 1 if with_band_units else 0)

            if with_band_units:
                bw.write_f(5, 1)  # num_band_units_rows_minus_1 = 1 (2 rows)
                bw.write_f(5, 2)  # num_band_units_cols_minus_1 = 2 (3 cols)
                bw.write_f(1, 1)  # varying_size_band_units_flag

                # Varying size info
                bw.write_f(3, 3)  # band_block_in_luma_samples

                # Vertical sizes (2 rows)
                bw.write_f(5, 8)   # vert_size[0]
                bw.write_f(5, 10)  # vert_size[1]

                # Horizontal sizes (3 cols)
                bw.write_f(5, 6)   # horz_size[0]
                bw.write_f(5, 7)   # horz_size[1]
                bw.write_f(5, 9)   # horz_size[2]

                # Banding flags for 2x3 grid
                bw.write_f(1, 1)  # [0][0]
                bw.write_f(1, 0)  # [0][1]
                bw.write_f(1, 1)  # [0][2]
                bw.write_f(1, 0)  # [1][0]
                bw.write_f(1, 1)  # [1][1]
                bw.write_f(1, 0)  # [1][2]

    bits_before = bw.bit_count()
    if not skip_byte_alignment:
        bw.byte_alignment()

    return bw.to_bytes(), bits_before


def metadata_icc_profile(profile_data=None):
    """Create metadata_icc_profile(). Always byte-aligned."""
    if profile_data is None:
        profile_data = b"ICC" + b"\x00" * 125  # 128 bytes minimum

    bw = BitWriter()
    for byte in profile_data:
        bw.write_f(8, byte)

    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


def metadata_scan_type(pic_struct_type=0, source_scan_type_idc=0,
                      duplicate_flag=0):
    """Create metadata_scan_type(). Always byte-aligned (8 bits)."""
    bw = BitWriter()
    bw.write_f(5, pic_struct_type)
    bw.write_f(2, source_scan_type_idc)
    bw.write_f(1, duplicate_flag)
    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


def metadata_temporal_point_info(frame_presentation_time=0,
                                 time_length=10,
                                 skip_byte_alignment=False):
    """Create metadata_temporal_point_info(). Naturally non-byte-aligned (5+n bits, typically 15)."""
    bw = BitWriter()

    bw.write_f(5, time_length - 1)
    n = time_length
    bw.write_f(n, frame_presentation_time & ((1 << n) - 1))

    bits_before = bw.bit_count()
    if not skip_byte_alignment:
        bw.byte_alignment()

    return bw.to_bytes(), bits_before


def metadata_decoded_frame_hash(hash_type=0, hash_value=None):
    """Create metadata_decoded_frame_hash() (simplified). Always byte-aligned."""
    bw = BitWriter()
    bw.write_f(8, hash_type)

    if hash_value is None:
        hash_value = b"\x00" * 16  # MD5 hash

    for byte in hash_value:
        bw.write_f(8, byte)

    bits_before = bw.bit_count()
    bw.byte_alignment()
    return bw.to_bytes(), bits_before


# ============================================================================
# OBU Constructors
# ============================================================================

def metadata_short_obu(metadata_type, payload,
                      metadata_is_suffix=0,
                      muh_layer_idc=0,
                      muh_persistence_idc=0):
    """Create metadata_short_obu() for OBU_METADATA type 8."""
    bw = BitWriter()

    bw.write_f(1, metadata_is_suffix)
    bw.write_f(3, muh_layer_idc)

    muh_cancel_flag = 0
    bw.write_f(1, muh_cancel_flag)
    bw.write_f(3, muh_persistence_idc)

    # automatically byte-aligns
    bw.write_leb128(metadata_type)

    # Convert to bytes and append payload
    obu_payload = bytearray(bw.to_bytes())
    if not muh_cancel_flag:
        obu_payload.extend(payload)

    obu_payload.append(0x80)  # trailing_bits
    return bytes(obu_payload)


def metadata_group_obu(metadata_units, metadata_is_suffix=0,
                      metadata_necessity_idc=0,
                      metadata_application_id=0):
    """Create metadata_group_obu(). metadata_units: list of (metadata_type, payload_bytes)."""
    bw = BitWriter()

    bw.write_f(1, metadata_is_suffix)
    bw.write_f(2, metadata_necessity_idc)
    bw.write_f(5, metadata_application_id)

    bw.write_leb128(len(metadata_units) - 1)

    for metadata_type, payload in metadata_units:
        bw.write_leb128(metadata_type)

        payload_size_bytes = leb128_size(len(payload))
        muh_header_size = payload_size_bytes + 2

        muh_cancel_flag = 0
        header_byte = (muh_header_size << 1) | muh_cancel_flag
        bw.write_f(8, header_byte)

        if not muh_cancel_flag:
            bw.write_leb128(len(payload))

            bw.write_f(8, 0x00)  # layer/persistence/priority
            bw.write_f(8, 0x00)  # reserved

            # Append raw payload bytes
            for byte in payload:
                bw.write_f(8, byte)

    # Convert to bytes and add trailing_bits
    obu_payload = bytearray(bw.to_bytes())
    obu_payload.append(0x80)  # trailing_bits
    return bytes(obu_payload)


# ============================================================================
# High-level generators
# ============================================================================

def generate_all_metadata_types(skip_byte_alignment=False):
    """Generate all metadata types. skip_byte_alignment only affects naturally non-aligned types."""
    generators = {
        METADATA_TYPE_HDR_CLL: lambda: metadata_hdr_cll(),
        METADATA_TYPE_HDR_MDCV: lambda: metadata_hdr_mdcv(),
        METADATA_TYPE_ITUT_T35: lambda: metadata_itut_t35(),
        METADATA_TYPE_DECODED_FRAME_HASH: lambda: metadata_decoded_frame_hash(),
        METADATA_TYPE_ICC_PROFILE: lambda: metadata_icc_profile(),
        METADATA_TYPE_SCAN_TYPE: lambda: metadata_scan_type(),
        METADATA_TYPE_TIMECODE: lambda: metadata_timecode(skip_byte_alignment=skip_byte_alignment),
        METADATA_TYPE_BANDING_HINTS: lambda: metadata_banding_hints(
            hints_flag=True, three_components=True, with_band_units=True,
            skip_byte_alignment=skip_byte_alignment
        ),
        METADATA_TYPE_TEMPORAL_POINT_INFO: lambda: metadata_temporal_point_info(skip_byte_alignment=skip_byte_alignment),
    }

    results = {}
    for mtype, gen in generators.items():
        payload, bits_before = gen()
        results[mtype] = (payload, bits_before)

    return results


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate AVM metadata OBU test files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate all metadata types (short + group format)
  %(prog)s --all

  # Generate specific metadata type
  %(prog)s --type timecode

  # Generate group with multiple units
  %(prog)s --group timecode,cll

  # Generate with custom output filename
  %(prog)s --type timecode --output my_timecode.obu

  # Generate with incorrect byte alignment (for testing)
  %(prog)s --type timecode --no-byte-alignment

  # Generate comprehensive test suite
  %(prog)s --all --test-alignment
        """
    )

    parser.add_argument('--all', action='store_true',
                       help='Generate all metadata types (both short and group)')
    parser.add_argument('--type', choices=['cll', 'mdcv', 't35', 'timecode', 'hash',
                                           'banding', 'icc', 'scan', 'temporal'],
                       help='Generate specific metadata type')
    parser.add_argument('--group', type=str,
                       help='Comma-separated list of types for group OBU')
    parser.add_argument('--output', type=str, default=None,
                       help='Output filename (default: auto-generated)')
    parser.add_argument('--output-dir', type=str, default='test_obus/metadata',
                       help='Output directory for generated files (default: test_obus/metadata)')
    parser.add_argument('--no-byte-alignment', action='store_true',
                       help='Use incorrect 0-padding instead of byte_alignment() (for testing)')
    parser.add_argument('--test-alignment', action='store_true',
                       help='Generate both correct and incorrect alignment variants')
    parser.add_argument('--prefix', type=str, default='test',
                       help='Filename prefix for generated files (default: test)')

    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    type_map = {
        'cll': METADATA_TYPE_HDR_CLL,
        'mdcv': METADATA_TYPE_HDR_MDCV,
        't35': METADATA_TYPE_ITUT_T35,
        'timecode': METADATA_TYPE_TIMECODE,
        'hash': METADATA_TYPE_DECODED_FRAME_HASH,
        'banding': METADATA_TYPE_BANDING_HINTS,
        'icc': METADATA_TYPE_ICC_PROFILE,
        'scan': METADATA_TYPE_SCAN_TYPE,
        'temporal': METADATA_TYPE_TEMPORAL_POINT_INFO,
    }

    skip_alignment = args.no_byte_alignment

    print("=" * 70)
    print("AVM Metadata OBU Generator")
    print("=" * 70)
    print(f"Byte alignment: {'INCORRECT (0-padding, for testing)' if skip_alignment else 'CORRECT (spec-compliant)'}")
    print()

    if args.all:
        print("Generating all metadata types...\n")

        all_metadata = generate_all_metadata_types(skip_byte_alignment=skip_alignment)

        for mtype, (payload, bits) in all_metadata.items():
            name = METADATA_TYPE_NAMES[mtype]

            short_obu = metadata_short_obu(mtype, payload)
            obu_short = create_obu(OBU_METADATA_SHORT, short_obu)
            filename = os.path.join(args.output_dir, f"{args.prefix}_{name.lower()}_short.obu")
            write_obu_file(filename, obu_short,
                          f"OBU_METADATA (short), {bits} bits before alignment")

            group_obu = metadata_group_obu([(mtype, payload)])
            obu_group = create_obu(OBU_METADATA_GROUP, group_obu)
            filename = os.path.join(args.output_dir, f"{args.prefix}_{name.lower()}_group.obu")
            write_obu_file(filename, obu_group,
                          f"OBU_METADATA_GROUP (1 unit), {bits} bits before alignment")

        all_units = [(mtype, payload) for mtype, (payload, bits) in all_metadata.items()]
        group_all = metadata_group_obu(all_units)
        obu_all = create_obu(OBU_METADATA_GROUP, group_all)
        filename = os.path.join(args.output_dir, f"{args.prefix}_all_metadata_group.obu")
        write_obu_file(filename, obu_all,
                      f"OBU_METADATA_GROUP with all {len(all_units)} metadata types")

        if args.test_alignment:
            print()
            print("Generating alignment test variants...")
            print()

            test_types = [
                (METADATA_TYPE_TIMECODE, metadata_timecode),
                (METADATA_TYPE_BANDING_HINTS, metadata_banding_hints),
                (METADATA_TYPE_TEMPORAL_POINT_INFO, metadata_temporal_point_info),
            ]

            for mtype, gen_func in test_types:
                name = METADATA_TYPE_NAMES[mtype]

                payload_correct, bits = gen_func(skip_byte_alignment=False)
                short_correct = metadata_short_obu(mtype, payload_correct)
                obu_correct = create_obu(OBU_METADATA_SHORT, short_correct)
                filename = os.path.join(args.output_dir, f"{args.prefix}_{name.lower()}_aligned_correct.obu")
                write_obu_file(filename, obu_correct, "CORRECT: byte_alignment()")

                payload_bad, _ = gen_func(skip_byte_alignment=True)
                short_bad = metadata_short_obu(mtype, payload_bad)
                obu_bad = create_obu(OBU_METADATA_SHORT, short_bad)
                filename = os.path.join(args.output_dir, f"{args.prefix}_{name.lower()}_aligned_incorrect.obu")
                write_obu_file(filename, obu_bad, "INCORRECT: skip byte_alignment() for testing")

    elif args.type:
        mtype = type_map[args.type]
        name = METADATA_TYPE_NAMES[mtype]

        all_metadata = generate_all_metadata_types(skip_byte_alignment=skip_alignment)
        payload, bits = all_metadata[mtype]

        short_obu = metadata_short_obu(mtype, payload)
        obu_short = create_obu(OBU_METADATA_SHORT, short_obu)

        if args.output:
            output = os.path.join(args.output_dir, args.output)
        else:
            output = os.path.join(args.output_dir, f"{args.prefix}_{name.lower()}_short.obu")
        write_obu_file(output, obu_short,
                      f"OBU_METADATA (short), {name}, {bits} bits before alignment")

    elif args.group:
        type_names = [t.strip() for t in args.group.split(',')]
        mtypes = [type_map[t] for t in type_names]

        all_metadata = generate_all_metadata_types(skip_byte_alignment=skip_alignment)
        units = [(mtype, all_metadata[mtype][0]) for mtype in mtypes]

        group_obu = metadata_group_obu(units)
        obu = create_obu(OBU_METADATA_GROUP, group_obu)

        if args.output:
            output = os.path.join(args.output_dir, args.output)
        else:
            output = os.path.join(args.output_dir, f"{args.prefix}_group_{'_'.join(type_names)}.obu")
        type_list = ', '.join(METADATA_TYPE_NAMES[m] for m in mtypes)
        write_obu_file(output, obu,
                      f"OBU_METADATA_GROUP with {len(units)} units: {type_list}")

    else:
        parser.print_help()
        return 1

    print()
    print(f"Done! Files written to: {args.output_dir}/")
    print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
