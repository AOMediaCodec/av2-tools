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

#include "mp4_writer.h"
#include "mux_log.h"

#include <cstring>

#include <spdlog/spdlog.h>

namespace av2_obu {

namespace {

constexpr uint32_t kBrandAv02 = MP4_FOUR_CHAR_CODE('a', 'v', '0', '2');
constexpr uint32_t kBrandIso6 = MP4_FOUR_CHAR_CODE('i', 's', 'o', '6');
constexpr uint32_t kAtomTypeAv2C = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'C');
constexpr uint32_t kSampleEntryTypeAv02 = MP4_FOUR_CHAR_CODE('a', 'v', '0', '2');
constexpr uint32_t kAtomTypeColr = MP4_FOUR_CHAR_CODE('c', 'o', 'l', 'r');
constexpr uint32_t kColourTypeNclx = MP4_FOUR_CHAR_CODE('n', 'c', 'l', 'x');

// Allocate an MP4Handle and copy bytes into it.
MP4Handle make_handle(const void* data, size_t size) {
  MP4Handle h = nullptr;
  if (MP4NewHandle(static_cast<u32>(size), &h) != MP4NoErr || !h) return nullptr;
  if (size > 0) std::memcpy(*h, data, size);
  return h;
}

}  // namespace

Mp4Writer::Mp4Writer() {
  // 0xff in each profile slot = "no profile" per libisomedia conventions.
  MP4Err err = MP4NewMovie(&movie_, /*initialODID=*/1,
                           /*OD*/ 0xff, /*scene*/ 0xff,
                           /*audio*/ 0xff, /*visual*/ 0xff,
                           /*graphics*/ 0xff);
  if (err != MP4NoErr) {
    MUX_ERROR("MP4NewMovie failed (err={})", err);
    movie_ = nullptr;
    return;
  }

  ISOSetMovieBrand(movie_, kBrandAv02, /*minor=*/0);
  ISOSetMovieCompatibleBrand(movie_, kBrandAv02);
  ISOSetMovieCompatibleBrand(movie_, kBrandIso6);
}

Mp4Writer::~Mp4Writer() {
  for (auto& [idx, handle] : sample_entries_) {
    if (handle) MP4DisposeHandle(handle);
  }
  if (movie_) MP4DisposeMovie(movie_);
}

bool Mp4Writer::add_video_track(uint32_t timescale, uint16_t width, uint16_t height,
                                const AV2CodecConfigurationBox& av2c) {
  if (!movie_) return false;

  MP4Err err = MP4NewMovieTrack(movie_, MP4NewTrackIsVisual, &track_);
  if (err != MP4NoErr || !track_) {
    MUX_ERROR("MP4NewMovieTrack failed (err={})", err);
    return false;
  }

  err = MP4NewTrackMedia(track_, &media_, MP4VisualHandlerType, timescale, /*dataRef=*/nullptr);
  if (err != MP4NoErr || !media_) {
    MUX_ERROR("MP4NewTrackMedia failed (err={})", err);
    return false;
  }

  uint32_t desc_idx = add_sample_entry(av2c, width, height, std::nullopt);
  return desc_idx == 1;
}

