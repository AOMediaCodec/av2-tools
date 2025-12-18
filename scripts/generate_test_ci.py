#!/usr/bin/env python3
"""
Generate AVM bitstream files with Content Interpretation OBU for testing.

Supports various configurations:
- Color description (BT.709, BT.2020, etc.)
- Chroma sample position
- Aspect ratio (predefined or custom)
- Timing info
"""

import argparse
import os
import sys

from av2_obu_writer import (
    BitWriter,
    OBU_CONTENT_INTERPRETATION,
    create_obu,
    write_obu_file,
)


# ============================================================================
# Content Interpretation (CI) Generators
# ============================================================================

def content_interpretation(
    scan_type_idc=0,
    # Color description
    color_description=False,
    color_description_idc=0,
    color_primaries=1,
    transfer_characteristics=1,
    matrix_coefficients=1,
    full_range_flag=0,
    # Chroma sample position
    chroma_sample_position=False,
    chroma_sample_position_top=0,
    chroma_sample_position_bottom=0,
    # Aspect ratio
    aspect_ratio=False,
    aspect_ratio_idc=1,
    sar_width=None,
    sar_height=None,
    # Timing info
    timing_info=False,
    num_units_in_display_tick=1000,
    time_scale=60000,
    equal_picture_interval=True,
    num_ticks_per_picture_minus_1=0
):
    """ Create content_interpretation_obu(). """
    bw = BitWriter()

    # Main flags
    bw.write_f(2, scan_type_idc)
    bw.write_f(1, 1 if color_description else 0)
    bw.write_f(1, 1 if chroma_sample_position else 0)
    bw.write_f(1, 1 if aspect_ratio else 0)
    bw.write_f(1, 1 if timing_info else 0)
    bw.write_f(2, 0)  # reserved_2bit

    # Color description
    if color_description:
        bw.write_rg(2, color_description_idc)
        if color_description_idc == 0:
            bw.write_f(8, color_primaries)
            bw.write_f(8, transfer_characteristics)
            bw.write_f(8, matrix_coefficients)
        bw.write_f(1, full_range_flag)

    # Chroma sample position
    if chroma_sample_position:
        bw.write_uvlc(chroma_sample_position_top)
        if scan_type_idc != 1:
            bw.write_uvlc(chroma_sample_position_bottom)

    # Aspect ratio
    if aspect_ratio:
        bw.write_f(8, aspect_ratio_idc)
        if aspect_ratio_idc == 255:
            bw.write_uvlc(sar_width if sar_width is not None else 1)
            bw.write_uvlc(sar_height if sar_height is not None else 1)

    # Timing info
    if timing_info:
        bw.write_f(32, num_units_in_display_tick)
        bw.write_f(32, time_scale)
        bw.write_f(1, 1 if equal_picture_interval else 0)
        if equal_picture_interval:
            bw.write_uvlc(num_ticks_per_picture_minus_1)

    bits_before = bw.bit_count()
    bw.byte_alignment()

    return bw.to_bytes(), bits_before


# ============================================================================
# High-level generators
# ============================================================================

def generate_preset_configurations():
    """Generate common content interpretation configurations."""
    presets = {
        'bt709_progressive': {
            'description': 'BT.709 progressive (HD/SDR)',
            'params': {
                'scan_type_idc': 1,  # Progressive
                'color_description': True,
                'color_description_idc': 0,
                'color_primaries': 1,
                'transfer_characteristics': 1,
                'matrix_coefficients': 1,
                'full_range_flag': 0,
                'chroma_sample_position': True,
                'chroma_sample_position_top': 0,  # CSP_LEFT
                'aspect_ratio': True,
                'aspect_ratio_idc': 1,
                'timing_info': True,
                'num_units_in_display_tick': 1000,
                'time_scale': 60000,
                'equal_picture_interval': True,
                'num_ticks_per_picture_minus_1': 0,
            }
        },
        'bt2020_hdr_pq': {
            'description': 'BT.2020 HDR with PQ (4K/HDR10)',
            'params': {
                'scan_type_idc': 1,  # Progressive
                'color_description': True,
                'color_description_idc': 0,
                'color_primaries': 9,  # BT.2020
                'transfer_characteristics': 16,  # PQ
                'matrix_coefficients': 9,  # BT.2020 NCL
                'full_range_flag': 0,
                'chroma_sample_position': True,
                'chroma_sample_position_top': 2,  # CSP_TOPLEFT
                'aspect_ratio': True,
                'aspect_ratio_idc': 1,
                'timing_info': True,
                'num_units_in_display_tick': 1001,
                'time_scale': 24000,
                'equal_picture_interval': True,
                'num_ticks_per_picture_minus_1': 0,
            }
        },
        'bt2020_hdr_hlg': {
            'description': 'BT.2020 HDR with HLG',
            'params': {
                'scan_type_idc': 1,  # Progressive
                'color_description': True,
                'color_description_idc': 0,
                'color_primaries': 9,  # BT.2020
                'transfer_characteristics': 18,  # HLG
                'matrix_coefficients': 9,  # BT.2020 NCL
                'full_range_flag': 0,
                'chroma_sample_position': True,
                'chroma_sample_position_top': 0,  # CSP_LEFT
                'aspect_ratio': True,
                'aspect_ratio_idc': 1,
                'timing_info': True,
                'num_units_in_display_tick': 1001,
                'time_scale': 30000,
                'equal_picture_interval': True,
                'num_ticks_per_picture_minus_1': 0,
            }
        },
        'interlaced': {
            'description': 'Interlaced field pictures (BT.601)',
            'params': {
                'scan_type_idc': 2,  # Interlaced
                'color_description': True,
                'color_description_idc': 0,
                'color_primaries': 6,  # BT.601
                'transfer_characteristics': 6,  # BT.601
                'matrix_coefficients': 6,  # BT.601
                'full_range_flag': 0,
                'chroma_sample_position': True,
                'chroma_sample_position_top': 0,  # CSP_LEFT (top field)
                'chroma_sample_position_bottom': 0,  # CSP_LEFT (bottom field)
                'aspect_ratio': True,
                'aspect_ratio_idc': 1,
                'timing_info': True,
                'num_units_in_display_tick': 1001,
                'time_scale': 60000,
                'equal_picture_interval': True,
                'num_ticks_per_picture_minus_1': 0,
            }
        },
        'custom_sar': {
            'description': 'Custom sample aspect ratio (2.35:1 anamorphic)',
            'params': {
                'scan_type_idc': 1,  # Progressive
                'color_description': True,
                'color_description_idc': 0,
                'color_primaries': 1,
                'transfer_characteristics': 1,
                'matrix_coefficients': 1,
                'full_range_flag': 0,
                'aspect_ratio': True,
                'aspect_ratio_idc': 255,  # Extended SAR
                'sar_width': 235,
                'sar_height': 100,
                'timing_info': True,
                'num_units_in_display_tick': 1001,
                'time_scale': 24000,
                'equal_picture_interval': True,
                'num_ticks_per_picture_minus_1': 0,
            }
        },
        'minimal': {
            'description': 'Minimal (all flags off)',
            'params': {
                'scan_type_idc': 0,  # Unspecified
                'color_description': False,
                'chroma_sample_position': False,
                'aspect_ratio': False,
                'timing_info': False,
            }
        },
    }

    results = {}
    for name, config in presets.items():
        payload, bits = content_interpretation(**config['params'])
        results[name] = (payload, bits, config['description'])

    return results


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate Content Interpretation OBU test files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate all preset configurations
  %(prog)s --all

  # Generate specific preset
  %(prog)s --preset bt2020_hdr_pq

  # Generate with custom parameters
  %(prog)s --scan-type 0 --color-primaries 9 --transfer 16

  # Custom output location
  %(prog)s --preset bt709_progressive --output my_ci.obu

