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

#include <cstring>

#include <spdlog/spdlog.h>

namespace av2_obu {

namespace {

constexpr uint32_t kBrandAv02 = MP4_FOUR_CHAR_CODE('a', 'v', '0', '2');
constexpr uint32_t kBrandIso6 = MP4_FOUR_CHAR_CODE('i', 's', 'o', '6');
constexpr uint32_t kAtomTypeAv2C = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'C');
constexpr uint32_t kSampleEntryTypeAv02 = MP4_FOUR_CHAR_CODE('a', 'v', '0', '2');

// Allocate an MP4Handle and copy bytes into it.
MP4Handle make_handle(const void* data, size_t size) {
  MP4Handle h = nullptr;
  if (MP4NewHandle(static_cast<u32>(size), &h) != MP4NoErr || !h) return nullptr;
  if (size > 0) std::memcpy(*h, data, size);
  return h;
}

}  // namespace

Mp4Writer::Mp4Writer() {
  // Profile/level fields are MPEG-4 systems-era and irrelevant for AV2 video;
  // 0xff means "no profile" per libisomedia conventions.
  MP4Err err = MP4NewMovie(&movie_, /*initialODID=*/1,
                           /*OD*/ 0xff, /*scene*/ 0xff,
                           /*audio*/ 0xff, /*visual*/ 0xff,
                           /*graphics*/ 0xff);
  if (err != MP4NoErr) {
    spdlog::error("MP4NewMovie failed (err={})", err);
    movie_ = nullptr;
    return;
  }

  ISOSetMovieBrand(movie_, kBrandAv02, /*minor=*/0);
  ISOSetMovieCompatibleBrand(movie_, kBrandAv02);
  ISOSetMovieCompatibleBrand(movie_, kBrandIso6);
}

Mp4Writer::~Mp4Writer() {
  if (sample_entry_) MP4DisposeHandle(sample_entry_);
  if (movie_) MP4DisposeMovie(movie_);
}

bool Mp4Writer::add_video_track(uint32_t timescale, uint16_t width, uint16_t height,
                                const AV2CodecConfigurationBox& av2c) {
  if (!movie_) return false;

  MP4Err err = MP4NewMovieTrack(movie_, MP4NewTrackIsVisual, &track_);
  if (err != MP4NoErr || !track_) {
    spdlog::error("MP4NewMovieTrack failed (err={})", err);
    return false;
  }

  err = MP4NewTrackMedia(track_, &media_, MP4VisualHandlerType, timescale, /*dataRef=*/nullptr);
  if (err != MP4NoErr || !media_) {
    spdlog::error("MP4NewTrackMedia failed (err={})", err);
    return false;
  }

  // Build the av2C extension atom from the box bytes.
  std::vector<uint8_t> av2c_bytes = av2c.serialize();
  if (av2c_bytes.empty()) {
    spdlog::error("av2C serialization produced empty payload");
    return false;
  }

  MP4Handle av2c_payload = make_handle(av2c_bytes.data(), av2c_bytes.size());
  if (!av2c_payload) {
    spdlog::error("MP4NewHandle(av2C payload) failed");
    return false;
  }

  MP4GenericAtom av2c_atom = nullptr;
  err = MP4NewForeignAtom(&av2c_atom, kAtomTypeAv2C, av2c_payload);
  // Ownership of the payload bytes transfers to the atom on success.
  // (libisomedia takes a copy; we still dispose ours to be safe — but only
  // if the call succeeded with a different owner, otherwise we leak.)
  if (err != MP4NoErr || !av2c_atom) {
    spdlog::error("MP4NewForeignAtom('av2C') failed (err={})", err);
    MP4DisposeHandle(av2c_payload);
    return false;
  }

  // Create the sample entry and attach the av2C atom.
  err = MP4NewHandle(0, &sample_entry_);
  if (err != MP4NoErr || !sample_entry_) {
    spdlog::error("MP4NewHandle(sample_entry) failed (err={})", err);
    return false;
  }

  err = ISONewGeneralSampleDescription(track_, sample_entry_, /*dataRefIdx=*/1,
                                       kSampleEntryTypeAv02, av2c_atom);
  if (err != MP4NoErr) {
    spdlog::error("ISONewGeneralSampleDescription failed (err={})", err);
    return false;
  }

  err = ISOSetSampleDescriptionDimensions(sample_entry_, width, height);
  if (err != MP4NoErr) {
    spdlog::error("ISOSetSampleDescriptionDimensions failed (err={})", err);
    return false;
  }

  return true;
}

bool Mp4Writer::add_sample(const std::vector<uint8_t>& bytes, uint32_t duration, bool is_sync) {
  if (!media_ || !sample_entry_) return false;
  pending_sizes_.push_back(static_cast<uint32_t>(bytes.size()));
  pending_data_.insert(pending_data_.end(), bytes.begin(), bytes.end());
  pending_durations_.push_back(duration);
  if (is_sync) {
    // 1-based index relative to this chunk.
    pending_sync_indices_.push_back(static_cast<uint32_t>(pending_sizes_.size()));
  }
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

  // First chunk passes sample_entry_; later chunks pass NULL to re-use it.
  MP4Handle entry_for_call = first_chunk_ ? sample_entry_ : nullptr;

  MP4Err err = MP4AddMediaSamples(media_, data, n, durations, sizes, entry_for_call,
                                  /*decodingOffsetsH=*/nullptr, sync);

  MP4DisposeHandle(data);
  MP4DisposeHandle(sizes);
  MP4DisposeHandle(durations);
  if (sync) MP4DisposeHandle(sync);

  if (err != MP4NoErr) {
    spdlog::error("MP4AddMediaSamples failed (err={}, samples={})", err, n);
    return false;
  }

  first_chunk_ = false;
  pending_data_.clear();
  pending_sizes_.clear();
  pending_durations_.clear();
  pending_sync_indices_.clear();
  spdlog::debug("Flushed chunk: {} samples", n);
  return true;
}

bool Mp4Writer::finalize(const std::string& output_path) {
  if (!movie_) return false;
  if (!flush_chunk()) return false;

  MP4Err err = MP4WriteMovieToFile(movie_, output_path.c_str());
  if (err != MP4NoErr) {
    spdlog::error("MP4WriteMovieToFile failed (err={})", err);
    return false;
  }
  spdlog::info("Wrote {}", output_path);
  return true;
}

}  // namespace av2_obu
