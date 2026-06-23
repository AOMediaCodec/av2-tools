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
    if (!write_tu(tus[i], ctts)) {
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

bool Av2Muxer::setup_video_track(const OBUParser& parser) {
  const SequenceHeaderOBU* first_sh = find_first_sequence_header(parser);
  if (!first_sh) {
    MUX_ERROR("No Sequence Header OBU found");
    return false;
  }
  const AV2SequenceHeader& sh = first_sh->sequence_header();

  // Multi-sample-entry on SH change is not implemented yet; warn so users know
  // later SHs will be silently lost from av2C.
  const auto& sh_stats = parser.get_statistics().sequence_headers;
  if (sh_stats.has_changes) {
    MUX_WARN(
      "Bitstream contains {} sequence header(s) with differing bytes (changes at OBU indices: "
      "{}). Multi-sample-entry support is not implemented; only the first SH (seq_header_id={}) "
      "will be carried in av2C. Samples that depend on a later SH may decode incorrectly.",
      sh_stats.count, [&] {
        std::string out;
        for (size_t i = 0; i < sh_stats.change_positions.size(); ++i) {
          if (i) out += ", ";
          out += std::to_string(sh_stats.change_positions[i]);
        }
        return out;
      }(),
      sh.seq_header_id);
  }

  strategy_.any_non_monotonic = (sh.monotonic_output_order_flag == 0);
  if (strategy_.any_non_monotonic) {
    if (!check_doh_lifter_supported(parser)) return false;
    doh_lifter_ = std::make_unique<DisplayOrderLifter>(sh.inter_config.OrderHintBits);
  }

  AV2CodecConfigurationBox av2c(input_ifs_);
  av2c.set_from_sequence_header(sh);
  if (!av2c.append_config_obu(*first_sh)) {
    MUX_ERROR("Failed to append SH to av2C configOBUs");
    return false;
  }

  uint32_t w_full = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
  uint32_t h_full = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
  if (w_full > 0xFFFF || h_full > 0xFFFF) {
    MUX_WARN("Frame dimensions {}x{} exceed 16-bit av2C field; truncating", w_full, h_full);
  }
  uint16_t width = static_cast<uint16_t>(w_full & 0xFFFF);
  uint16_t height = static_cast<uint16_t>(h_full & 0xFFFF);

  if (!writer_.add_video_track(strategy_.timescale, width, height, av2c)) return false;
  writer_.set_samples_per_chunk(strategy_.samples_per_chunk);

  std::optional<ColrInfo> colr =
    strategy_.colr_override ? strategy_.colr_override : extract_colr_info(parser);
  if (colr) {
    if (!writer_.add_colr_nclx(*colr)) return false;
    MUX_DEBUG("colr/nclx ({}): cp={} tc={} mc={} full_range={}",
              strategy_.colr_override ? "override" : "extracted from bitstream",
              colr->colour_primaries, colr->transfer_characteristics, colr->matrix_coefficients,
              colr->full_range_flag);
  } else {
    MUX_DEBUG("No CI/LCR/OPS color info found and no --colr-override; omitting colr box");
  }
  last_colr_ = colr;

  return true;
}

bool Av2Muxer::check_doh_lifter_supported(const OBUParser& parser) {
  // The simple modular-unwrap lifter is only correct for a narrow class of
  // streams; refuse anything outside it rather than silently producing bad CTS.
  // See display_order_lifter.h for the full list of preconditions.
  const auto stats = parser.get_statistics();
  if (!stats.layers.is_single_layer) {
    MUX_ERROR("Non-monotonic stream uses {} mlayer(s) and {} xlayer(s); the v1 DOH lifter "
              "supports single-layer streams only. Refusing to produce wrong ctts.",
              stats.layers.mlayer_ids.size(), stats.layers.xlayer_ids.size());
    return false;
  }
  uint32_t clk_count = 0;
  for (const auto& obu : parser.obus()) {
    if (obu->type() == OBUType::CLK) ++clk_count;
  }
  if (clk_count > 1) {
    MUX_ERROR("Non-monotonic stream contains {} CLK frames (multi-CVS); the v1 DOH lifter "
              "does not reset across CVS boundaries.", clk_count);
    return false;
  }
  for (const auto& obu : parser.obus()) {
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

bool Av2Muxer::write_tu(const TemporalUnit& tu, int32_t composition_offset) {
  std::vector<uint8_t> bytes;
  if (!assemble_sample_bytes(tu, bytes)) return false;
  return writer_.add_sample(bytes, strategy_.default_sample_duration, tu.is_sync_sample(),
                            composition_offset);
}

std::vector<int32_t> Av2Muxer::compute_composition_offsets(
    const OBUParser& parser, const std::vector<TemporalUnit>& tus, uint32_t start, uint32_t end) {
  std::vector<int32_t> offsets;
  if (!doh_lifter_) return offsets;
  offsets.reserve(end - start);
  const int64_t dur = static_cast<int64_t>(strategy_.default_sample_duration);
  const SequenceHeaderOBU* first_sh = find_first_sequence_header(parser);
  const AV2SequenceHeader& sh = first_sh->sequence_header();

  for (uint32_t i = start; i < end; ++i) {
    int32_t off = 0;
    int64_t output_doh = -1;

    // Process every coded frame OBU in this TU through the lifter so the ref
    // buffer reflects the bitstream's decode-order state. The output frame's
    // lifted DOH (the last is_output_frame=true frame in decode order, if any)
    // determines this sample's ctts.
    for (const auto* obu : tus[i].obus()) {
      const FrameHeaderInfo* fh = frame_header_of(obu);
      if (!fh) continue;
      const int64_t doh = doh_lifter_->process(*obu, *fh, sh, ref_buffer_);
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
