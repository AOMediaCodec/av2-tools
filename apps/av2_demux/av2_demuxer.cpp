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

constexpr uint32_t kSampleEntryTypeAv2E = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'E');
constexpr uint32_t kSampleEntryTypeAv2e = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'e');
constexpr uint32_t kAtomTypeAv2C = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'C');
constexpr uint32_t kAtomTypeAv2c = MP4_FOUR_CHAR_CODE('a', 'v', '2', 'c');

// OutOfBandAV2CodecConfigurationBox ('av2C') payload layout per av2-isobmff:
// 2-byte prefix (reserved1, config_obus_count_minus1) followed by config_obu[].
constexpr size_t kAv2CPrefixSize = 2;
// InBandAV2CodecConfigurationBox ('av2c') payload size per av2-isobmff.
constexpr size_t kAv2cPayloadSize = 2;

// Annex B-framed Temporal Delimiter OBU (leb128(1) = 0x01, header = 0x08).
constexpr uint8_t kTdBytes[2] = {0x01, 0x08};

struct AnnexBObuSlice {
  const uint8_t* data = nullptr;
  size_t total_len = 0;
  uint32_t obu_type = 0;
  uint8_t xlayer_id = 31;
  uint8_t is_global_prefix_metadata = 0;
};

// Parse an Annex B length-delimited OBU at [p, p + n).
bool parse_next_annexb_obu(const uint8_t* p, size_t n, AnnexBObuSlice& out) {
  if (n < 2) return false;
  size_t i = 0;
  uint64_t obu_size = 0;
  uint32_t shift = 0;
  while (i < n && i < 8) {
    uint8_t b = p[i++];
    obu_size |= static_cast<uint64_t>(b & 0x7F) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  if (obu_size == 0 || i >= n || i + obu_size > n) return false;
  const uint8_t* hdr = p + i;
  uint8_t ext_flag = (hdr[0] >> 7) & 0x01;
  uint32_t obu_type = (hdr[0] >> 2) & 0x1F;
  uint8_t xlayer_id = 0;
  size_t hdr_len = 1;
  if (ext_flag) {
    if (obu_size < 2) return false;
    xlayer_id = hdr[1] & 0x1F;
    hdr_len = 2;
  } else if (obu_type == 2 || obu_type == 20 || obu_type == 16 || obu_type == 18 ||
             obu_type == 17 || obu_type == 25) {
    xlayer_id = 31;
  }
  uint8_t is_global_prefix_meta = 0;
  if ((obu_type == 8 || obu_type == 9) && xlayer_id == 31 && obu_size > hdr_len) {
    uint8_t is_suffix = (hdr[hdr_len] >> 7) & 0x01;
    if (is_suffix == 0) is_global_prefix_meta = 1;
  }

  out.data = p;
  out.total_len = i + static_cast<size_t>(obu_size);
  out.obu_type = obu_type;
  out.xlayer_id = xlayer_id;
  out.is_global_prefix_metadata = is_global_prefix_meta;
  return true;
}

std::vector<AnnexBObuSlice> split_annexb_obus(const uint8_t* p, size_t n, size_t max_obus = 0) {
  std::vector<AnnexBObuSlice> slices;
  size_t off = 0;
  while (off < n && (max_obus == 0 || slices.size() < max_obus)) {
    AnnexBObuSlice s;
    if (!parse_next_annexb_obu(p + off, n - off, s)) break;
    slices.push_back(s);
    off += s.total_len;
  }
  return slices;
}

bool sample_has_rap_frame(const std::vector<AnnexBObuSlice>& obus) {
  for (const auto& s : obus) {
    // OBU_CLOSED_LOOP_KEY (4), OBU_OPEN_LOOP_KEY (5), OBU_RAS_FRAME (21)
    if (s.obu_type == 4 || s.obu_type == 5 || s.obu_type == 21) {
      return true;
    }
  }
  return false;
}

// Find the first track whose sample entry type is 'av2E' or 'av2e'.
uint32_t find_av2_track(MP4Movie movie, uint32_t* out_track_count, uint32_t* out_entry_type) {
  uint32_t count = 0;
  if (MP4GetMovieTrackCount(movie, &count) != MP4NoErr) return 0;
  if (out_track_count) *out_track_count = count;
  for (uint32_t i = 1; i <= count; ++i) {
    uint32_t entry_type = 0;
    if (MP4GetMovieIndTrackSampleEntryType(movie, i, &entry_type) != MP4NoErr) continue;
    if (entry_type == kSampleEntryTypeAv2E || entry_type == kSampleEntryTypeAv2e) {
      if (out_entry_type) *out_entry_type = entry_type;
      return i;
    }
  }
  return 0;
}

constexpr size_t kVisualSampleEntryFixedSize = 78;

uint32_t be32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

bool read_config_atom_payload(MP4Media media, uint32_t desc_index, uint32_t& out_box_type,
                              std::vector<uint8_t>& out_bytes) {
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

  if (entry_size < 8 + kVisualSampleEntryFixedSize) {
    spdlog::error("Sample entry too short ({} bytes) to contain a VisualSampleEntry", entry_size);
    MP4DisposeHandle(entry_h);
    return false;
  }
  size_t cursor = 8 + kVisualSampleEntryFixedSize;

  while (cursor + 8 <= entry_size) {
    uint32_t child_size = be32(p + cursor);
    uint32_t child_type = be32(p + cursor + 4);
    if (child_size < 8 || cursor + child_size > entry_size) {
      spdlog::error("Malformed child box at offset {} (size={})", cursor, child_size);
      MP4DisposeHandle(entry_h);
      return false;
    }
    if (child_type == kAtomTypeAv2C || child_type == kAtomTypeAv2c) {
      out_box_type = child_type;
      out_bytes.assign(p + cursor + 8, p + cursor + child_size);
      MP4DisposeHandle(entry_h);
      return true;
    }
    cursor += child_size;
  }

  spdlog::error("Neither 'av2C' nor 'av2c' box found among VisualSampleEntry children");
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
  uint32_t entry_type = 0;
  uint32_t track_idx = find_av2_track(movie, &track_count, &entry_type);
  if (track_idx == 0) {
    spdlog::error("No AV2 video track ('av2E' or 'av2e') found in {} ({} track(s) total)",
                  input_mp4, track_count);
    MP4DisposeMovie(movie);
    return false;
  }
  char entry_4cc[5] = {static_cast<char>((entry_type >> 24) & 0xFF),
                       static_cast<char>((entry_type >> 16) & 0xFF),
                       static_cast<char>((entry_type >> 8) & 0xFF),
                       static_cast<char>(entry_type & 0xFF), 0};
  spdlog::debug("Selected track {} of {} (sample entry type '{}')", track_idx, track_count,
                entry_4cc);

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

  struct SampleEntryConfig {
    uint32_t box_type = 0;
    std::vector<uint8_t> raw_bytes;
    std::vector<AnnexBObuSlice> slices;
    size_t config_obus_size = 0;
  };

  // Discover every stsd sample entry (one per CVS) by probing indices until failure;
  // libisomedia has no MP4GetMediaSampleDescriptionCount API.
  std::map<uint32_t, SampleEntryConfig> config_map;
  for (uint32_t desc_index = 1;; ++desc_index) {
    MP4Handle probe_h = nullptr;
    if (MP4NewHandle(0, &probe_h) != MP4NoErr || !probe_h) break;
    MP4Err probe_err = MP4GetMediaSampleDescription(media, desc_index, probe_h, nullptr);
    MP4DisposeHandle(probe_h);
    if (probe_err != MP4NoErr) break;

    SampleEntryConfig& cfg = config_map[desc_index];
    if (!read_config_atom_payload(media, desc_index, cfg.box_type, cfg.raw_bytes)) {
      MP4DisposeMovie(movie);
      return false;
    }
    if (cfg.box_type == kAtomTypeAv2C) {
      if (cfg.raw_bytes.size() < kAv2CPrefixSize) {
        spdlog::error("av2C payload too short ({} bytes, need >= {}) for desc_idx {}",
                      cfg.raw_bytes.size(), kAv2CPrefixSize, desc_index);
        MP4DisposeMovie(movie);
        return false;
      }
      const size_t expected_count = static_cast<size_t>(cfg.raw_bytes[1]) + 1;
      const uint8_t* config_obus_ptr = cfg.raw_bytes.data() + kAv2CPrefixSize;
      const size_t avail_size = cfg.raw_bytes.size() - kAv2CPrefixSize;
      cfg.slices = split_annexb_obus(config_obus_ptr, avail_size, expected_count);
      if (cfg.slices.size() < expected_count) {
        spdlog::error("av2C desc_idx {} config_obus_count_minus1={} expects {} OBUs, found {}",
                      desc_index, cfg.raw_bytes[1], expected_count, cfg.slices.size());
        MP4DisposeMovie(movie);
        return false;
      }
      for (const auto& s : cfg.slices) {
        cfg.config_obus_size += s.total_len;
      }
      spdlog::debug("av2C desc_idx {}: {} payload bytes (2-byte header + {} config_obu(s), {} bytes)",
                    desc_index, cfg.raw_bytes.size(), expected_count, cfg.config_obus_size);
    } else {
      if (cfg.raw_bytes.size() < kAv2cPayloadSize) {
        spdlog::error("av2c payload too short ({} bytes, need >= {}) for desc_idx {}",
                      cfg.raw_bytes.size(), kAv2cPayloadSize, desc_index);
        MP4DisposeMovie(movie);
        return false;
      }
      spdlog::debug("av2c desc_idx {}: {}-byte in-band configuration header", desc_index,
                    cfg.raw_bytes.size());
    }
  }
  if (config_map.empty()) {
    spdlog::error("No AV2 sample descriptions found");
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

  MP4Handle sample_h = nullptr;
  if (MP4NewHandle(0, &sample_h) != MP4NoErr || !sample_h) {
    spdlog::error("MP4NewHandle(sample) failed");
    MP4DisposeMovie(movie);
    return false;
  }

  uint64_t total_config_bytes = 0;
  uint32_t samples_with_td = 0;
  uint32_t prev_desc_idx = 0;
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
    auto cfg_it = config_map.find(desc_idx);
    if (cfg_it == config_map.end()) {
      spdlog::error("Sample {} references desc_idx {} with no known AV2 sample description", i,
                    desc_idx);
      MP4DisposeHandle(sample_h);
      MP4DisposeMovie(movie);
      return false;
    }
    const SampleEntryConfig& cfg = cfg_it->second;

    const uint8_t* sample_data = reinterpret_cast<const uint8_t*>(*sample_h);
    auto sample_slices = split_annexb_obus(sample_data, size);

    // Step 1: Write Temporal Delimiter OBU.
    size_t start_slice_idx = 0;
    if (!sample_slices.empty() && sample_slices[0].obu_type == 2) {
      out.write(reinterpret_cast<const char*>(sample_slices[0].data),
                static_cast<std::streamsize>(sample_slices[0].total_len));
      start_slice_idx = 1;
      ++samples_with_td;
    } else {
      out.write(reinterpret_cast<const char*>(kTdBytes), sizeof(kTdBytes));
    }

    // Inject out-of-band config OBUs at sample 1, when sample entry changes, or at any
    // random access point (sync sample or OLK/RAS frame) per AV2 §7.3.6.1.
    const bool is_sync = (flags & MP4MediaSampleNotSync) == 0;
    const bool inject_config =
        (cfg.box_type == kAtomTypeAv2C) &&
        (i == 1 || desc_idx != prev_desc_idx || is_sync || sample_has_rap_frame(sample_slices));
    prev_desc_idx = desc_idx;

    if (inject_config && !cfg.slices.empty()) {
      // Write global-scope config OBUs first, then any global prefix metadata OBUs from the
      // sample, then the grouped HLS block from config_obus, preserving AV2 §7.3.5 ordering.
      size_t cfg_idx = 0;
      while (cfg_idx < cfg.slices.size() && cfg.slices[cfg_idx].xlayer_id == 31) {
        out.write(reinterpret_cast<const char*>(cfg.slices[cfg_idx].data),
                  static_cast<std::streamsize>(cfg.slices[cfg_idx].total_len));
        ++cfg_idx;
      }
      while (start_slice_idx < sample_slices.size() &&
             sample_slices[start_slice_idx].is_global_prefix_metadata) {
        out.write(reinterpret_cast<const char*>(sample_slices[start_slice_idx].data),
                  static_cast<std::streamsize>(sample_slices[start_slice_idx].total_len));
        ++start_slice_idx;
      }
      while (cfg_idx < cfg.slices.size()) {
        out.write(reinterpret_cast<const char*>(cfg.slices[cfg_idx].data),
                  static_cast<std::streamsize>(cfg.slices[cfg_idx].total_len));
        ++cfg_idx;
      }
      total_config_bytes += cfg.config_obus_size;
    }

    // Write remaining sample OBUs, skipping any bit-exact duplicates of injected config OBUs.
    for (size_t s_idx = start_slice_idx; s_idx < sample_slices.size(); ++s_idx) {
      const auto& s = sample_slices[s_idx];
      bool is_dup_cfg = false;
      if (inject_config) {
        for (const auto& c : cfg.slices) {
          if (c.total_len == s.total_len && std::memcmp(c.data, s.data, s.total_len) == 0) {
            is_dup_cfg = true;
            break;
          }
        }
      }
      if (!is_dup_cfg) {
        out.write(reinterpret_cast<const char*>(s.data),
                  static_cast<std::streamsize>(s.total_len));
      }
    }
  }

  MP4DisposeHandle(sample_h);
  MP4DisposeMovie(movie);
  out.close();

  spdlog::info("Wrote {} ({} samples [{} carried a TD, {} got a synthesized TD], {} sample "
               "entries, {} configOBU bytes total)",
               output_obu, sample_count, samples_with_td, sample_count - samples_with_td,
               config_map.size(), total_config_bytes);
  return true;
}

}  // namespace av2_obu
