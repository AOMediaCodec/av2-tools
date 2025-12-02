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

#include "bitstream_switcher.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

using namespace av2_obu;

BitstreamSwitcher::BitstreamSwitcher(double fps) : fps_(fps) {}

bool BitstreamSwitcher::attach_streams(const std::vector<std::string>& names) {
  streams_.clear();
  streams_.reserve(names.size());
  for (const auto& p : names) {
    Stream s;
    s.path = p;
    s.parser = std::make_unique<OBUParser>();
    if (!s.parser->parse_file(p)) {
      spdlog::error("Failed to parse stream: {}", p);
      streams_.clear();
      return false;
    }
    streams_.push_back(std::move(s));
  }
  return true;
}

size_t BitstreamSwitcher::find_switch_index(const Stream& s, double t, OBUType type) const {
  // Frame number from timestamp (OBUs treated as frame granularity per your note)
  const size_t target_frame = static_cast<size_t>(std::round(t * fps_));
  // Search for entries of the desired type and pick closest to target_frame.
  // Assume alignment across streams: same count/order of type at similar frames.
  size_t best = 0;
  size_t best_dist = std::numeric_limits<size_t>::max();
  size_t frame_counter = 0;

  const auto& obus = s.parser->obus();
  for (size_t i = 0; i < obus.size(); ++i) {
    const auto& obu = obus[i];
    // Treat any frame-carrying OBU as advancing "frame_counter".
    // Note: These OBU types may need updating for AV2
    if (obu->type() == OBUType::LEADING_TILE_GROUP || obu->type() == OBUType::REGULAR_TILE_GROUP) {
      // frame boundary reached; evaluate if this entry matches requested type
      if (obu->type() == type) {
        size_t dist = (frame_counter > target_frame) ? (frame_counter - target_frame)
                                                     : (target_frame - frame_counter);
        if (dist < best_dist) {
          best_dist = dist;
          best = i;
        }
      }
      ++frame_counter;
    }
  }
  if (best_dist == std::numeric_limits<size_t>::max()) {
    // Fallback: no such OBU type; choose last byte (shouldn't happen if aligned)
    return obus.empty() ? 0 : obus.size() - 1;
  }
  spdlog::info("Switch on {} @ frame {} -> picked index {}", to_string(type),
               (best_dist == 0 ? target_frame : target_frame), best);
  return best;
}

bool BitstreamSwitcher::copy_range(const Stream& from, size_t ibegin, size_t iend,
                                   std::ofstream& ofs) const {
  const auto& obus = from.parser->obus();
  if (ibegin > iend || iend > obus.size())
    return false;
  std::ifstream ifs(from.path, std::ios::binary);
  if (!ifs)
    return false;

  size_t total_bytes_copied = 0;
  long long first_offset = -1;
  long long last_offset = -1;

  for (size_t i = ibegin; i < iend; ++i) {
    const auto& obu = obus[i];
    const auto& pos = obu->position();
    const auto start = pos.start_pos;
    const auto end = pos.end_pos;
    size_t obu_total_bytes = static_cast<size_t>(end - start);

    if (first_offset < 0)
      first_offset = static_cast<long long>(start);
    last_offset = static_cast<long long>(end);

    ifs.clear();
    ifs.seekg(start);
    std::vector<char> buf(obu_total_bytes);
    if (!ifs.read(buf.data(), buf.size()))
      return false;
    if (!ofs.write(buf.data(), buf.size()))
      return false;

    total_bytes_copied += obu_total_bytes;

    spdlog::debug("  Copied OBU #{}: {} bytes from offset {} (type: {})", i, obu_total_bytes,
                  static_cast<long long>(start), obu->type_name());
  }

  if (total_bytes_copied > 0) {
    spdlog::info("  Copied {} OBUs, {} bytes total [file offset {}-{}] from '{}'", iend - ibegin,
                 total_bytes_copied, first_offset, last_offset, from.path);
  }

  return true;
}

bool BitstreamSwitcher::stitch(const std::vector<size_t>& stream_order,
                               const std::vector<SwitchPoint>& switch_points,
                               const std::string& output_path) {
  if (stream_order.empty() || streams_.empty())
    return false;
  if (stream_order.size() != switch_points.size() + 1) {
    spdlog::error("stream_order must be size = switch_points + 1");
    return false;
  }
  std::ofstream ofs(output_path, std::ios::binary);
  if (!ofs)
    return false;

  // Start on stream_order[0], copy from frame 0 (index 0) up to switch index.
  size_t cur_stream_id = stream_order[0];
  const Stream* cur = &streams_[cur_stream_id];
  size_t prev_end_index = 0;

  for (size_t sp = 0; sp < switch_points.size(); ++sp) {
    const auto& sw = switch_points[sp];
    const size_t switch_index = find_switch_index(*cur, sw.timestamp_sec, sw.on_obu_type);

    spdlog::info("Switch point {}: time={:.3f}s, type={}, switching at OBU index {}", sp,
                 sw.timestamp_sec, to_string(sw.on_obu_type), switch_index);

    // Copy [prev_end_index, switch_index) from current stream.
    if (!copy_range(*cur, prev_end_index, switch_index, ofs))
      return false;

    // Jump to next stream in order; start copying from that same switch_index.
    cur_stream_id = stream_order[sp + 1];
    cur = &streams_[cur_stream_id];
    prev_end_index = switch_index;
  }

  spdlog::info("Final segment from stream #{}: OBU index {} to EOF", cur_stream_id, prev_end_index);

  // Final leg: copy from prev_end_index to EOF of the last stream only.
  if (!copy_range(*cur, prev_end_index, cur->parser->obus().size(), ofs))
    return false;
  return true;
}