bool Mp4Writer::attach_colr_nclx(MP4Handle entry, const ColrInfo& info) {
  // TODO(https://github.com/MPEGGroup/isobmff/issues/76): replace with a libisomedia helper once landed.
  // ColourInformationBox
  uint8_t payload[11];
  payload[0] = static_cast<uint8_t>((kColourTypeNclx >> 24) & 0xFF);
  payload[1] = static_cast<uint8_t>((kColourTypeNclx >> 16) & 0xFF);
  payload[2] = static_cast<uint8_t>((kColourTypeNclx >> 8) & 0xFF);
  payload[3] = static_cast<uint8_t>(kColourTypeNclx & 0xFF);
  payload[4] = static_cast<uint8_t>((info.colour_primaries >> 8) & 0xFF);
  payload[5] = static_cast<uint8_t>(info.colour_primaries & 0xFF);
  payload[6] = static_cast<uint8_t>((info.transfer_characteristics >> 8) & 0xFF);
  payload[7] = static_cast<uint8_t>(info.transfer_characteristics & 0xFF);
  payload[8] = static_cast<uint8_t>((info.matrix_coefficients >> 8) & 0xFF);
  payload[9] = static_cast<uint8_t>(info.matrix_coefficients & 0xFF);
  payload[10] = static_cast<uint8_t>((info.full_range_flag & 0x1) << 7);

  MP4Handle h = make_handle(payload, sizeof(payload));
  if (!h) {
    MUX_ERROR("MP4NewHandle(colr payload) failed");
    return false;
  }

  MP4GenericAtom atom = nullptr;
  MP4Err err = MP4NewForeignAtom(&atom, kAtomTypeColr, h);
  if (err != MP4NoErr || !atom) {
    MUX_ERROR("MP4NewForeignAtom('colr') failed (err={})", err);
    MP4DisposeHandle(h);
    return false;
  }

  err = ISOAddAtomToSampleDescription(entry, atom);
  if (err != MP4NoErr) {
    MUX_ERROR("ISOAddAtomToSampleDescription('colr') failed (err={})", err);
    return false;
  }

  MUX_DEBUG("Attached colr/nclx (cp={} tc={} mc={} fr={})", info.colour_primaries,
                info.transfer_characteristics, info.matrix_coefficients,
                info.full_range_flag);
  return true;
}

bool Mp4Writer::add_colr_nclx(const ColrInfo& info) {
  auto it = sample_entries_.find(1);
  if (it == sample_entries_.end()) return false;
  return attach_colr_nclx(it->second, info);
}

uint32_t Mp4Writer::add_sample_entry(const AV2CodecConfigurationBox& av2c, uint16_t width,
                                     uint16_t height, const std::optional<ColrInfo>& colr) {
  if (!track_) return 0;

  std::vector<uint8_t> av2c_bytes = av2c.serialize();
  if (av2c_bytes.empty()) {
    MUX_ERROR("av2C serialization produced empty payload");
    return 0;
  }

  MP4Handle av2c_payload = make_handle(av2c_bytes.data(), av2c_bytes.size());
  if (!av2c_payload) {
    MUX_ERROR("MP4NewHandle(av2C payload) failed");
    return 0;
  }

  MP4GenericAtom av2c_atom = nullptr;
  MP4Err err = MP4NewForeignAtom(&av2c_atom, kAtomTypeAv2C, av2c_payload);
  if (err != MP4NoErr || !av2c_atom) {
    MUX_ERROR("MP4NewForeignAtom('av2C') failed (err={})", err);
    MP4DisposeHandle(av2c_payload);
    return 0;
  }

  MP4Handle entry = nullptr;
  err = MP4NewHandle(0, &entry);
  if (err != MP4NoErr || !entry) {
    MUX_ERROR("MP4NewHandle(sample_entry) failed (err={})", err);
    return 0;
  }

  err = ISONewGeneralSampleDescription(track_, entry, /*dataRefIdx=*/1, kSampleEntryTypeAv02,
                                       av2c_atom);
  if (err != MP4NoErr) {
    MUX_ERROR("ISONewGeneralSampleDescription failed (err={})", err);
    MP4DisposeHandle(entry);
    return 0;
  }

  err = ISOSetSampleDescriptionDimensions(entry, width, height);
  if (err != MP4NoErr) {
    MUX_ERROR("ISOSetSampleDescriptionDimensions failed (err={})", err);
    MP4DisposeHandle(entry);
    return 0;
  }

  if (colr && !attach_colr_nclx(entry, *colr)) {
    MP4DisposeHandle(entry);
    return 0;
  }

  uint32_t desc_idx = ++next_desc_idx_;
  sample_entries_[desc_idx] = entry;
  return desc_idx;
}

