/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License and the Alliance for
 * Open Media Patent License 1.0. If the BSD 3-Clause Clear License was not distributed with this
 * source code in the LICENSE file, you can obtain it at
 * aomedia.org/license/software-license/bsd-3-c-c/. If the Alliance for Open Media Patent
 * License 1.0 was not distributed with this source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#pragma once

// AV2 OBU Tool - Master Header
// This is a convenience header that includes all public API headers

// Core infrastructure
#include <av2_obu/core/av2_math.h>
#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/core/frame_header_info.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/core/tile_group_header.h>

// OBU implementations
#include <av2_obu/obus/atlas_segment_obu.h>
#include <av2_obu/obus/bridge_frame_obu.h>
#include <av2_obu/obus/buffer_removal_timing_obu.h>
#include <av2_obu/obus/clk_obu.h>
#include <av2_obu/obus/content_interpretation_obu.h>
#include <av2_obu/obus/fgm_obu.h>
#include <av2_obu/obus/layer_configuration_record_obu.h>
#include <av2_obu/obus/leading_sef_obu.h>
#include <av2_obu/obus/leading_tile_group_obu.h>
#include <av2_obu/obus/leading_tip_obu.h>
#include <av2_obu/obus/metadata_group_obu.h>
#include <av2_obu/obus/metadata_obu.h>
#include <av2_obu/obus/metadata_unit.h>
#include <av2_obu/obus/msdo_obu.h>
#include <av2_obu/obus/multi_frame_header_obu.h>
#include <av2_obu/obus/olk_obu.h>
#include <av2_obu/obus/operating_point_set_obu.h>
#include <av2_obu/obus/padding_obu.h>
#include <av2_obu/obus/qm_obu.h>
#include <av2_obu/obus/ras_frame_obu.h>
#include <av2_obu/obus/regular_sef_obu.h>
#include <av2_obu/obus/regular_tile_group_obu.h>
#include <av2_obu/obus/regular_tip_obu.h>
#include <av2_obu/obus/sequence_header_obu.h>
#include <av2_obu/obus/switch_obu.h>
#include <av2_obu/obus/temporal_delimiter_obu.h>
#include <av2_obu/obus/tile_group_obu.h>
#include <av2_obu/obus/unknown_obu.h>

namespace av2 {}  // namespace av2
