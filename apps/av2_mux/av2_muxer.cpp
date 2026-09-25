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

#include "av2_muxer.h"
#include "mux_log.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "av2_codec_config.h"
#include "colr_info.h"
#include "frame_header_helpers.h"
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/frame_header_info.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/obus/sequence_header_obu.h>

namespace av2_obu {

Av2Muxer::Av2Muxer(const std::string& input_path, const MuxStrategy& strategy)
    : input_path_(input_path), strategy_(strategy) {}

bool Av2Muxer::package(const OBUParser& parser, const std::string& output_path) {
  input_ifs_.open(input_path_, std::ios::binary);
  if (!input_ifs_) {
    MUX_ERROR("Failed to open {} for byte extraction", input_path_);
    return false;
  }

  if (!setup_video_track(parser)) return false;

  const auto& tus = parser.temporal_units();
  if (strategy_.start_tu >= tus.size()) {
    MUX_ERROR("--start-tu {} out of range (have {} TUs)", strategy_.start_tu, tus.size());
    return false;
  }
  if (strategy_.start_tu > 0 && !tus[strategy_.start_tu].is_sync_sample()) {
    MUX_WARN("--start-tu {} is not a sync sample; output may not be cleanly decodable",
             strategy_.start_tu);
  }
  const uint32_t end_tu =
    strategy_.num_samples == 0
      ? static_cast<uint32_t>(tus.size())
      : std::min<uint32_t>(strategy_.start_tu + strategy_.num_samples,
                           static_cast<uint32_t>(tus.size()));
  MUX_DEBUG("Writing TUs [{}, {}) of {} total", strategy_.start_tu, end_tu, tus.size());

  // ISOBMFF §8.6.1.3 forbids ctts when CTS == DTS for every sample, so emit it
  // only after computing offsets and observing at least one non-zero.
  std::vector<int32_t> ctts_offsets = compute_composition_offsets(parser, tus,
                                                                  strategy_.start_tu, end_tu);
  bool need_ctts = std::any_of(ctts_offsets.begin(), ctts_offsets.end(),
                               [](int32_t v) { return v != 0; });
  if (need_ctts) {
    MUX_DEBUG("ctts: enabling signed composition offsets ({} non-zero)",
              std::count_if(ctts_offsets.begin(), ctts_offsets.end(),
                            [](int32_t v) { return v != 0; }));
    if (!writer_.enable_signed_composition_offsets()) return false;
  } else if (doh_lifter_) {
    MUX_DEBUG("ctts: omitted (non-monotonic SH but all CTS == DTS)");
  } else {
    MUX_DEBUG("ctts: omitted (monotonic_output_order_flag=1)");
  }

  uint32_t num_samples = 0;
  uint32_t sync_samples = 0;
  for (uint32_t i = strategy_.start_tu; i < end_tu; ++i) {
    int32_t ctts = need_ctts ? ctts_offsets[i - strategy_.start_tu] : 0;
    if (!write_tu(tus[i], i, ctts)) {
      MUX_ERROR("Failed to write TU {}", i);
      return false;
    }
    ++num_samples;
    if (tus[i].is_sync_sample()) ++sync_samples;
  }

  if (!writer_.finalize(output_path)) return false;

  std::string ctts_str = need_ctts ? "ctts=present" : "ctts=absent";
  std::string colr_str =
    last_colr_ ? fmt::format("colr=nclx {}/{}/{}/{}", last_colr_->colour_primaries,
                             last_colr_->transfer_characteristics,
                             last_colr_->matrix_coefficients,
                             last_colr_->full_range_flag)
               : "colr=absent";
  MUX_INFO("Wrote {} ({} samples, {} sync, {:.3f} fps, timescale={}, {}, {})", output_path,
           num_samples, sync_samples, strategy_.frame_rate, strategy_.timescale, ctts_str,
           colr_str);
  return true;
}

namespace {

std::vector<uint8_t> read_obu_bytes(std::ifstream& ifs, const BaseOBU& o) {
  const auto& pos = o.position();
  size_t total = pos.size_field_len + pos.header_len + pos.payload_size;
  std::vector<uint8_t> b(total);
  ifs.clear();
  ifs.seekg(pos.start_pos);
  ifs.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(total));
  ifs.clear();
  return b;
}

}  // namespace

