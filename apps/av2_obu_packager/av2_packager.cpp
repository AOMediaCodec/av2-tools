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

#include "av2_packager.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "av2_codec_config.h"
#include <av2_obu/core/base_obu.h>
#include <av2_obu/core/obu_parser.h>
#include <av2_obu/core/temporal_unit.h>
#include <av2_obu/obus/sequence_header_obu.h>

namespace av2_obu {

Av2Packager::Av2Packager(const std::string& input_path, const PackagingStrategy& strategy)
    : input_path_(input_path), strategy_(strategy) {}

bool Av2Packager::package(const OBUParser& parser, const std::string& output_path) {
  input_ifs_.open(input_path_, std::ios::binary);
  if (!input_ifs_) {
    spdlog::error("Failed to open {} for byte extraction", input_path_);
    return false;
  }

  if (!setup_video_track(parser)) return false;

  // Resolve TU range: [start, start + num_samples) clamped to available TUs.
  const auto& tus = parser.temporal_units();
  if (strategy_.start_tu >= tus.size()) {
    spdlog::error("--start-tu {} out of range (have {} TUs)", strategy_.start_tu, tus.size());
    return false;
  }
  if (strategy_.start_tu > 0 && !tus[strategy_.start_tu].is_sync_sample()) {
    spdlog::warn("--start-tu {} is not a sync sample; output may not be cleanly decodable",
                 strategy_.start_tu);
  }
  const uint32_t end_tu =
    strategy_.num_samples == 0
      ? static_cast<uint32_t>(tus.size())
      : std::min<uint32_t>(strategy_.start_tu + strategy_.num_samples,
                           static_cast<uint32_t>(tus.size()));
  spdlog::info("Writing TUs [{}, {}) of {} total", strategy_.start_tu, end_tu, tus.size());

  // 5.1: write only the first TU in the selected range. 5.2 will loop.
  if (!write_tu(tus[strategy_.start_tu])) return false;

  return writer_.finalize(output_path);
}

bool Av2Packager::setup_video_track(const OBUParser& parser) {
  const SequenceHeaderOBU* first_sh = find_first_sequence_header(parser);
  if (!first_sh) {
    spdlog::error("No Sequence Header OBU found");
    return false;
  }
  const AV2SequenceHeader& sh = first_sh->sequence_header();

  AV2CodecConfigurationBox av2c(input_ifs_);
  av2c.set_from_sequence_header(sh);
  if (!av2c.append_config_obu(*first_sh)) {
    spdlog::error("Failed to append SH to av2C configOBUs");
    return false;
  }

  uint32_t w_full = static_cast<uint32_t>(sh.max_frame_width_minus_1) + 1;
  uint32_t h_full = static_cast<uint32_t>(sh.max_frame_height_minus_1) + 1;
  if (w_full > 0xFFFF || h_full > 0xFFFF) {
    spdlog::warn("Frame dimensions {}x{} exceed 16-bit av2C field; truncating", w_full, h_full);
  }
  uint16_t width = static_cast<uint16_t>(w_full & 0xFFFF);
  uint16_t height = static_cast<uint16_t>(h_full & 0xFFFF);

  return writer_.add_video_track(strategy_.timescale, width, height, av2c);
}

bool Av2Packager::write_tu(const TemporalUnit& tu) {
  std::vector<uint8_t> bytes;
  if (!assemble_sample_bytes(tu, bytes)) return false;
  return writer_.add_sample(bytes, strategy_.default_sample_duration, tu.is_sync_sample());
}

bool Av2Packager::assemble_sample_bytes(const TemporalUnit& tu, std::vector<uint8_t>& out) {
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
      spdlog::error("Failed to read {} bytes for OBU at offset {}", total,
                    static_cast<long long>(pos.start_pos));
      return false;
    }
  }
  return true;
}

const SequenceHeaderOBU* Av2Packager::find_first_sequence_header(const OBUParser& parser) {
  for (const auto& obu : parser.obus()) {
    if (obu->type() == OBUType::SEQUENCE_HEADER) {
      if (auto* sh = dynamic_cast<const SequenceHeaderOBU*>(obu.get())) return sh;
    }
  }
  return nullptr;
}

void log_stream_summary(const OBUParser& parser) {
  const auto stats = parser.get_statistics();
  spdlog::debug("Stream summary:");
  spdlog::debug("  OBUs: {} ({} bytes)", stats.total_obus, stats.total_bytes);
  spdlog::debug("  Sequence headers: {}{}", stats.sequence_headers.count,
                stats.sequence_headers.has_changes ? " (with changes)" : " (identical)");
  spdlog::debug("  TDs: {}", stats.temporal.td_count);
  spdlog::debug("  Frames: {} ({} key)", stats.frames.total_frames, stats.frames.keyframe_count);
  spdlog::debug("  Single layer: {}", stats.layers.is_single_layer ? "yes" : "no");
  spdlog::debug("  CI timing_info: {}",
                stats.content_interpretation.has_timing_info ? "present" : "absent");

  size_t sync_count = 0;
  for (const auto& tu : parser.temporal_units()) {
    if (tu.is_sync_sample()) sync_count++;
  }
  spdlog::debug("  TUs: {} ({} sync samples)", parser.temporal_units().size(), sync_count);
}

}  // namespace av2_obu
