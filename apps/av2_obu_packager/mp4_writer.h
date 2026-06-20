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

// libisomedia
#include <ISOMovies.h>
#include <MP4Movies.h>

#include "av2_codec_config.h"

namespace av2_obu {

// Mp4Writer wraps libisomedia to produce an AV2-in-ISOBMFF .mp4. Owns the
class Mp4Writer {
public:
  Mp4Writer();
  ~Mp4Writer();

  Mp4Writer(const Mp4Writer&) = delete;
  Mp4Writer& operator=(const Mp4Writer&) = delete;

  // Create one video track and install its sample entry from av2c.
  bool add_video_track(uint32_t timescale, uint16_t width, uint16_t height,
                       const AV2CodecConfigurationBox& av2c);

  // Buffer a sample for the next chunk flush. duration is in track timescale units
  // Bytes are copied into an internal buffer; caller's vector can be freed after the call returns.
  bool add_sample(const std::vector<uint8_t>& bytes, uint32_t duration, bool is_sync);

  // Flush all currently buffered samples to libisomedia (one MP4AddMediaSamples call = one chunk in the stsc box).
  // No-op if buffer is empty. Auto-triggers by samples_per_chunk.
  bool flush_chunk();

  // Flush any pending samples and write the .mp4 to disk.
  bool finalize(const std::string& output_path);

private:
  MP4Movie movie_ = nullptr;
  MP4Track track_ = nullptr;
  MP4Media media_ = nullptr;
  MP4Handle sample_entry_ = nullptr;

  // First chunk passes sample_entry_ to MP4AddMediaSamples; subsequent chunks pass NULL to re-use it (per MP4Movies.h)
  bool first_chunk_ = true;

  // Pending-chunk buffer.
  std::vector<uint8_t> pending_data_;     // concatenated sample bytes
  std::vector<uint32_t> pending_sizes_;   // per-sample byte sizes
  std::vector<uint32_t> pending_durations_;
  std::vector<uint32_t> pending_sync_indices_;  // 1-based indices within this chunk
};

}  // namespace av2_obu