bool Av2Muxer::setup_video_track(const OBUParser& parser) {
  if (parser.temporal_units().empty()) {
    MUX_ERROR("No temporal units found");
    return false;
  }

  // All extended layers of a temporal unit share the same timing info.
  // The base layer's frames can drive the CTS computation.
  base_xlayer_id_ = GLOBAL_XLAYER_ID;
  for (const auto& o : parser.obus()) {
    if (!frame_header_of(o.get())) continue;
    const uint32_t x = o->header().get_xlayer_id();
    if (x == GLOBAL_XLAYER_ID) continue;
    if (base_xlayer_id_ == GLOBAL_XLAYER_ID || x < base_xlayer_id_) base_xlayer_id_ = x;
  }

  const auto& tus = parser.temporal_units();
  tu_desc_idx_.assign(tus.size(), 0);

  std::vector<uint8_t> last_config_bytes;
  uint32_t active_desc_idx = 0;

  for (uint32_t i = 0; i < tus.size(); ++i) {
    if (!tus[i].is_sync_sample()) {
      tu_desc_idx_[i] = active_desc_idx;
      continue;
    }

    AV2CodecConfigurationBox av2c(input_ifs_);
    std::vector<uint8_t> config_bytes;
    const SequenceHeaderOBU* sh = nullptr;
    size_t config_count = 0;

    for (const BaseOBU* p : tus[i]) {
      if (!is_config_obu(p->type())) continue;
      if (p->type() == OBUType::SEQUENCE_HEADER && !sh) {
        sh = dynamic_cast<const SequenceHeaderOBU*>(p);
      }
      auto raw = read_obu_bytes(input_ifs_, *p);
      config_bytes.insert(config_bytes.end(), raw.begin(), raw.end());
      ++config_count;
      if (!av2c.append_config_obu(*p)) {
        MUX_ERROR("Failed to append config OBU (type {}) to av2C configOBUs",
                  static_cast<int>(p->type()));
        return false;
      }
    }

    if (!sh) {
      MUX_ERROR("Sync sample TU {} has no Sequence Header OBU", i);
      return false;
    }
    const AV2SequenceHeader& sequence_header = sh->sequence_header();
    av2c.set_from_sequence_header(sequence_header);

    if (!config_bytes.empty() && config_bytes == last_config_bytes) {
      tu_desc_idx_[i] = active_desc_idx;
      continue;
    }
    last_config_bytes = config_bytes;

    std::optional<ColrInfo> colr =
      strategy_.colr_override ? strategy_.colr_override : extract_colr_info_from_tu(tus[i]);

    uint32_t w_full = static_cast<uint32_t>(sequence_header.max_frame_width_minus_1) + 1;
    uint32_t h_full = static_cast<uint32_t>(sequence_header.max_frame_height_minus_1) + 1;
    if (w_full > 0xFFFF || h_full > 0xFFFF) {
      MUX_WARN("Frame dimensions {}x{} exceed 16-bit av2C field; truncating", w_full, h_full);
    }
    uint16_t width = static_cast<uint16_t>(w_full & 0xFFFF);
    uint16_t height = static_cast<uint16_t>(h_full & 0xFFFF);

    uint32_t new_desc_idx;
    if (sample_entries_.empty()) {
      strategy_.any_non_monotonic = (sequence_header.monotonic_output_order_flag == 0);
      if (strategy_.any_non_monotonic) {
        if (!check_doh_lifter_supported(parser)) return false;
        doh_lifter_ =
          std::make_unique<DisplayOrderLifter>(sequence_header.inter_config.OrderHintBits);
      }

      if (!writer_.add_video_track(strategy_.timescale, width, height, av2c)) return false;
      writer_.set_samples_per_chunk(strategy_.samples_per_chunk);
      if (colr && !writer_.add_colr_nclx(*colr)) return false;
      new_desc_idx = 1;
    } else {
      new_desc_idx = writer_.add_sample_entry(av2c, width, height, colr);
      if (new_desc_idx == 0) return false;
      MUX_INFO("CVS boundary at TU {}: new sample entry (desc_idx={})", i, new_desc_idx);
    }

    MUX_DEBUG("configOBUs (desc_idx={}): {} OBU(s)", new_desc_idx, config_count);
    if (colr) {
      MUX_DEBUG("colr/nclx ({}): cp={} tc={} mc={} full_range={}",
                strategy_.colr_override ? "override" : "extracted from bitstream",
                colr->colour_primaries, colr->transfer_characteristics, colr->matrix_coefficients,
                colr->full_range_flag);
    } else {
      MUX_DEBUG("No CI/LCR/OPS color info found and no --colr-override; omitting colr box");
    }
    last_colr_ = colr;

    sample_entries_.push_back({av2c, colr, new_desc_idx, config_bytes});
    active_desc_idx = new_desc_idx;
    tu_desc_idx_[i] = active_desc_idx;
  }

  if (sample_entries_.empty()) {
    MUX_ERROR("No sync sample found in stream");
    return false;
  }

  if (sample_entries_.size() > 1) {
    MUX_DEBUG("Multi-CVS stream: {} sample entries", sample_entries_.size());
    check_cmaf_invariants(sample_entries_);
  }

  return true;
}

