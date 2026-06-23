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

#include <cstdint>
#include <string>
#include <vector>

#include <ISOMovies.h>
#include <MP4Movies.h>

#include "av2_codec_config.h"
#include "colr_info.h"

namespace av2_obu {

// Codec-agnostic libisomedia wrapper for a single AV2 video track.
class Mp4Writer {
public:
  Mp4Writer();
  ~Mp4Writer();

  Mp4Writer(const Mp4Writer&) = delete;
  Mp4Writer& operator=(const Mp4Writer&) = delete;

  bool add_video_track(uint32_t timescale, uint16_t width, uint16_t height,
                       const AV2CodecConfigurationBox& av2c);

  // Must be called after add_video_track() and before the first add_sample().
  bool add_colr_nclx(const ColrInfo& info);

  // Must be called before any add_sample()
  bool enable_signed_composition_offsets();

  // 0 disables auto-flush; caller drives chunk boundaries via flush_chunk()/finalize().
  void set_samples_per_chunk(uint32_t n) { samples_per_chunk_ = n; }

  bool add_sample(const std::vector<uint8_t>& bytes, uint32_t duration, bool is_sync,
                  int32_t composition_offset = 0);

  // One flush_chunk() = one MP4AddMediaSamples() call = one stsc chunk.
  bool flush_chunk();

  bool finalize(const std::string& output_path);

private:
  MP4Movie movie_ = nullptr;
  MP4Track track_ = nullptr;
  MP4Media media_ = nullptr;
  MP4Handle sample_entry_ = nullptr;

  // First chunk supplies sample_entry_; later chunks pass NULL to reuse it.
  bool first_chunk_ = true;

  std::vector<uint8_t> pending_data_;
  std::vector<uint32_t> pending_sizes_;
  std::vector<uint32_t> pending_durations_;
  std::vector<uint32_t> pending_sync_indices_;
  std::vector<int32_t> pending_ctts_offsets_;

  bool signed_ctts_enabled_ = false;
  uint32_t samples_per_chunk_ = 0;
};

}  // namespace av2_obu
