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

#include "av2_demuxer.h"

#include <cstring>
#include <fstream>
#include <map>
#include <vector>

#include <spdlog/spdlog.h>

#include <ISOMovies.h>
#include <MP4Movies.h>

namespace av2_obu {

namespace {

constexpr uint32_t kSampleEntryTypeAv02 = MP4_FOUR_CHAR_CODE('a', 'v', '0', '2');
constexpr uint32_t kAtomTypeAv2C = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'C');

// av2C box payload layout (per av2-isobmff working draft): a 9-byte prefix followed by configOBUs[]
constexpr size_t kAv2CPrefixSize = 9;

// Annex B-framed Temporal Delimiter
constexpr uint8_t kTdBytes[2] = {0x01, 0x08};

// If the sample bytes start with an Annex-B-framed TD OBU, return its length in bytes (otherwise 0)
size_t leading_td_length(const uint8_t* p, size_t n) {
  if (n < 2) return 0;
  // Decode LEB128 OBU size (max 8 bytes per AV2 Annex B).
  size_t i = 0;
  uint64_t obu_size = 0;
  uint32_t shift = 0;
  while (i < n && i < 8) {
    uint8_t b = p[i++];
    obu_size |= static_cast<uint64_t>(b & 0x7F) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  if (obu_size == 0 || i >= n || i + obu_size > n) return 0;
  // p[i] is the OBU header byte: ext_flag(1) | obu_type(5) | tlayer(2).
  uint32_t obu_type = (p[i] >> 2) & 0x1F;
  if (obu_type != 2) return 0;  // not a TEMPORAL_DELIMITER
  return i + static_cast<size_t>(obu_size);
}

// Find the first track whose sample entry type is 'av02'.
uint32_t find_av02_track(MP4Movie movie, uint32_t* out_track_count) {
  uint32_t count = 0;
  if (MP4GetMovieTrackCount(movie, &count) != MP4NoErr) return 0;
  if (out_track_count) *out_track_count = count;
  for (uint32_t i = 1; i <= count; ++i) {
    uint32_t entry_type = 0;
    if (MP4GetMovieIndTrackSampleEntryType(movie, i, &entry_type) != MP4NoErr) continue;
    if (entry_type == kSampleEntryTypeAv02) return i;
  }
  return 0;
}

// Read the av2C atom payload from the active sample entry of the media. 
// Parse VisualSampleEntry bytes skip sample entry header (78 byets) then walk the children and find av2C
constexpr size_t kVisualSampleEntryFixedSize = 78;

uint32_t be32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

bool read_av2c_payload(MP4Media media, uint32_t desc_index, std::vector<uint8_t>& out_bytes) {
  MP4Handle entry_h = nullptr;
  if (MP4NewHandle(0, &entry_h) != MP4NoErr || !entry_h) {
    spdlog::error("MP4NewHandle(sampleEntry) failed");
    return false;
  }
  if (MP4GetMediaSampleDescription(media, desc_index, entry_h, /*outIdx=*/nullptr) != MP4NoErr) {
    spdlog::error("MP4GetMediaSampleDescription({}) failed", desc_index);
    MP4DisposeHandle(entry_h);
    return false;
  }
  uint32_t entry_size = 0;
  MP4GetHandleSize(entry_h, &entry_size);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(*entry_h);

  // Outer box header: u32 size, u32 type.
  if (entry_size < 8 + kVisualSampleEntryFixedSize) {
    spdlog::error("Sample entry too short ({} bytes) to contain a VisualSampleEntry", entry_size);
    MP4DisposeHandle(entry_h);
    return false;
  }
  size_t cursor = 8 + kVisualSampleEntryFixedSize;

  // Walk child boxes: u32 size, u32 type, payload.
  while (cursor + 8 <= entry_size) {
    uint32_t child_size = be32(p + cursor);
    uint32_t child_type = be32(p + cursor + 4);
    if (child_size < 8 || cursor + child_size > entry_size) {
      spdlog::error("Malformed child box at offset {} (size={})", cursor, child_size);
      MP4DisposeHandle(entry_h);
      return false;
    }
    if (child_type == kAtomTypeAv2C) {
      out_bytes.assign(p + cursor + 8, p + cursor + child_size);
      MP4DisposeHandle(entry_h);
      return true;
    }
    cursor += child_size;
  }

  spdlog::error("av2C box not found among VisualSampleEntry children");
  MP4DisposeHandle(entry_h);
  return false;
}

}  // namespace

bool Av2Demuxer::demux(const std::string& input_mp4, const std::string& output_obu) {
  spdlog::debug("Demuxing {} -> {}", input_mp4, output_obu);

  MP4Movie movie = nullptr;
  if (MP4OpenMovieFile(&movie, input_mp4.c_str(), MP4OpenMovieNormal) != MP4NoErr || !movie) {
    spdlog::error("Failed to open {}", input_mp4);
    return false;
  }

  uint32_t track_count = 0;
  uint32_t track_idx = find_av02_track(movie, &track_count);
  if (track_idx == 0) {
    spdlog::error("No 'av02' video track found in {} ({} track(s) total)", input_mp4, track_count);
    MP4DisposeMovie(movie);
    return false;
  }
  spdlog::debug("Selected track {} of {} (sample entry type 'av02')", track_idx, track_count);

  MP4Track track = nullptr;
  if (MP4GetMovieIndTrack(movie, track_idx, &track) != MP4NoErr || !track) {
    spdlog::error("MP4GetMovieIndTrack({}) failed", track_idx);
    MP4DisposeMovie(movie);
    return false;
  }
  MP4Media media = nullptr;
  if (MP4GetTrackMedia(track, &media) != MP4NoErr || !media) {
    spdlog::error("MP4GetTrackMedia failed");
    MP4DisposeMovie(movie);
    return false;
  }

  // Discover every stsd sample entry (one per CVS) by probing indices until failure;
  // libisomedia has no MP4GetMediaSampleDescriptionCount API.
  std::map<uint32_t, std::vector<uint8_t>> config_obus_map;
  for (uint32_t desc_index = 1;; ++desc_index) {
    MP4Handle probe_h = nullptr;
    if (MP4NewHandle(0, &probe_h) != MP4NoErr || !probe_h) break;
    MP4Err probe_err = MP4GetMediaSampleDescription(media, desc_index, probe_h, nullptr);
    MP4DisposeHandle(probe_h);
    if (probe_err != MP4NoErr) break;

    std::vector<uint8_t> av2c_bytes;
    if (!read_av2c_payload(media, desc_index, av2c_bytes)) {
      MP4DisposeMovie(movie);
      return false;
    }
    if (av2c_bytes.size() < kAv2CPrefixSize) {
      spdlog::error("av2C payload too short ({} bytes, need >= {}) for desc_idx {}",
                    av2c_bytes.size(), kAv2CPrefixSize, desc_index);
      MP4DisposeMovie(movie);
      return false;
    }
    config_obus_map[desc_index] =
      std::vector<uint8_t>(av2c_bytes.begin() + kAv2CPrefixSize, av2c_bytes.end());
    spdlog::debug("av2C desc_idx {}: {} configOBU bytes", desc_index,
                  config_obus_map[desc_index].size());
  }
  if (config_obus_map.empty()) {
    spdlog::error("No av2C sample descriptions found");
    MP4DisposeMovie(movie);
    return false;
  }

  uint32_t sample_count = 0;
  if (MP4GetMediaSampleCount(media, &sample_count) != MP4NoErr) {
    spdlog::error("MP4GetMediaSampleCount failed");
    MP4DisposeMovie(movie);
    return false;
  }
  spdlog::debug("Track has {} samples", sample_count);

  std::ofstream out(output_obu, std::ios::binary);
  if (!out) {
    spdlog::error("Failed to open output {} for writing", output_obu);
    MP4DisposeMovie(movie);
    return false;
  }

  // For every sample: emit a TD first, then (only for sync samples) the configOBUs for that
  // sample's desc_idx, then the sample bytes. AV2's SH is prohibited from appearing inside
  // samples, so every sync sample must have configOBUs re-injected to stay independently
  // decodable/seekable.
  MP4Handle sample_h = nullptr;
  if (MP4NewHandle(0, &sample_h) != MP4NoErr || !sample_h) {
    spdlog::error("MP4NewHandle(sample) failed");
    MP4DisposeMovie(movie);
    return false;
  }

  uint64_t total_sample_bytes = 0;
  uint64_t total_config_bytes = 0;
  uint32_t samples_with_td = 0;
  for (uint32_t i = 1; i <= sample_count; ++i) {
    u32 size = 0;
    u64 dts = 0, dur = 0;
    s32 cts_off = 0;
    u32 flags = 0, desc_idx = 0;
    if (MP4GetIndMediaSample(media, i, sample_h, &size, &dts, &cts_off, &dur, &flags,
                             &desc_idx) != MP4NoErr) {
      spdlog::error("MP4GetIndMediaSample({}) failed", i);
      MP4DisposeHandle(sample_h);
      MP4DisposeMovie(movie);
      return false;
    }
    bool is_sync = !(flags & MP4MediaSampleNotSync);
    const uint8_t* config_obus = nullptr;
    size_t config_obus_size = 0;
    if (is_sync) {
      auto it = config_obus_map.find(desc_idx);
      if (it == config_obus_map.end()) {
        spdlog::error("Sample {} references desc_idx {} with no known av2C configOBUs", i,
                      desc_idx);
        MP4DisposeHandle(sample_h);
        MP4DisposeMovie(movie);
        return false;
      }
      config_obus = it->second.data();
      config_obus_size = it->second.size();
    }

    const uint8_t* sample_data = reinterpret_cast<const uint8_t*>(*sample_h);
    size_t td_prefix = leading_td_length(sample_data, size);
    if (td_prefix > 0) {
      // Sample already carries its own TD. Emit the TD that came from the bitstream.
      // For a sync sample splice configOBUs in between the TD and the frame data.
      out.write(reinterpret_cast<const char*>(sample_data),
                static_cast<std::streamsize>(td_prefix));
      if (is_sync) {
        out.write(reinterpret_cast<const char*>(config_obus),
                  static_cast<std::streamsize>(config_obus_size));
      }
      out.write(reinterpret_cast<const char*>(sample_data + td_prefix),
                static_cast<std::streamsize>(size - td_prefix));
      ++samples_with_td;
    } else {
      // Sample has no TD (the default --drop-td muxing). Synthesize one.
      out.write(reinterpret_cast<const char*>(kTdBytes), sizeof(kTdBytes));
      if (is_sync) {
        out.write(reinterpret_cast<const char*>(config_obus),
                  static_cast<std::streamsize>(config_obus_size));
      }
      out.write(reinterpret_cast<const char*>(sample_data),
                static_cast<std::streamsize>(size));
    }
    total_sample_bytes += size;
    total_config_bytes += config_obus_size;
  }

  MP4DisposeHandle(sample_h);
  MP4DisposeMovie(movie);
  out.close();

  spdlog::info("Wrote {} ({} samples [{} carried a TD, {} got a synthesized TD], {} sample "
               "entries, {} configOBU bytes total)",
               output_obu, sample_count, samples_with_td, sample_count - samples_with_td,
               config_obus_map.size(), total_config_bytes);
  return true;
}

}  // namespace av2_obu
