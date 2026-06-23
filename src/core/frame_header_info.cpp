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

#include <spdlog/spdlog.h>
#include <av2_obu/core/logging.h>

#include <av2_obu/core/av2_sequence_header.h>
#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/core/frame_header_info.h>

namespace av2_obu {

// film_grain_config(): up to 1 + 19 bits depending on conditions; updates fhi.{apply_grain, fgm_id, grain_seed}.
static void parse_film_grain_config(BitstreamReader& br, const AV2SequenceHeader& sh,
                                    FrameHeaderInfo& fhi) {
  if (!sh.film_grain_params_present ||
      (!fhi.immediate_output_frame && !fhi.implicit_output_frame)) {
    fhi.apply_grain = 0;
  } else if (sh.single_picture_header_flag) {
    fhi.apply_grain = 1;
  } else {
    fhi.apply_grain = br.read_bit();
  }
  if (fhi.apply_grain) {
    fhi.fgm_id = br.read_bits(3);
    // load_grain_model() is decoder state — not parsed.
    fhi.grain_seed = br.read_bits(16);
  }
}

bool FrameHeaderInfo::parse_lightweight(BitstreamReader& br, OBUType obu_type,
                                        const AV2SequenceHeader& sh) {
  // The keyFrame (CLK || OLK) helper from the spec is only needed for state we don't track
  // in lightweight (LCR activation, RefValid reset, OlkEncountered), so we don't bind it.
  // IsBridge is needed locally and exposed in the struct.
  IsBridge = (obu_type == OBUType::BRIDGE_FRAME);

  // --- cur_mfh_id ---
  if (IsBridge) {
    cur_mfh_id = 0;
  } else {
    cur_mfh_id = br.read_uvlc();
  }

  // --- seq_header_id_in_frame_header ---
  if (cur_mfh_id == 0) {
    seq_header_id_in_frame_header = br.read_uvlc();
  }

  // --- Bridge frame ref idx ---
  if (IsBridge) {
    uint32_t n = CeilLog2(sh.inter_config.NumRefFrames);
    bridge_frame_ref_idx = (n > 0) ? br.read_bits(n) : 0;
  }

  uint32_t allFrames = (1 << sh.inter_config.NumRefFrames) - 1;
  use_bru = 0;
  bru_inactive = 0;

  // --- ShowExistingFrame / SEF handling ---
  if (sh.single_picture_header_flag) {
    ShowExistingFrame = false;
    FrameType = KEY_FRAME;
    FrameIsIntra = true;
    immediate_output_frame = 1;
    implicit_output_frame = 0;
  } else {
    ShowExistingFrame = is_sef(obu_type);

    if (ShowExistingFrame) {
      // SEF: parse frame_to_show_map_idx, order hint, then return
      uint32_t n = CeilLog2(sh.inter_config.NumRefFrames);
      frame_to_show_map_idx = (n > 0) ? br.read_bits(n) : 0;
      derive_sef_order_hint = br.read_bit();
      if (derive_sef_order_hint == 0) {
        sef_order_hint = br.read_bits(sh.inter_config.OrderHintBits);
        order_hint = sef_order_hint;
      }
      // For SEF: refresh_frame_flags = 0, FrameType derived from ref
      refresh_frame_flags = 0;
      immediate_output_frame = 1;
      // Spec §frame_header_info: film_grain_config() is called here, before the early
      // return. Must consume its bits to keep the bitstream pointer correct for any
      // subsequent (deep) parsing of this OBU.
      parse_film_grain_config(br, sh, *this);
      TipFrameMode = TIP_FRAME_DISABLED;
      // Frame dimensions come from the referenced frame (not signaled here)
      FrameWidth = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
      FrameHeight = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
      // SEF always produces an output picture (the previously decoded frame is shown).
      is_output_frame = true;
      parsed = true;
      return true;
    }

    // --- FrameType determination ---
    if (IsBridge) {
      FrameType = INTER_FRAME;
    } else if (obu_type == OBUType::SWITCH || obu_type == OBUType::RAS_FRAME) {
      restricted_prediction_switch = br.read_bit();
      FrameType = SWITCH_FRAME;
    } else if (is_tip_frame(obu_type)) {
      FrameType = INTER_FRAME;
    } else if (obu_type == OBUType::CLK || obu_type == OBUType::OLK) {
      FrameType = KEY_FRAME;
    } else {
      // LEADING_TILE_GROUP or REGULAR_TILE_GROUP
      frame_is_inter = br.read_bit();
      FrameType = frame_is_inter ? INTER_FRAME : INTRA_ONLY_FRAME;
    }

    // --- LongTermId ---
    LongTermId = -1;
    if (FrameType == KEY_FRAME && sh.inter_config.long_term_frame_id_bits > 0) {
      long_term_id_plus_1 = br.read_bits(sh.inter_config.long_term_frame_id_bits);
      LongTermId = static_cast<int32_t>(long_term_id_plus_1) - 1;
    }

    // --- num_key_ref_frames (RAS/OLK with long-term IDs) ---
    if ((obu_type == OBUType::RAS_FRAME || obu_type == OBUType::OLK) &&
        sh.inter_config.long_term_frame_id_bits != 0) {
      uint32_t num_key_ref_frames = br.read_bits(3);
      for (uint32_t i = 0; i < num_key_ref_frames; i++) {
        br.read_bits(sh.inter_config.long_term_frame_id_bits);  // ref_long_term_id[i]
      }
    }

    // --- SWITCH restricted prediction handling (skip state tracking) ---

    FrameIsIntra = (FrameType == INTRA_ONLY_FRAME || FrameType == KEY_FRAME);

    // --- immediate_output_frame ---
    if (IsBridge || obu_type == OBUType::OLK) {
      immediate_output_frame = 0;
    } else {
      immediate_output_frame = br.read_bit();
    }

    // --- implicit_output_frame ---
    if (IsBridge || immediate_output_frame || sh.monotonic_output_order_flag) {
      implicit_output_frame = 0;
    } else {
      implicit_output_frame = br.read_bit();
    }
  }

  // --- disable_cross_frame_cdf_init ---
  disable_cross_frame_cdf_init = 0;

  // --- frame_size_override_flag, order_hint, primary_ref_frame ---
  if (IsBridge) {
    primary_ref_frame = PRIMARY_REF_NONE;
    // Bridge: order_hint derived from ref (not signaled)
    // For lightweight, use 0 as placeholder
    order_hint = 0;
  } else {
    if (FrameType == SWITCH_FRAME) {
      frame_size_override_flag = 1;
    } else if (sh.single_picture_header_flag) {
      frame_size_override_flag = 0;
    } else {
      frame_size_override_flag = br.read_bit();
    }

    // --- order_hint ---
    order_hint = br.read_bits(sh.inter_config.OrderHintBits);

    // --- primary_ref_frame ---
    if (FrameIsIntra || FrameType == SWITCH_FRAME) {
      primary_ref_frame = PRIMARY_REF_NONE;
    } else {
      signal_primary_ref_frame = br.read_bit();
      if (!is_tip_frame(obu_type)) {
        disable_cross_frame_cdf_init = br.read_bit();
      }
      if (signal_primary_ref_frame) {
        primary_ref_frame = br.read_bits(3);
      } else {
        primary_ref_frame = PRIMARY_REF_CHOOSE;
      }
    }
  }

  // --- bridge_frame_overwrite_flag (read BEFORE refresh_frame_flags) ---
  if (IsBridge) {
    bridge_frame_overwrite_flag = br.read_bit();
  }

  // --- refresh_frame_flags ---
  if (FrameType == KEY_FRAME) {
    if (obu_type == OBUType::CLK && sh.max_mlayer_id == 0) {
      refresh_frame_flags = allFrames;
    } else if (sh.inter_config.enable_short_refresh_frame_flags) {
      uint32_t n = CeilLog2(sh.inter_config.NumRefFrames);
      frame_to_refresh = (n > 0) ? br.read_bits(n) : 0;
      refresh_frame_flags = 1 << frame_to_refresh;
    } else {
      refresh_frame_flags = br.read_bits(sh.inter_config.NumRefFrames);
    }
  } else if (IsBridge && !bridge_frame_overwrite_flag) {
    // Bridge with !overwrite refreshes only the slot it replaces.
    refresh_frame_flags = 1u << bridge_frame_ref_idx;
  } else if (obu_type == OBUType::RAS_FRAME && sh.max_mlayer_id == 0) {
    // RAS with single layer: refresh non-long-term refs
    // For lightweight, approximate as 0 (full logic needs ref state)
    refresh_frame_flags = 0;
  } else if (FrameType == SWITCH_FRAME) {
    refresh_frame_flags = br.read_bits(sh.inter_config.NumRefFrames);
  } else if (sh.inter_config.enable_short_refresh_frame_flags && FrameType != SWITCH_FRAME &&
             FrameType != KEY_FRAME) {
    // Bridge with overwrite==1 falls through here when SH enables short flags.
    has_refresh_frame_flags = br.read_bit();
    if (has_refresh_frame_flags) {
      uint32_t n = CeilLog2(sh.inter_config.NumRefFrames);
      frame_to_refresh = (n > 0) ? br.read_bits(n) : 0;
      refresh_frame_flags = 1 << frame_to_refresh;
    } else {
      refresh_frame_flags = 0;
    }
  } else {
    refresh_frame_flags = br.read_bits(sh.inter_config.NumRefFrames);
  }

  // --- Frame size and reference info for intra vs inter ---
  if (FrameIsIntra) {
    // frame_size()
    if (frame_size_override_flag) {
      uint32_t w_bits = sh.frame_width_bits_minus_1 + 1;
      uint32_t h_bits = sh.frame_height_bits_minus_1 + 1;
      FrameWidth = br.read_bits(w_bits) + 1;
      FrameHeight = br.read_bits(h_bits) + 1;
    } else {
      FrameWidth = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
      FrameHeight = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
    }

    NumTotalRefs = 0;
    TipFrameMode = TIP_FRAME_DISABLED;
    // LIGHTWEIGHT STOP for intra frames
    // (skip: screen_content_params, intrabc_params)
  } else {
    // --- Inter frame reference parsing ---
    if (FrameType == SWITCH_FRAME || IsBridge) {
      explicitRefFrameMap = 1;
    } else if (sh.inter_config.explicit_ref_frame_map) {
      explicitRefFrameMap = br.read_bit();
    } else {
      explicitRefFrameMap = 0;
    }

    if (IsBridge) {
      NumTotalRefs = 1;
    } else if (explicitRefFrameMap) {
      num_total_refs = br.read_bits(3);
      NumTotalRefs = num_total_refs;
    } else {
      // Implicit ref frame map: get_ref_frames(0) determines refs
      // For lightweight, we can't resolve this without full state
      // Parse will continue but ref_frame_idx won't be populated
      NumTotalRefs = 0;
      LIB_DEBUG("Implicit ref frame map — ref_frame_idx not available in lightweight mode");
    }

    ref_frame_idx.resize(NumTotalRefs);
    for (uint32_t i = 0; i < NumTotalRefs; i++) {
      if (IsBridge) {
        // Bridge: ref_frame_idx[i] = bridge_frame_ref_idx (already parsed above).
        ref_frame_idx[i] = bridge_frame_ref_idx;
      } else if (explicitRefFrameMap) {
        uint32_t n = CeilLog2(sh.inter_config.NumRefFrames);
        ref_frame_idx[i] = (n > 0) ? br.read_bits(n) : 0;
      }
    }

    // --- frame_size / frame_size_with_refs / frame_size_with_bridge ---
    if (IsBridge) {
      // frame_size_with_bridge: read bridge_frame_{width,height}_minus_1.
      // Spec also clamps via Min(RefFrame{Width,Height}[bridge_frame_ref_idx], ...);
      // we lack ref state in lightweight mode, so we report the bitstream value + 1
      // (an upper bound on the actual FrameWidth/FrameHeight).
      uint32_t w_bits = sh.frame_width_bits_minus_1 + 1;
      uint32_t h_bits = sh.frame_height_bits_minus_1 + 1;
      FrameWidth = br.read_bits(w_bits) + 1;
      FrameHeight = br.read_bits(h_bits) + 1;
    } else if (frame_size_override_flag && FrameType != SWITCH_FRAME) {
      // frame_size_with_refs: check each ref for matching size
      bool found_ref = false;
      for (uint32_t i = 0; i < NumTotalRefs; i++) {
        found_ref = br.read_bit();
        if (found_ref) {
          // Dimensions come from reference (need ref state to resolve)
          // For lightweight, use seq header defaults
          FrameWidth = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
          FrameHeight = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
          break;
        }
      }
      if (!found_ref) {
        // frame_size() fallback
        uint32_t w_bits = sh.frame_width_bits_minus_1 + 1;
        uint32_t h_bits = sh.frame_height_bits_minus_1 + 1;
        FrameWidth = br.read_bits(w_bits) + 1;
        FrameHeight = br.read_bits(h_bits) + 1;
      }
    } else {
      // frame_size() — no override or SWITCH frame
      if (frame_size_override_flag) {
        uint32_t w_bits = sh.frame_width_bits_minus_1 + 1;
        uint32_t h_bits = sh.frame_height_bits_minus_1 + 1;
        FrameWidth = br.read_bits(w_bits) + 1;
        FrameHeight = br.read_bits(h_bits) + 1;
      } else {
        FrameWidth = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
        FrameHeight = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
      }
    }

    // --- BRU ---
    if (!explicitRefFrameMap) {
      // get_ref_frames(1) would be called here but we can't without state
    }

    if (sh.inter_config.enable_bru && FrameType == INTER_FRAME && !is_tip_frame(obu_type) &&
        !IsBridge) {
      use_bru = br.read_bit();
      if (use_bru) {
        uint32_t n = CeilLog2(NumTotalRefs);
        bru_ref = (n > 0) ? br.read_bits(n) : 0;
        bru_inactive = br.read_bit();
      }
    }

    // --- use_ref_frame_mvs ---
    if (FrameType == SWITCH_FRAME || !sh.inter_config.enable_ref_frame_mvs || IsBridge ||
        bru_inactive) {
      use_ref_frame_mvs = 0;
    } else {
      use_ref_frame_mvs = br.read_bit();
    }

    // --- tmvp_sample_step_minus_1 ---
    // Per-frame SbSize: 256x256 promotes to BLOCK_256X256 only for non-intra frames.
    BlockSize SbSize;
    if (sh.partition_config.use_256x256_superblock) {
      SbSize = FrameIsIntra ? BLOCK_128X128 : BLOCK_256X256;
    } else if (sh.partition_config.use_128x128_superblock) {
      SbSize = BLOCK_128X128;
    } else {
      SbSize = BLOCK_64X64;
    }
    if (use_ref_frame_mvs && NumTotalRefs > 1 && SbSize != BLOCK_64X64) {
      tmvp_sample_step_minus_1 = br.read_bit();
    }

    // LIGHTWEIGHT STOP for inter frames: skip enable_tip block, screen_content_params,
    // intrabc_params, MV precision, interpolation filter, motion modes, etc.
    // TipFrameMode left at default (TIP_FRAME_DISABLED); the real value comes from the
    // enable_tip block we don't read here.
    TipFrameMode = TIP_FRAME_DISABLED;
  }

  parsed = true;
  // is_output_frame: derived from the per-frame output flags. SEF returns earlier with
  // is_output_frame = true. For other paths, single_picture_header_flag forces
  // immediate_output_frame = 1, otherwise immediate_output_frame and implicit_output_frame
  // are read from the bitstream above.
  is_output_frame = (immediate_output_frame != 0) || (implicit_output_frame != 0);
  return true;
}

bool FrameHeaderInfo::parse_deep(BitstreamReader& /*br*/, OBUType /*obu_type*/,
                                 const AV2SequenceHeader& /*seq_header*/) {
  // TODO: Continue parsing from where parse_lightweight() stopped.
  // For intra frames: screen_content_params(), intrabc_params()
  // For inter frames: use_ref_frame_mvs, TIP details, screen_content_params(),
  //   intrabc_params(), MV precision, interpolation_filter(), motion modes
  // For all frames: tile_info(), quantization_params(), segmentation_params(),
  //   setup_qm_params(), delta_q_params(), loop_filter_params(), gdf_params(),
  //   cdef_params(), lr_params(), ccso_params(), read_tx_mode(),
  //   frame_reference_mode(), skip_mode_params(), global_motion_params(),
  //   film_grain_config()
  LIB_DEBUG("Deep frame header parsing not yet implemented");
  return true;
}

json FrameHeaderInfo::to_json() const {
  json j;

  j["cur_mfh_id"] = cur_mfh_id;
  if (cur_mfh_id == 0) {
    j["seq_header_id_in_frame_header"] = seq_header_id_in_frame_header;
  }

  // Emit FrameType only when reliably known. SEF's FrameType comes from RefFrameType[frame_to_show_map_idx]
  // (decoder ref state we don't track), so skip it to avoid reporting the constructor default.
  const char* frame_type_names[] = {"KEY_FRAME", "INTER_FRAME", "INTRA_ONLY_FRAME", "SWITCH_FRAME"};
  if (!ShowExistingFrame) {
    j["FrameType"] = (FrameType <= 3) ? frame_type_names[FrameType] : "UNKNOWN";
  }

  if (ShowExistingFrame) {
    j["frame_to_show_map_idx"] = frame_to_show_map_idx;
    j["derive_sef_order_hint"] = derive_sef_order_hint;
    if (derive_sef_order_hint == 0) {
      j["sef_order_hint"] = sef_order_hint;
    }
    if (apply_grain) {
      j["apply_grain"] = apply_grain;
      j["fgm_id"] = fgm_id;
      j["grain_seed"] = grain_seed;
    }
  }

  if (!ShowExistingFrame) {
    if (FrameType == SWITCH_FRAME) {
      j["restricted_prediction_switch"] = restricted_prediction_switch;
    }

    if (FrameType == KEY_FRAME && LongTermId >= 0) {
      j["long_term_id_plus_1"] = long_term_id_plus_1;
    }

    j["immediate_output_frame"] = immediate_output_frame;
    j["implicit_output_frame"] = implicit_output_frame;
    j["frame_size_override_flag"] = frame_size_override_flag;
    // For bridge frames, order_hint comes from RefOrderHintLsbs[bridge_frame_ref_idx]
    // (decoder ref state) — not from the bitstream — so we skip emission rather than
    // report the placeholder 0.
    if (!IsBridge) {
      j["order_hint"] = order_hint;
    }
    j["primary_ref_frame"] = primary_ref_frame;
    j["refresh_frame_flags"] = refresh_frame_flags;
    j["FrameWidth"] = FrameWidth;
    j["FrameHeight"] = FrameHeight;

    if (!FrameIsIntra) {
      j["NumTotalRefs"] = NumTotalRefs;
      if (!ref_frame_idx.empty()) {
        j["ref_frame_idx"] = ref_frame_idx;
      }
      j["use_bru"] = use_bru;
      if (use_bru) {
        j["bru_ref"] = bru_ref;
        j["bru_inactive"] = bru_inactive;
      }
      j["use_ref_frame_mvs"] = use_ref_frame_mvs;
      if (use_ref_frame_mvs) {
        j["tmvp_sample_step_minus_1"] = tmvp_sample_step_minus_1;
      }
    }

    if (IsBridge) {
      j["bridge_frame_ref_idx"] = bridge_frame_ref_idx;
      j["bridge_frame_overwrite_flag"] = bridge_frame_overwrite_flag;
    }

    if (apply_grain) {
      j["apply_grain"] = apply_grain;
      j["fgm_id"] = fgm_id;
      j["grain_seed"] = grain_seed;
    }
  }

  // Computed values not directly serialized in the bitstream (no f(N) descriptor).
  // Placed under a "derived" sub-object to keep them visually distinct from
  // bitstream-read fields above.
  json derived = json::object();
  derived["FrameIsIntra"] = FrameIsIntra;
  derived["ShowExistingFrame"] = ShowExistingFrame;
  derived["is_output_frame"] = is_output_frame;
  derived["TipFrameMode"] = TipFrameMode;
  if (FrameType == KEY_FRAME && LongTermId >= 0) {
    derived["LongTermId"] = LongTermId;
  }
  j["derived"] = derived;

  return j;
}

}  // namespace av2_obu
