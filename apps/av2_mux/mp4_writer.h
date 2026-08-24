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
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <ISOMovies.h>
#include <MP4Movies.h>

#include "av2_codec_config.h"
#include "colr_info.h"

namespace av2_obu {

// Codec-agnostic libisomedia wrapper for a single AV2 video track. Supports
// multiple sample entries (stsd) on the track, one per CVS whose configOBUs
// differ from the previous CVS.
class Mp4Writer {
public:
  Mp4Writer();
  ~Mp4Writer();

  Mp4Writer(const Mp4Writer&) = delete;
  Mp4Writer& operator=(const Mp4Writer&) = delete;

  // Creates the track and its first sample entry (desc_idx == 1).
  bool add_video_track(uint32_t timescale, uint16_t width, uint16_t height,
                       const AV2CodecConfigurationBox& av2c);

  // Must be called after add_video_track() and before the first add_sample().
  // Attaches colr to the first sample entry (desc_idx == 1).
  bool add_colr_nclx(const ColrInfo& info);

  // Creates an additional sample entry on the same track (CVS boundary with
  // changed configOBUs). Returns the assigned desc_idx (>= 2), or 0 on failure.
  uint32_t add_sample_entry(const AV2CodecConfigurationBox& av2c, uint16_t width,
                            uint16_t height, const std::optional<ColrInfo>& colr);

  // Must be called before any add_sample()
  bool enable_signed_composition_offsets();

  // 0 disables auto-flush; caller drives chunk boundaries via flush_chunk()/finalize().
  void set_samples_per_chunk(uint32_t n) { samples_per_chunk_ = n; }

  // desc_idx selects which sample entry (from add_video_track()/add_sample_entry())
  // this sample belongs to. Changing desc_idx from the previous call forces a
  // chunk flush first, since a chunk cannot span sample entries.
  bool add_sample(const std::vector<uint8_t>& bytes, uint32_t duration, bool is_sync,
                  int32_t composition_offset, uint32_t desc_idx);

  // One flush_chunk() = one MP4AddMediaSamples() call = one stsc chunk.
  bool flush_chunk();

  bool finalize(const std::string& output_path);

private:
  MP4Movie movie_ = nullptr;
  MP4Track track_ = nullptr;
  MP4Media media_ = nullptr;

  // Sample entries by desc_idx (1-based, matching stsd entry order).
  std::map<uint32_t, MP4Handle> sample_entries_;
  uint32_t next_desc_idx_ = 0;

  // desc_idx of the samples currently buffered in pending_*.
  uint32_t current_desc_idx_ = 0;

  // desc_idx values already passed to MP4AddMediaSamples as sampleEntryH.
  // libisomedia appends a brand-new stsd entry every time a non-null
  // sampleEntryH is passed, so each entry may be attached only once; later
  // chunks reusing that entry must pass NULL to keep it "current".
  std::set<uint32_t> entries_attached_;

  std::vector<uint8_t> pending_data_;
  std::vector<uint32_t> pending_sizes_;
  std::vector<uint32_t> pending_durations_;
  std::vector<uint32_t> pending_sync_indices_;
  std::vector<int32_t> pending_ctts_offsets_;

  bool signed_ctts_enabled_ = false;
  uint32_t samples_per_chunk_ = 0;

  bool attach_colr_nclx(MP4Handle entry, const ColrInfo& info);
};

}  // namespace av2_obu