Available presets:
  bt709_progressive  - BT.709 progressive (HD/SDR)
  bt2020_hdr_pq      - BT.2020 HDR with PQ (4K/HDR10)
  bt2020_hdr_hlg     - BT.2020 HDR with HLG
  interlaced         - Interlaced (top field first)
  custom_sar         - Custom sample aspect ratio
  minimal            - Minimal (all optional flags off)
        """
    )

    parser.add_argument('--all', action='store_true',
                       help='Generate all preset configurations')
    parser.add_argument('--preset', choices=['bt709_progressive', 'bt2020_hdr_pq',
                                             'bt2020_hdr_hlg', 'interlaced',
                                             'custom_sar', 'minimal'],
                       help='Generate specific preset configuration')
    parser.add_argument('--output', type=str, default=None,
                       help='Output filename (default: auto-generated)')
    parser.add_argument('--output-dir', type=str, default='test_obus/ci',
                       help='Output directory for generated files (default: test_obus/ci)')
    parser.add_argument('--prefix', type=str, default='ci',
                       help='Filename prefix for generated files (default: ci)')

    # Custom parameters
    parser.add_argument('--scan-type', type=int, choices=[0, 1, 2],
                       help='Scan type: 0=progressive, 1=interlaced top, 2=interlaced bottom')
    parser.add_argument('--color-primaries', type=int,
                       help='Color primaries (1=BT.709, 9=BT.2020)')
    parser.add_argument('--transfer', type=int,
                       help='Transfer characteristics (1=BT.709, 16=PQ, 18=HLG)')

    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    print("=" * 70)
    print("AVM Content Interpretation OBU Generator")
    print("=" * 70)
    print()

    if args.all:
        print("Generating all preset configurations...\n")

        presets = generate_preset_configurations()
        for name, (payload, bits, description) in presets.items():
            obu = create_obu(OBU_CONTENT_INTERPRETATION, payload)
            filename = os.path.join(args.output_dir, f"{args.prefix}_{name}.obu")
            write_obu_file(filename, obu, f"{description}, {bits} bits before alignment")

    elif args.preset:
        presets = generate_preset_configurations()
        payload, bits, description = presets[args.preset]

        obu = create_obu(OBU_CONTENT_INTERPRETATION, payload)

        if args.output:
            output = os.path.join(args.output_dir, args.output)
        else:
            output = os.path.join(args.output_dir, f"{args.prefix}_{args.preset}.obu")

        write_obu_file(output, obu, f"{description}, {bits} bits before alignment")

    elif any([args.scan_type is not None, args.color_primaries, args.transfer]):
        # Custom configuration
        params = {
            'scan_type_idc': args.scan_type if args.scan_type is not None else 0,
        }

        if args.color_primaries or args.transfer:
            params['color_description'] = True
            params['color_description_idc'] = 0
            params['color_primaries'] = args.color_primaries if args.color_primaries else 1
            params['transfer_characteristics'] = args.transfer if args.transfer else 1
            params['matrix_coefficients'] = 1

        payload, bits = content_interpretation(**params)
        obu = create_obu(OBU_CONTENT_INTERPRETATION, payload)

        if args.output:
            output = os.path.join(args.output_dir, args.output)
        else:
            output = os.path.join(args.output_dir, f"{args.prefix}_custom.obu")

        write_obu_file(output, obu, f"Custom configuration, {bits} bits before alignment")

    else:
        parser.print_help()
        return 1

    print()
    print(f"Done! Files written to: {args.output_dir}/")
    print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