bool Mp4Writer::add_sample(const std::vector<uint8_t>& bytes, uint32_t duration, bool is_sync,
                           int32_t composition_offset, uint32_t desc_idx) {
  if (!media_ || sample_entries_.find(desc_idx) == sample_entries_.end()) return false;

  if (current_desc_idx_ != 0 && desc_idx != current_desc_idx_) {
    if (!flush_chunk()) return false;
  }
  current_desc_idx_ = desc_idx;

  pending_sizes_.push_back(static_cast<uint32_t>(bytes.size()));
  pending_data_.insert(pending_data_.end(), bytes.begin(), bytes.end());
  pending_durations_.push_back(duration);
  if (is_sync) {
    pending_sync_indices_.push_back(static_cast<uint32_t>(pending_sizes_.size()));
  }
  if (signed_ctts_enabled_) pending_ctts_offsets_.push_back(composition_offset);
  if (samples_per_chunk_ > 0 && pending_sizes_.size() >= samples_per_chunk_) {
    return flush_chunk();
  }
  return true;
}

bool Mp4Writer::enable_signed_composition_offsets() {
  if (!media_) return false;
  if (!pending_sizes_.empty()) {
    MUX_ERROR("enable_signed_composition_offsets() called after samples were buffered");
    return false;
  }
  MP4Err err = MP4UseSignedCompositionTimeOffsets(media_);
  if (err != MP4NoErr) {
    MUX_ERROR("MP4UseSignedCompositionTimeOffsets failed (err={})", err);
    return false;
  }
  signed_ctts_enabled_ = true;
  return true;
}

bool Mp4Writer::flush_chunk() {
  if (pending_sizes_.empty()) return true;
  if (!media_) return false;

  uint32_t n = static_cast<uint32_t>(pending_sizes_.size());

  MP4Handle data = make_handle(pending_data_.data(), pending_data_.size());
  MP4Handle sizes = make_handle(pending_sizes_.data(), n * sizeof(uint32_t));
  MP4Handle durations = make_handle(pending_durations_.data(), n * sizeof(uint32_t));
  MP4Handle sync = pending_sync_indices_.empty()
                     ? nullptr
                     : make_handle(pending_sync_indices_.data(),
                                   pending_sync_indices_.size() * sizeof(uint32_t));
  // Stored as u32 in the handle; libisomedia interprets as int32 when signed ctts is enabled.
  MP4Handle ctts = nullptr;
  if (signed_ctts_enabled_) {
    ctts = make_handle(pending_ctts_offsets_.data(), n * sizeof(int32_t));
  }

  bool already_attached = entries_attached_.count(current_desc_idx_) > 0;
  MP4Handle entry_for_call = already_attached ? nullptr : sample_entries_.at(current_desc_idx_);

  MP4Err err = MP4AddMediaSamples(media_, data, n, durations, sizes, entry_for_call,
                                  /*decodingOffsetsH=*/ctts, sync);

  MP4DisposeHandle(data);
  MP4DisposeHandle(sizes);
  MP4DisposeHandle(durations);
  if (sync) MP4DisposeHandle(sync);
  if (ctts) MP4DisposeHandle(ctts);

  if (err != MP4NoErr) {
    MUX_ERROR("MP4AddMediaSamples failed (err={}, samples={})", err, n);
    return false;
  }

  entries_attached_.insert(current_desc_idx_);
  pending_data_.clear();
  pending_sizes_.clear();
  pending_durations_.clear();
  pending_sync_indices_.clear();
  pending_ctts_offsets_.clear();
  MUX_DEBUG("Flushed chunk: {} samples", n);
  return true;
}

bool Mp4Writer::finalize(const std::string& output_path) {
  if (!movie_) return false;
  if (!flush_chunk()) return false;

  MP4Err err = MP4WriteMovieToFile(movie_, output_path.c_str());
  if (err != MP4NoErr) {
    MUX_ERROR("MP4WriteMovieToFile failed (err={})", err);
    return false;
  }
  MUX_DEBUG("MP4WriteMovieToFile: {}", output_path);
  return true;
}

}  // namespace av2_obu