bool Av2Muxer::check_doh_lifter_supported(const OBUParser& parser) {
  // For multi-layer streams the lifter runs on the base extended layer only.
  // Multi-CVS non-monotonic streams are supported: the lifter is reset at each
  // CVS boundary (see compute_composition_offsets()).
  for (const auto& obu : parser.obus()) {
    if (obu->header().get_xlayer_id() != base_xlayer_id_) continue;
    if (obu->type() == OBUType::BRIDGE_FRAME) {
      MUX_ERROR("Non-monotonic stream contains a Bridge frame at OBU offset {}; its "
                "DispOrderHint is RefOrderHint[bridge_frame_ref_idx], which the v1 lifter "
                "cannot resolve without reference-state tracking.",
                static_cast<long long>(obu->position().start_pos));
      return false;
    }
    const FrameHeaderInfo* fh = frame_header_of(obu.get());
    if (!fh) continue;
    if (is_sef(obu->type()) && fh->derive_sef_order_hint == 1) {
      MUX_ERROR("Non-monotonic stream contains a derived-DOH SEF at OBU offset {} "
                "(derive_sef_order_hint=1); its order_hint comes from the reference "
                "buffer, which the v1 lifter cannot resolve.",
                static_cast<long long>(obu->position().start_pos));
      return false;
    }
  }
  return true;
}

bool Av2Muxer::write_tu(const TemporalUnit& tu, uint32_t tu_index, int32_t composition_offset) {
  std::vector<uint8_t> bytes;
  if (!assemble_sample_bytes(tu, bytes)) return false;
  uint32_t desc_idx = tu_desc_idx_[tu_index];
  return writer_.add_sample(bytes, strategy_.default_sample_duration, tu.is_sync_sample(),
                            composition_offset, desc_idx);
}

std::vector<int32_t> Av2Muxer::compute_composition_offsets(
    const OBUParser& parser, const std::vector<TemporalUnit>& tus, uint32_t start, uint32_t end) {
  std::vector<int32_t> offsets;
  if (!doh_lifter_) return offsets;
  offsets.reserve(end - start);
  const int64_t dur = static_cast<int64_t>(strategy_.default_sample_duration);
  const SequenceHeaderOBU* first_sh = find_first_sequence_header(parser);
  const AV2SequenceHeader* sh = &first_sh->sequence_header();

  for (uint32_t i = start; i < end; ++i) {
    int32_t off = 0;
    int64_t output_doh = -1;

    if (tus[i].is_sync_sample() && i > start) {
      if (const SequenceHeaderOBU* cvs_sh = find_sh_in_tu(tus[i], base_xlayer_id_)) {
        doh_lifter_ =
          std::make_unique<DisplayOrderLifter>(cvs_sh->sequence_header().inter_config.OrderHintBits);
        ref_buffer_ = RefFrameBuffer{};
        sh = &cvs_sh->sequence_header();
      }
    }

    // Feed only the base extended layer's frames to the lifter (sample has shared timing for all layers)
    for (const auto* obu : tus[i].obus()) {
      if (obu->header().get_xlayer_id() != base_xlayer_id_) continue;
      const FrameHeaderInfo* fh = frame_header_of(obu);
      if (!fh) continue;
      const int64_t doh = doh_lifter_->process(*obu, *fh, *sh, ref_buffer_);
      if (fh->is_output_frame) output_doh = doh;
    }

    if (output_doh >= 0) {
      const int64_t cts = output_doh * dur;
      const int64_t dts = static_cast<int64_t>(i - start) * dur;
      off = static_cast<int32_t>(cts - dts);
    }
    // Hidden-only TUs: leave CTS == DTS (off stays 0).
    offsets.push_back(off);
  }
  return offsets;
}

bool Av2Muxer::assemble_sample_bytes(const TemporalUnit& tu, std::vector<uint8_t>& out) {
  out.clear();
  const bool keep_td = !strategy_.drop_temporal_delimiters;
  for (const auto* obu : tu.sample_obus(keep_td)) {
    const auto& pos = obu->position();
    size_t total = pos.size_field_len + pos.header_len + pos.payload_size;
    size_t prev = out.size();
    out.resize(prev + total);

    input_ifs_.clear();
    input_ifs_.seekg(pos.start_pos);
    if (!input_ifs_.read(reinterpret_cast<char*>(out.data() + prev),
                         static_cast<std::streamsize>(total))) {
      MUX_ERROR("Failed to read {} bytes for OBU at offset {}", total,
                static_cast<long long>(pos.start_pos));
      return false;
    }
  }
  return true;
}

const SequenceHeaderOBU* Av2Muxer::find_first_sequence_header(const OBUParser& parser) {
  for (const auto& obu : parser.obus()) {
    if (obu->type() == OBUType::SEQUENCE_HEADER) {
      if (auto* sh = dynamic_cast<const SequenceHeaderOBU*>(obu.get())) return sh;
    }
  }
  return nullptr;
}

const SequenceHeaderOBU* Av2Muxer::find_sh_in_tu(const TemporalUnit& tu, uint32_t xlayer_id) {
  for (const BaseOBU* obu : tu.obus()) {
    if (obu->type() != OBUType::SEQUENCE_HEADER) continue;
    if (obu->header().get_xlayer_id() != xlayer_id) continue;
    if (auto* sh = dynamic_cast<const SequenceHeaderOBU*>(obu)) return sh;
  }
  return nullptr;
}

void Av2Muxer::check_cmaf_invariants(const std::vector<SampleEntryRecord>& entries) {
  const auto& first = entries.front().av2c;
  bool color_warned = false;
  for (size_t i = 1; i < entries.size(); ++i) {
    const auto& e = entries[i].av2c;
    if (e.seq_profile_idc != first.seq_profile_idc) {
      MUX_WARN("Sample entry {} has seq_profile_idc={} (first={}); players may not support "
               "switching profiles mid-stream.",
               i, e.seq_profile_idc, first.seq_profile_idc);
    }
    if (e.still_picture != first.still_picture) {
      MUX_WARN("Sample entry {} has still_picture={} (first={})", i, e.still_picture,
                first.still_picture);
    }
    if (e.seq_level_idx != first.seq_level_idx) {
      MUX_WARN("Sample entry {} has seq_level_idx={} (first={})", i, e.seq_level_idx,
                first.seq_level_idx);
    }
    if (e.seq_tier != first.seq_tier) {
      MUX_WARN("Sample entry {} has seq_tier={} (first={})", i, e.seq_tier, first.seq_tier);
    }
    if (e.seq_initial_display_delay_minus_1 != first.seq_initial_display_delay_minus_1) {
      MUX_WARN("Sample entry {} has seq_initial_display_delay_minus_1={} (first={})", i,
                e.seq_initial_display_delay_minus_1, first.seq_initial_display_delay_minus_1);
    }
    if (!color_warned && entries[i].colr.has_value() != entries.front().colr.has_value()) {
      MUX_WARN("Sample entry {} colr presence differs from first sample entry", i);
      color_warned = true;
    } else if (!color_warned && entries[i].colr && entries.front().colr) {
      const auto& c0 = *entries.front().colr;
      const auto& ci = *entries[i].colr;
      if (c0.colour_primaries != ci.colour_primaries ||
          c0.transfer_characteristics != ci.transfer_characteristics ||
          c0.matrix_coefficients != ci.matrix_coefficients ||
          c0.full_range_flag != ci.full_range_flag) {
        MUX_WARN("Sample entry {} colr/nclx differs from first sample entry", i);
        color_warned = true;
      }
    }
  }
}

void log_stream_summary(const OBUParser& parser) {
  const auto stats = parser.get_statistics();
  MUX_DEBUG("Stream summary:");
  MUX_DEBUG("  OBUs: {} ({} bytes)", stats.total_obus, stats.total_bytes);
  MUX_DEBUG("  Sequence headers: {}{}", stats.sequence_headers.count,
            stats.sequence_headers.has_changes ? " (with changes)" : " (identical)");
  MUX_DEBUG("  TDs: {}", stats.temporal.td_count);
  MUX_DEBUG("  Frames: {} ({} key)", stats.frames.total_frames, stats.frames.keyframe_count);
  MUX_DEBUG("  Single layer: {}", stats.layers.is_single_layer ? "yes" : "no");
  MUX_DEBUG("  CI timing_info: {}",
            stats.content_interpretation.has_timing_info ? "present" : "absent");

  size_t sync_count = 0;
  for (const auto& tu : parser.temporal_units()) {
    if (tu.is_sync_sample()) sync_count++;
  }
  MUX_DEBUG("  TUs: {} ({} sync samples)", parser.temporal_units().size(), sync_count);
}

}  // namespace av2_obu
