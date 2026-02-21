/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause
 * Clear License was not distributed with this source code in the LICENSE file,
 * you can obtain it at aomedia.org/license/software-license/bsd-3-c-c/. If
 * the Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

// AV1 OBU Tool - Slim AV1 bitstream parser
//
// Parses the length-delimited (Annex B / packed) AV1 bitstream format:
//   bitstream -> temporal_unit* -> frame_unit* -> OBU*
//
// Implements per the AV1 specification:
//   - General OBU syntax  (Section 5.3.1)
//   - OBU header syntax   (Section 5.3.2 / 5.3.3)
//   - Metadata OBU syntax (Section 5.8.1)
//     * metadata_itut_t35  (Section 5.8.2)
//     * metadata_hdr_cll   (Section 5.8.3)
//     * metadata_hdr_mdcv  (Section 5.8.4)

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef AV1_TOOL_WITH_MP4
#include <ISOMovies.h>
#include <MP4Movies.h>
#endif

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// AV1 OBU type constants  (Table 5 in the AV1 spec)
// ---------------------------------------------------------------------------
enum class ObuType : uint8_t {
  kReserved0 = 0,
  kSequenceHeader = 1,
  kTemporalDelimiter = 2,
  kFrameHeader = 3,
  kTileGroup = 4,
  kMetadata = 5,
  kFrame = 6,
  kRedundantFrameHeader = 7,
  kTileList = 8,
  // 9-14 reserved
  kPadding = 15,
};

static const char* obu_type_name(ObuType t) {
  switch (t) {
    case ObuType::kReserved0:
      return "RESERVED_0";
    case ObuType::kSequenceHeader:
      return "SEQUENCE_HEADER";
    case ObuType::kTemporalDelimiter:
      return "TEMPORAL_DELIMITER";
    case ObuType::kFrameHeader:
      return "FRAME_HEADER";
    case ObuType::kTileGroup:
      return "TILE_GROUP";
    case ObuType::kMetadata:
      return "METADATA";
    case ObuType::kFrame:
      return "FRAME";
    case ObuType::kRedundantFrameHeader:
      return "REDUNDANT_FRAME_HEADER";
    case ObuType::kTileList:
      return "TILE_LIST";
    case ObuType::kPadding:
      return "PADDING";
    default:
      return "RESERVED";
  }
}

// Parse an OBU type name (case-insensitive) or numeric value.
// Returns false if unrecognised.
static bool parse_obu_type_filter(const std::string& s, ObuType& out) {
  // Try numeric first
  try {
    size_t pos = 0;
    int n = std::stoi(s, &pos);
    if (pos == s.size() && n >= 0 && n <= 15) {
      out = static_cast<ObuType>(n);
      return true;
    }
  } catch (...) {}

  std::string upper;
  for (char c : s)
    upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

  // Accept both bare names and prefixed names (e.g. "METADATA" or "OBU_METADATA")
  auto strip = [](const std::string& u) {
    if (u.rfind("OBU_", 0) == 0)
      return u.substr(4);
    return u;
  };
  upper = strip(upper);

  if (upper == "RESERVED_0") {
    out = ObuType::kReserved0;
    return true;
  }
  if (upper == "SEQUENCE_HEADER") {
    out = ObuType::kSequenceHeader;
    return true;
  }
  if (upper == "TEMPORAL_DELIMITER") {
    out = ObuType::kTemporalDelimiter;
    return true;
  }
  if (upper == "FRAME_HEADER") {
    out = ObuType::kFrameHeader;
    return true;
  }
  if (upper == "TILE_GROUP") {
    out = ObuType::kTileGroup;
    return true;
  }
  if (upper == "METADATA") {
    out = ObuType::kMetadata;
    return true;
  }
  if (upper == "FRAME") {
    out = ObuType::kFrame;
    return true;
  }
  if (upper == "REDUNDANT_FRAME_HEADER") {
    out = ObuType::kRedundantFrameHeader;
    return true;
  }
  if (upper == "TILE_LIST") {
    out = ObuType::kTileList;
    return true;
  }
  if (upper == "PADDING") {
    out = ObuType::kPadding;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// AV1 Metadata type constants  (Table in Section 6.7.1)
// ---------------------------------------------------------------------------
enum class MetadataType : uint32_t {
  kReserved = 0,
  kHdrCll = 1,
  kHdrMdcv = 2,
  kScalability = 3,
  kItutT35 = 4,
  kTimecode = 5,
};

static const char* metadata_type_name(uint32_t t) {
  switch (t) {
    case 0:
      return "RESERVED";
    case 1:
      return "HDR_CLL";
    case 2:
      return "HDR_MDCV";
    case 3:
      return "SCALABILITY";
    case 4:
      return "ITUT_T35";
    case 5:
      return "TIMECODE";
    default:
      if (t >= 6 && t <= 31)
        return "USER_PRIVATE";
      return "RESERVED_AOM";
  }
}

// Parse a metadata type name (case-insensitive) or numeric value.
// Returns false if unrecognised.
static bool parse_metadata_type_filter(const std::string& s, uint32_t& out) {
  // Try numeric first
  try {
    size_t pos = 0;
    int n = std::stoi(s, &pos);
    if (pos == s.size() && n >= 0) {
      out = static_cast<uint32_t>(n);
      return true;
    }
  } catch (...) {}

  std::string upper;
  for (char c : s)
    upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

  if (upper == "HDR_CLL") {
    out = static_cast<uint32_t>(MetadataType::kHdrCll);
    return true;
  }
  if (upper == "HDR_MDCV") {
    out = static_cast<uint32_t>(MetadataType::kHdrMdcv);
    return true;
  }
  if (upper == "SCALABILITY") {
    out = static_cast<uint32_t>(MetadataType::kScalability);
    return true;
  }
  if (upper == "ITUT_T35" || upper == "T35") {
    out = static_cast<uint32_t>(MetadataType::kItutT35);
    return true;
  }
  if (upper == "TIMECODE") {
    out = static_cast<uint32_t>(MetadataType::kTimecode);
    return true;
  }
  return false;
}
class BitReader {
public:
  BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  // Read n bits (MSB first), return as uint32_t
  uint32_t f(int n) {
    uint32_t val = 0;
    for (int i = 0; i < n; ++i) {
      if (byte_offset_ >= size_) {
        error_ = true;
        return 0;
      }
      val = (val << 1) | ((data_[byte_offset_] >> (7 - bit_offset_)) & 1);
      advance_one_bit();
    }
    return val;
  }

  // Read leb128() - variable-length unsigned integer (byte-aligned)
  // Returns the decoded value; updates leb128_bytes_ with number of bytes read
  uint64_t leb128(int& bytes_read) {
    // leb128 must be byte-aligned
    bytes_read = 0;
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
      if (byte_offset_ >= size_) {
        error_ = true;
        return 0;
      }
      uint8_t b = data_[byte_offset_++];
      // bit_offset_ stays 0 since we consume whole bytes
      ++bytes_read;
      value |= static_cast<uint64_t>(b & 0x7F) << (i * 7);
      if (!(b & 0x80))
        break;
    }
    return value;
  }

  bool error() const { return error_; }
  size_t byte_offset() const { return byte_offset_; }
  size_t bytes_remaining() const { return size_ > byte_offset_ ? size_ - byte_offset_ : 0; }

private:
  void advance_one_bit() {
    ++bit_offset_;
    if (bit_offset_ == 8) {
      bit_offset_ = 0;
      ++byte_offset_;
    }
  }

  const uint8_t* data_;
  size_t size_;
  size_t byte_offset_ = 0;
  int bit_offset_ = 0;
  bool error_ = false;
};

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct ObuHeader {
  uint8_t forbidden_bit;
  ObuType obu_type;
  bool extension_flag;
  bool has_size_field;
  // extension header fields (valid only when extension_flag == true)
  uint8_t temporal_id;
  uint8_t spatial_id;
  size_t header_size;  // 1 or 2 bytes
};

struct MetadataItutT35 {
  uint8_t country_code;
  uint8_t country_code_extension;  // valid only when country_code == 0xFF
  std::vector<uint8_t> payload_bytes;
};

struct MetadataHdrCll {
  uint16_t max_cll;
  uint16_t max_fall;
};

struct MetadataHdrMdcv {
  uint16_t primary_chromaticity_x[3];
  uint16_t primary_chromaticity_y[3];
  uint16_t white_point_chromaticity_x;
  uint16_t white_point_chromaticity_y;
  uint32_t luminance_max;
  uint32_t luminance_min;
};

struct MetadataObu {
  uint32_t metadata_type;
  bool parsed;
  // Only one of the following is valid depending on metadata_type
  MetadataItutT35 itut_t35;
  MetadataHdrCll hdr_cll;
  MetadataHdrMdcv hdr_mdcv;
};

struct ObuInfo {
  // Position in file
  uint64_t file_offset;  // byte offset of the OBU length field (in frame_unit)
  uint64_t obu_length;   // the obu_length LEB128 value (bytes for the full OBU)

  ObuHeader header;
  uint64_t payload_size;  // bytes of payload after header (and size field if present)

  // Temporal unit / frame unit context
  int temporal_unit_index;
  int frame_unit_index;

  // Metadata, valid only when header.obu_type == kMetadata
  bool has_metadata;
  MetadataObu metadata;

  // Raw payload bytes (after the OBU header and obu_size field), used for analysis
  std::vector<uint8_t> raw_payload;
};

// ---------------------------------------------------------------------------
// FNV-1a 64-bit hash (no extra dependencies)
// ---------------------------------------------------------------------------
static uint64_t fnv1a(const std::vector<uint8_t>& data) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (uint8_t b : data) {
    h ^= b;
    h *= 0x100000001b3ULL;
  }
  return h;
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

// Parse obu_header() from a BitReader
// Returns false on error
static bool parse_obu_header(BitReader& br, ObuHeader& hdr) {
  hdr.forbidden_bit = static_cast<uint8_t>(br.f(1));
  hdr.obu_type = static_cast<ObuType>(br.f(4));
  hdr.extension_flag = br.f(1) != 0;
  hdr.has_size_field = br.f(1) != 0;
  /* obu_reserved_1bit = */ br.f(1);
  hdr.header_size = 1;

  hdr.temporal_id = 0;
  hdr.spatial_id = 0;

  if (hdr.extension_flag) {
    // obu_extension_header()
    hdr.temporal_id = static_cast<uint8_t>(br.f(3));
    hdr.spatial_id = static_cast<uint8_t>(br.f(2));
    /* extension_header_reserved_3bits = */ br.f(3);
    hdr.header_size = 2;
  }

  return !br.error();
}

// Parse metadata_itut_t35() from a BitReader (payload bytes already sliced)
static void parse_metadata_itut_t35(BitReader& br, size_t payload_size, MetadataItutT35& meta) {
  meta.country_code = static_cast<uint8_t>(br.f(8));
  size_t header_bytes = 1;
  if (meta.country_code == 0xFF) {
    meta.country_code_extension = static_cast<uint8_t>(br.f(8));
    header_bytes = 2;
  } else {
    meta.country_code_extension = 0;
  }
  // Read remaining payload bytes
  size_t remaining = (payload_size > header_bytes) ? payload_size - header_bytes : 0;
  meta.payload_bytes.resize(remaining);
  for (size_t i = 0; i < remaining; ++i) {
    meta.payload_bytes[i] = static_cast<uint8_t>(br.f(8));
  }
}

// Parse metadata_hdr_cll()
static void parse_metadata_hdr_cll(BitReader& br, MetadataHdrCll& meta) {
  meta.max_cll = static_cast<uint16_t>(br.f(16));
  meta.max_fall = static_cast<uint16_t>(br.f(16));
}

// Parse metadata_hdr_mdcv()
static void parse_metadata_hdr_mdcv(BitReader& br, MetadataHdrMdcv& meta) {
  for (int i = 0; i < 3; ++i) {
    meta.primary_chromaticity_x[i] = static_cast<uint16_t>(br.f(16));
    meta.primary_chromaticity_y[i] = static_cast<uint16_t>(br.f(16));
  }
  meta.white_point_chromaticity_x = static_cast<uint16_t>(br.f(16));
  meta.white_point_chromaticity_y = static_cast<uint16_t>(br.f(16));
  meta.luminance_max = br.f(32);
  meta.luminance_min = br.f(32);
}

// ---------------------------------------------------------------------------
// Main parser
// ---------------------------------------------------------------------------

class Av1Parser {
public:
  bool parse_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      spdlog::error("Cannot open file: {}", path);
      return false;
    }

    // Read entire file into memory for simplicity
    ifs.seekg(0, std::ios::end);
    file_size_ = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    file_data_.resize(file_size_);
    ifs.read(reinterpret_cast<char*>(file_data_.data()), static_cast<std::streamsize>(file_size_));
    if (!ifs) {
      spdlog::error("Failed to read file: {}", path);
      return false;
    }

    file_path_ = path;
    parse_bitstream();
    return true;
  }

  // Parse a single MP4 AV1 sample (flat OBU sequence, has_size_field=1).
  // sample_idx is encoded into file_offset as (sample_idx << 32 | intra_offset).
  void parse_sample(const uint8_t* data, size_t size, int sample_idx) {
    size_t off = 0;
    while (off < size) {
      size_t remaining = size - off;
      const uint8_t* p = data + off;

      if (remaining < 1)
        break;

      // Parse OBU header byte(s) to find total OBU size so we can advance correctly.
      uint8_t b0 = p[0];
      bool extension_flag = ((b0 >> 2) & 0x1) != 0;
      bool has_size_field = ((b0 >> 1) & 0x1) != 0;
      size_t hdr_bytes = extension_flag ? 2 : 1;

      if (!has_size_field) {
        // ISOBMFF AV1 samples must have has_size_field=1; skip malformed OBU
        spdlog::warn("OBU at sample {} +{} has has_size_field=0, skipping rest of sample",
                     sample_idx, off);
        break;
      }

      if (hdr_bytes >= remaining) {
        spdlog::warn("Truncated OBU header at sample {} +{}", sample_idx, off);
        break;
      }

      // Read obu_size LEB128
      size_t leb_pos = hdr_bytes;
      uint64_t payload_size = 0;
      size_t leb_bytes = 0;
      for (int i = 0; i < 8; ++i) {
        if (leb_pos + i >= remaining) {
          leb_bytes = 0;
          break;
        }
        uint8_t lb = p[leb_pos + i];
        payload_size |= static_cast<uint64_t>(lb & 0x7F) << (i * 7);
        ++leb_bytes;
        if (!(lb & 0x80))
          break;
      }
      if (leb_bytes == 0) {
        spdlog::warn("Truncated obu_size LEB128 at sample {} +{}", sample_idx, off);
        break;
      }

      size_t total_obu_bytes = hdr_bytes + leb_bytes + static_cast<size_t>(payload_size);
      if (off + total_obu_bytes > size) {
        spdlog::warn("OBU extends beyond sample at sample {} +{}", sample_idx, off);
        break;
      }

      // Encode position: upper 32 bits = sample index, lower 32 bits = intra-sample offset
      uint64_t encoded_offset = (static_cast<uint64_t>(sample_idx) << 32) |
                                static_cast<uint64_t>(off & 0xFFFFFFFFu);

      parse_obu_from_buffer(p, total_obu_bytes, encoded_offset, sample_idx, 0);

      off += total_obu_bytes;
    }
  }

#ifdef AV1_TOOL_WITH_MP4
  bool parse_mp4(const std::string& path) {
    MP4Err err = MP4NoErr;
    MP4Movie moov = nullptr;

    err = MP4OpenMovieFile(&moov, path.c_str(), MP4OpenMovieNormal);
    if (err != MP4NoErr) {
      spdlog::error("Failed to open MP4 file: {} (err={})", path, static_cast<int>(err));
      return false;
    }

    u32 track_count = 0;
    if (MP4GetMovieTrackCount(moov, &track_count) != MP4NoErr) {
      MP4DisposeMovie(moov);
      spdlog::error("Failed to get track count from: {}", path);
      return false;
    }

    bool found_av1 = false;
    file_path_ = path;
    from_mp4_ = true;

    for (u32 track_number = 1; track_number <= track_count; ++track_number) {
      MP4Track trak = nullptr;
      if (MP4GetMovieIndTrack(moov, track_number, &trak) != MP4NoErr || !trak)
        continue;

      MP4TrackReader reader = nullptr;
      if (MP4CreateTrackReader(trak, &reader) != MP4NoErr || !reader)
        continue;

      MP4Handle sample_entry_h = nullptr;
      MP4NewHandle(0, &sample_entry_h);
      if (!sample_entry_h) {
        MP4DisposeTrackReader(reader);
        continue;
      }

      err = MP4TrackReaderGetCurrentSampleDescription(reader, sample_entry_h);
      if (err != MP4NoErr) {
        MP4DisposeHandle(sample_entry_h);
        MP4DisposeTrackReader(reader);
        continue;
      }

      u32 sample_entry_type = 0;
      ISOGetSampleDescriptionType(sample_entry_h, &sample_entry_type);

      // Unwrap resv / encv to get the original format
      if (sample_entry_type == MP4_FOUR_CHAR_CODE('r', 'e', 's', 'v') ||
          sample_entry_type == MP4_FOUR_CHAR_CODE('e', 'n', 'c', 'v')) {
        ISOGetOriginalFormat(sample_entry_h, &sample_entry_type);
      }

      MP4DisposeHandle(sample_entry_h);

      if (sample_entry_type != MP4_FOUR_CHAR_CODE('a', 'v', '0', '1')) {
        MP4DisposeTrackReader(reader);
        continue;
      }

      // Found an AV1 track — read all access units
      found_av1 = true;
      spdlog::debug("av1_obu_tool: AV1 track found at track index {}", track_number);

      MP4Handle au_h = nullptr;
      MP4NewHandle(0, &au_h);
      if (!au_h) {
        MP4DisposeTrackReader(reader);
        continue;
      }

      u32 au_size = 0;
      u32 flags = 0;
      s32 cts = 0, dts = 0;
      int au_index = 0;

      while ((err = MP4TrackReaderGetNextAccessUnit(reader, au_h, &au_size, &flags, &cts, &dts)) ==
             MP4NoErr) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(*au_h);
        parse_sample(bytes, static_cast<size_t>(au_size), au_index);
        ++au_index;
        MP4SetHandleSize(au_h, 0);
      }

      if (err != MP4EOF && err != MP4NoErr && err != MP4BadParamErr) {
        spdlog::warn("Track reader error on track {} (err={})", track_number, static_cast<int>(err));
      }

      MP4DisposeHandle(au_h);
      MP4DisposeTrackReader(reader);
    }

    MP4DisposeMovie(moov);

    if (!found_av1) {
      spdlog::error("No AV1 track (av01) found in: {}", path);
      return false;
    }
    return true;
  }
#endif  // AV1_TOOL_WITH_MP4

  const std::vector<ObuInfo>& obus() const { return obus_; }
  const std::string& file_path() const { return file_path_; }
  size_t file_size() const { return file_size_; }

  // Returns true if this OBU passes both the OBU type filter and metadata type filter.
  static bool passes_filter(const ObuInfo& o, const std::set<ObuType>& obu_filter,
                            const std::set<uint32_t>& meta_filter) {
    // A non-empty meta_filter implies we only want METADATA OBUs.
    if (!meta_filter.empty()) {
      if (o.header.obu_type != ObuType::kMetadata)
        return false;
      if (!o.has_metadata || !meta_filter.count(o.metadata.metadata_type))
        return false;
      return true;
    }
    if (!obu_filter.empty() && !obu_filter.count(o.header.obu_type))
      return false;
    return true;
  }

  // ---- Human-readable dump ----
  void dump(const std::set<ObuType>& obu_filter = {},
            const std::set<uint32_t>& meta_filter = {}) const {
    std::cout << "=== AV1 OBU Parser ===" << std::endl;
    std::cout << "File: " << file_path_ << std::endl;

    // Count matches up front so the header is accurate
    size_t count = 0;
    for (const auto& o : obus_) {
      if (passes_filter(o, obu_filter, meta_filter))
        ++count;
    }
    bool any_filter = !obu_filter.empty() || !meta_filter.empty();
    std::cout << "OBU count: " << count;
    if (any_filter)
      std::cout << " (filtered from " << obus_.size() << " total)";
    std::cout << std::endl << std::endl;

    for (size_t i = 0; i < obus_.size(); ++i) {
      const ObuInfo& o = obus_[i];
      if (!passes_filter(o, obu_filter, meta_filter))
        continue;

      const ObuHeader& h = o.header;
      std::cout << "--- OBU #" << i << " ---" << std::endl;
      std::cout << "Type: " << obu_type_name(h.obu_type) << std::endl;
      std::cout << "Temporal unit: " << o.temporal_unit_index << std::endl;
      std::cout << "Frame unit: " << o.frame_unit_index << std::endl;
      if (from_mp4_) {
        uint32_t sample_idx = static_cast<uint32_t>(o.file_offset >> 32);
        uint32_t sample_off = static_cast<uint32_t>(o.file_offset & 0xFFFFFFFFu);
        std::cout << "Position: sample " << sample_idx << " @ +" << sample_off << std::endl;
      } else {
        std::cout << "Position: " << o.file_offset << std::endl;
      }
      std::cout << "Header length: " << h.header_size << std::endl;
      std::cout << "Payload size: " << o.payload_size << std::endl;
      if (h.extension_flag) {
        std::cout << "Temporal ID: " << static_cast<int>(h.temporal_id) << std::endl;
        std::cout << "Spatial ID: " << static_cast<int>(h.spatial_id) << std::endl;
      }
      if (h.forbidden_bit)
        std::cout << "WARNING: forbidden_bit=1" << std::endl;

      if (o.has_metadata) {
        dump_metadata(o.metadata, "  ");
      }

      std::cout << std::endl;
    }
  }

  // ---- JSON output ----
  json to_json(const std::set<ObuType>& obu_filter = {},
               const std::set<uint32_t>& meta_filter = {}) const {
    json j;
    j["file"] = file_path_;

    json obu_array = json::array();
    for (size_t i = 0; i < obus_.size(); ++i) {
      const ObuInfo& o = obus_[i];
      if (!passes_filter(o, obu_filter, meta_filter))
        continue;

      const ObuHeader& h = o.header;

      json jo;
      jo["type_name"] = obu_type_name(h.obu_type);

      // position — mirrors AV2's OBUPosition::to_json()
      json jp;
      if (from_mp4_) {
        jp["sample_index"] = static_cast<uint32_t>(o.file_offset >> 32);
        jp["sample_offset"] = static_cast<uint32_t>(o.file_offset & 0xFFFFFFFFu);
      } else {
        jp["file_offset"] = o.file_offset;
      }
      jp["obu_size"] = h.header_size + o.payload_size;
      jp["header_size"] = h.header_size;
      jp["payload_size"] = o.payload_size;
      // AV1-specific framing context
      jp["temporal_unit"] = o.temporal_unit_index;
      jp["frame_unit"] = o.frame_unit_index;
      jo["position"] = jp;

      // header — mirrors AV2's OBUHeader::to_json(), with AV1-specific fields
      json jh;
      jh["obu_type"] = static_cast<int>(h.obu_type);
      jh["temporal_id"] = h.extension_flag ? static_cast<int>(h.temporal_id) : 0;
      jh["spatial_id"] = h.extension_flag ? static_cast<int>(h.spatial_id) : 0;
      jh["extension_flag"] = static_cast<int>(h.extension_flag);
      jh["has_size_field"] = static_cast<int>(h.has_size_field);
      jh["forbidden_bit"] = static_cast<int>(h.forbidden_bit);
      jo["header"] = jh;

      if (o.has_metadata) {
        jo["metadata"] = metadata_to_json(o.metadata);
      }

      obu_array.push_back(jo);
    }
    j["obu_count"] = obu_array.size();
    if (!obu_filter.empty() || !meta_filter.empty())
      j["total_obu_count"] = obus_.size();
    j["obus"] = obu_array;
    return j;
  }

  // ---- Analysis ----

  struct DedupGroup {
    uint64_t hash;
    size_t header_size;   // OBU header bytes (1 or 2)
    size_t payload_size;  // payload bytes (after header)
    size_t obu_size;      // header_size + payload_size
    size_t count;
    std::vector<size_t> obu_indices;  // global OBU indices in this group
    ObuType obu_type;
    bool has_metadata_type;
    uint32_t metadata_type;
  };

  struct AnalysisResult {
    size_t total_obus;
    size_t frame_count;  // temporal delimiters in the full (unfiltered) stream
    size_t unique_payloads;
    size_t duplicate_obus;           // total_obus - unique_payloads
    uint64_t total_obu_bytes;        // header + payload for all analysed OBUs
    uint64_t redundant_obu_bytes;    // bytes that could be eliminated
    std::vector<DedupGroup> groups;  // sorted by count descending
    // Runs: sequences of consecutive identical OBUs
    size_t max_run_length;
    double avg_run_length;
  };

  AnalysisResult analyze(const std::set<ObuType>& obu_filter = {},
                         const std::set<uint32_t>& meta_filter = {}) const {
    // Collect filtered OBU indices
    std::vector<size_t> indices;
    for (size_t i = 0; i < obus_.size(); ++i) {
      if (passes_filter(obus_[i], obu_filter, meta_filter))
        indices.push_back(i);
    }

    // Group by payload hash
    std::map<uint64_t, DedupGroup> by_hash;
    uint64_t total_bytes = 0;
    for (size_t idx : indices) {
      const ObuInfo& o = obus_[idx];
      uint64_t h = fnv1a(o.raw_payload);
      auto& g = by_hash[h];
      if (g.count == 0) {
        g.hash = h;
        g.header_size = o.header.header_size;
        g.payload_size = o.raw_payload.size();
        g.obu_size = o.header.header_size + o.raw_payload.size();
        g.obu_type = o.header.obu_type;
        g.has_metadata_type = o.has_metadata;
        g.metadata_type = o.has_metadata ? o.metadata.metadata_type : 0;
      }
      g.count++;
      g.obu_indices.push_back(idx);
      total_bytes += o.header.header_size + o.raw_payload.size();
    }

    // Sort groups by count descending
    std::vector<DedupGroup> groups;
    groups.reserve(by_hash.size());
    for (auto& [h, g] : by_hash)
      groups.push_back(std::move(g));
    std::sort(groups.begin(), groups.end(),
              [](const DedupGroup& a, const DedupGroup& b) { return a.count > b.count; });

    // Redundant bytes: for each group, (count - 1) copies are redundant
    uint64_t redundant = 0;
    for (const auto& g : groups)
      redundant += static_cast<uint64_t>(g.count - 1) * g.obu_size;

    // Run-length analysis (consecutive identical payloads)
    size_t max_run = 0, run = 1;
    size_t total_runs = indices.empty() ? 0 : 1;
    uint64_t run_sum = 0;
    for (size_t i = 1; i < indices.size(); ++i) {
      if (obus_[indices[i]].raw_payload == obus_[indices[i - 1]].raw_payload) {
        ++run;
      } else {
        run_sum += run;
        if (run > max_run)
          max_run = run;
        run = 1;
        ++total_runs;
      }
    }
    if (!indices.empty()) {
      run_sum += run;
      if (run > max_run)
        max_run = run;
    }

    AnalysisResult r;
    r.total_obus = indices.size();
    r.frame_count = 0;
    for (const auto& o : obus_) {
      if (o.header.obu_type == ObuType::kTemporalDelimiter)
        ++r.frame_count;
    }
    r.unique_payloads = groups.size();
    r.duplicate_obus = indices.size() > groups.size() ? indices.size() - groups.size() : 0;
    r.total_obu_bytes = total_bytes;
    r.redundant_obu_bytes = redundant;
    r.groups = std::move(groups);
    r.max_run_length = max_run;
    r.avg_run_length = total_runs > 0 ? static_cast<double>(run_sum) / total_runs : 0.0;
    return r;
  }

  void dump_analysis(const std::set<ObuType>& obu_filter = {},
                     const std::set<uint32_t>& meta_filter = {}, double fps = 30.0,
                     bool fps_is_default = true) const {
    auto r = analyze(obu_filter, meta_filter);

    std::cout << "=== AV1 OBU Redundancy Analysis ===" << std::endl;
    std::cout << "File: " << file_path_ << std::endl;
    std::cout << "Frame rate: " << fps << " fps";
    if (fps_is_default)
      std::cout << " (default)";
    std::cout << std::endl << std::endl;

    std::cout << "--- Summary ---" << std::endl;
    std::cout << "OBUs analysed:        " << r.total_obus << std::endl;
    std::cout << "Frames (TDs):         " << r.frame_count << std::endl;
    std::cout << "Unique payloads:      " << r.unique_payloads << std::endl;
    std::cout << "Duplicate OBUs:       " << r.duplicate_obus;
    if (r.total_obus > 0) {
      double pct = 100.0 * r.duplicate_obus / r.total_obus;
      std::cout << "  (" << std::fixed << std::setprecision(1) << pct << "%)";
    }
    std::cout << std::endl;
    std::cout << "Total OBU bytes:      " << r.total_obu_bytes << std::endl;
    std::cout << "Redundant bytes:      " << r.redundant_obu_bytes;
    if (r.total_obu_bytes > 0) {
      double pct = 100.0 * r.redundant_obu_bytes / r.total_obu_bytes;
      std::cout << "  (" << std::fixed << std::setprecision(1) << pct << "%)";
    }
    std::cout << std::endl;
    {
      double duration_s = r.frame_count / fps;
      double total_kbps = r.total_obu_bytes * 8.0 / duration_s / 1000.0;
      double redundant_kbps = r.redundant_obu_bytes * 8.0 / duration_s / 1000.0;
      std::cout << "Total bitrate:        " << std::fixed << std::setprecision(2) << total_kbps
                << " kbps" << std::endl;
      std::cout << "Redundant bitrate:    " << std::fixed << std::setprecision(2) << redundant_kbps
                << " kbps" << std::endl;
    }
    std::cout << "Max consecutive run:  " << r.max_run_length << std::endl;
    std::cout << "Avg run length:       " << std::fixed << std::setprecision(2) << r.avg_run_length
              << std::endl;
    std::cout << std::endl;

    std::cout << "--- Payload groups (by frequency) ---" << std::endl;
    double duration_s = r.frame_count / fps;
    for (size_t i = 0; i < r.groups.size(); ++i) {
      const auto& g = r.groups[i];
      if (g.count < 2)
        break;  // groups are sorted by count desc; no more duplicates after this
      uint64_t total_group_bytes = static_cast<uint64_t>(g.count) * g.obu_size;
      double group_kbps = total_group_bytes * 8.0 / duration_s / 1000.0;
      std::cout << "Group " << (i + 1) << ": " << g.count << "x  " << g.obu_size
                << " bytes  total=" << total_group_bytes << " bytes  " << std::fixed
                << std::setprecision(2) << group_kbps << " kbps  ";
      // OBU type label
      std::cout << obu_type_name(g.obu_type);
      if (g.has_metadata_type)
        std::cout << "/" << metadata_type_name(g.metadata_type);
      std::cout << "  hash=0x" << std::hex << std::setw(16) << std::setfill('0') << g.hash
                << std::dec;
      if (g.count > 1) {
        // Show first/last occurrence
        std::cout << "  OBUs #" << g.obu_indices.front() << "..#" << g.obu_indices.back();
      } else {
        std::cout << "  OBU #" << g.obu_indices.front();
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }

  json analyze_to_json(const std::set<ObuType>& obu_filter = {},
                       const std::set<uint32_t>& meta_filter = {}, double fps = 30.0,
                       bool fps_is_default = true) const {
    auto r = analyze(obu_filter, meta_filter);

    json j;
    j["file"] = file_path_;
    j["fps"] = fps;
    j["fps_is_default"] = fps_is_default;

    json js;
    js["obus_analysed"] = r.total_obus;
    js["frame_count"] = r.frame_count;
    js["unique_payloads"] = r.unique_payloads;
    js["duplicate_obus"] = r.duplicate_obus;
    js["duplicate_obus_pct"] = r.total_obus > 0 ? 100.0 * r.duplicate_obus / r.total_obus : 0.0;
    js["total_obu_bytes"] = r.total_obu_bytes;
    js["redundant_obu_bytes"] = r.redundant_obu_bytes;
    js["redundant_obu_pct"] =
      r.total_obu_bytes > 0 ? 100.0 * r.redundant_obu_bytes / r.total_obu_bytes : 0.0;
    {
      double duration_s = r.frame_count / fps;
      js["total_kbps"] = r.total_obu_bytes * 8.0 / duration_s / 1000.0;
      js["redundant_kbps"] = r.redundant_obu_bytes * 8.0 / duration_s / 1000.0;
    }
    js["max_consecutive_run"] = r.max_run_length;
    js["avg_run_length"] = r.avg_run_length;
    j["summary"] = js;

    json jg = json::array();
    for (size_t i = 0; i < r.groups.size(); ++i) {
      const auto& g = r.groups[i];
      if (g.count < 2)
        break;
      json jgi;
      jgi["rank"] = i + 1;
      jgi["obu_type_name"] = obu_type_name(g.obu_type);
      if (g.has_metadata_type)
        jgi["metadata_type_name"] = metadata_type_name(g.metadata_type);
      jgi["count"] = g.count;
      jgi["obu_size"] = g.obu_size;
      jgi["header_size"] = g.header_size;
      jgi["payload_size"] = g.payload_size;
      jgi["total_bytes"] = static_cast<uint64_t>(g.count) * g.obu_size;
      jgi["kbps"] = static_cast<uint64_t>(g.count) * g.obu_size * 8.0 /
                    (static_cast<double>(r.frame_count) / fps) / 1000.0;
      // hex string for the hash
      std::ostringstream oss;
      oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << g.hash;
      jgi["hash"] = oss.str();
      jgi["obu_indices"] = g.obu_indices;
      jg.push_back(jgi);
    }
    j["groups"] = jg;
    return j;
  }

private:
  std::string file_path_;
  size_t file_size_ = 0;
  std::vector<uint8_t> file_data_;
  std::vector<ObuInfo> obus_;
  bool from_mp4_ = false;

  // Parse an OBU directly from an arbitrary buffer (used for MP4 sample parsing).
  // encoded_offset and the tu/fu indices are passed in directly.
  void parse_obu_from_buffer(const uint8_t* data, size_t total_obu_bytes, uint64_t encoded_offset,
                             int tu_idx, int fu_idx) {
    if (total_obu_bytes == 0)
      return;

    BitReader br(data, total_obu_bytes);

    ObuInfo info{};
    info.file_offset = encoded_offset;
    info.obu_length = total_obu_bytes;
    info.temporal_unit_index = tu_idx;
    info.frame_unit_index = fu_idx;
    info.has_metadata = false;

    if (!parse_obu_header(br, info.header)) {
      spdlog::warn("Failed to parse OBU header (encoded_offset=0x{:x})", encoded_offset);
      return;
    }

    uint64_t declared_obu_size = 0;
    if (info.header.has_size_field) {
      int sz_bytes = 0;
      declared_obu_size = br.leb128(sz_bytes);
      if (br.error()) {
        spdlog::warn("Failed to read obu_size leb128 (encoded_offset=0x{:x})", encoded_offset);
        return;
      }
    } else {
      uint64_t header_bytes = info.header.extension_flag ? 2 : 1;
      declared_obu_size =
        (total_obu_bytes > header_bytes) ? total_obu_bytes - header_bytes : 0;
    }
    info.payload_size = declared_obu_size;

    size_t payload_start = br.byte_offset();
    size_t payload_len = static_cast<size_t>(declared_obu_size);
    if (payload_start + payload_len <= total_obu_bytes) {
      info.raw_payload.assign(data + payload_start, data + payload_start + payload_len);
    }

    if (info.header.obu_type == ObuType::kMetadata) {
      parse_metadata_obu_payload(br, declared_obu_size, info);
    }

    obus_.push_back(std::move(info));
  }

  // ---- Bitstream parsing: temporal_unit / frame_unit / OBU hierarchy ----
  void parse_bitstream() {    size_t pos = 0;
    int tu_idx = 0;

    while (pos < file_size_) {
      // temporal_unit_size  leb128()
      int leb_bytes = 0;
      uint64_t tu_size = read_leb128(pos, leb_bytes);
      if (leb_bytes == 0 || pos + tu_size > file_size_) {
        spdlog::warn("Truncated temporal_unit_size at offset {}", pos);
        break;
      }
      pos += static_cast<size_t>(leb_bytes);
      size_t tu_end = pos + static_cast<size_t>(tu_size);

      parse_temporal_unit(pos, tu_end, tu_idx);
      pos = tu_end;
      ++tu_idx;
    }
  }

  void parse_temporal_unit(size_t& pos, size_t end, int tu_idx) {
    int fu_idx = 0;
    while (pos < end) {
      // frame_unit_size  leb128()
      int leb_bytes = 0;
      uint64_t fu_size = read_leb128(pos, leb_bytes);
      if (leb_bytes == 0 || pos + leb_bytes + fu_size > end) {
        spdlog::warn("Truncated frame_unit_size in temporal_unit {} at offset {}", tu_idx, pos);
        break;
      }
      pos += static_cast<size_t>(leb_bytes);
      size_t fu_end = pos + static_cast<size_t>(fu_size);

      parse_frame_unit(pos, fu_end, tu_idx, fu_idx);
      pos = fu_end;
      ++fu_idx;
    }
  }

  void parse_frame_unit(size_t& pos, size_t end, int tu_idx, int fu_idx) {
    while (pos < end) {
      // obu_length  leb128()
      size_t obu_length_offset = pos;
      int leb_bytes = 0;
      uint64_t obu_length = read_leb128(pos, leb_bytes);
      if (leb_bytes == 0 || pos + leb_bytes + obu_length > end) {
        spdlog::warn("Truncated obu_length in frame_unit at offset {}", pos);
        break;
      }
      pos += static_cast<size_t>(leb_bytes);

      // open_bitstream_unit(obu_length)
      parse_obu(pos, obu_length, obu_length_offset, obu_length, tu_idx, fu_idx);
      pos += static_cast<size_t>(obu_length);
    }
  }

  void parse_obu(size_t obu_start, uint64_t obu_length, uint64_t file_offset,
                 uint64_t raw_obu_length, int tu_idx, int fu_idx) {
    if (obu_length == 0)
      return;
    if (obu_start + obu_length > file_size_) {
      spdlog::warn("OBU extends beyond file at offset {}", obu_start);
      return;
    }

    const uint8_t* data = file_data_.data() + obu_start;
    BitReader br(data, static_cast<size_t>(obu_length));

    ObuInfo info{};
    info.file_offset = file_offset;
    info.obu_length = raw_obu_length;
    info.temporal_unit_index = tu_idx;
    info.frame_unit_index = fu_idx;
    info.has_metadata = false;

    // Parse obu_header()
    if (!parse_obu_header(br, info.header)) {
      spdlog::warn("Failed to parse OBU header at offset {}", obu_start);
      return;
    }

    // Determine payload size
    // The spec says: obu_size is the size of the payload NOT including the header
    // or the obu_size field itself.
    uint64_t declared_obu_size = 0;
    if (info.header.has_size_field) {
      int sz_bytes = 0;
      declared_obu_size = br.leb128(sz_bytes);
      if (br.error()) {
        spdlog::warn("Failed to read obu_size leb128 at offset {}", obu_start);
        return;
      }
    } else {
      // obu_size = sz - 1 - obu_extension_flag  (sz is obu_length here)
      uint64_t header_bytes = info.header.extension_flag ? 2 : 1;
      declared_obu_size = (obu_length > header_bytes) ? obu_length - header_bytes : 0;
    }
    info.payload_size = declared_obu_size;

    // Store raw payload bytes for analysis
    size_t payload_start = obu_start + br.byte_offset();
    size_t payload_len = static_cast<size_t>(declared_obu_size);
    if (payload_start + payload_len <= file_size_) {
      info.raw_payload.assign(file_data_.data() + payload_start,
                              file_data_.data() + payload_start + payload_len);
    }

    // Parse metadata OBU payload if applicable
    if (info.header.obu_type == ObuType::kMetadata) {
      parse_metadata_obu_payload(br, declared_obu_size, info);
    }

    obus_.push_back(std::move(info));
  }

  void parse_metadata_obu_payload(BitReader& br, uint64_t payload_size, ObuInfo& info) {
    if (payload_size == 0)
      return;

    // metadata_type  leb128()
    int meta_type_bytes = 0;
    uint64_t mtype = br.leb128(meta_type_bytes);
    if (br.error()) {
      spdlog::warn("Failed to read metadata_type");
      return;
    }

    MetadataObu meta{};
    meta.metadata_type = static_cast<uint32_t>(mtype);
    meta.parsed = false;

    size_t remaining = (payload_size > static_cast<uint64_t>(meta_type_bytes))
                         ? static_cast<size_t>(payload_size) - meta_type_bytes
                         : 0;

    if (mtype == static_cast<uint64_t>(MetadataType::kItutT35)) {
      parse_metadata_itut_t35(br, remaining, meta.itut_t35);
      meta.parsed = !br.error();
    } else if (mtype == static_cast<uint64_t>(MetadataType::kHdrCll)) {
      if (remaining >= 4) {
        parse_metadata_hdr_cll(br, meta.hdr_cll);
        meta.parsed = !br.error();
      }
    } else if (mtype == static_cast<uint64_t>(MetadataType::kHdrMdcv)) {
      if (remaining >= 24) {
        parse_metadata_hdr_mdcv(br, meta.hdr_mdcv);
        meta.parsed = !br.error();
      }
    }
    // SCALABILITY and TIMECODE are intentionally not parsed.

    info.has_metadata = true;
    info.metadata = meta;
  }

  // ---- Output helpers ----

  void dump_metadata(const MetadataObu& m, const std::string& indent) const {
    std::cout << indent << "Metadata type: " << metadata_type_name(m.metadata_type) << " ("
              << m.metadata_type << ")";
    if (!m.parsed) {
      std::cout << " [not parsed]" << std::endl;
      return;
    }
    std::cout << std::endl;

    if (m.metadata_type == static_cast<uint32_t>(MetadataType::kItutT35)) {
      const auto& t = m.itut_t35;
      std::cout << indent << "itu_t_t35_country_code: 0x" << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(t.country_code) << std::dec << std::endl;
      if (t.country_code == 0xFF) {
        std::cout << indent << "itu_t_t35_country_code_extension: 0x" << std::hex << std::setw(2)
                  << std::setfill('0') << static_cast<int>(t.country_code_extension) << std::dec
                  << std::endl;
      }
      std::cout << indent << "itu_t_t35_payload_bytes: " << t.payload_bytes.size() << " bytes";
      if (!t.payload_bytes.empty()) {
        std::cout << " [";
        size_t show = std::min(t.payload_bytes.size(), size_t(16));
        for (size_t i = 0; i < show; ++i) {
          if (i)
            std::cout << " ";
          std::cout << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(t.payload_bytes[i]);
        }
        std::cout << std::dec;
        if (t.payload_bytes.size() > 16)
          std::cout << " ...";
        std::cout << "]";
      }
      std::cout << std::endl;
    } else if (m.metadata_type == static_cast<uint32_t>(MetadataType::kHdrCll)) {
      const auto& c = m.hdr_cll;
      std::cout << indent << "max_cll: " << c.max_cll << std::endl;
      std::cout << indent << "max_fall: " << c.max_fall << std::endl;
    } else if (m.metadata_type == static_cast<uint32_t>(MetadataType::kHdrMdcv)) {
      const auto& d = m.hdr_mdcv;
      const char* primary_names[3] = {"R", "G", "B"};
      for (int i = 0; i < 3; ++i) {
        std::cout << indent << "primary_chromaticity_x[" << primary_names[i]
                  << "]: " << d.primary_chromaticity_x[i] << std::endl;
        std::cout << indent << "primary_chromaticity_y[" << primary_names[i]
                  << "]: " << d.primary_chromaticity_y[i] << std::endl;
      }
      std::cout << indent << "white_point_chromaticity_x: " << d.white_point_chromaticity_x
                << std::endl;
      std::cout << indent << "white_point_chromaticity_y: " << d.white_point_chromaticity_y
                << std::endl;
      std::cout << indent << "luminance_max: " << d.luminance_max << std::endl;
      std::cout << indent << "luminance_min: " << d.luminance_min << std::endl;
    }
  }

  json metadata_to_json(const MetadataObu& m) const {
    json jm;
    jm["metadata_type"] = m.metadata_type;
    jm["metadata_type_name"] = metadata_type_name(m.metadata_type);
    jm["parsed"] = m.parsed;

    if (!m.parsed)
      return jm;

    if (m.metadata_type == static_cast<uint32_t>(MetadataType::kItutT35)) {
      const auto& t = m.itut_t35;
      json ji;
      ji["itu_t_t35_country_code"] = t.country_code;
      if (t.country_code == 0xFF)
        ji["itu_t_t35_country_code_extension"] = t.country_code_extension;
      // Store payload as hex string
      std::ostringstream oss;
      for (size_t i = 0; i < t.payload_bytes.size(); ++i) {
        if (i)
          oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(t.payload_bytes[i]);
      }
      ji["payload_bytes_hex"] = oss.str();
      ji["payload_size"] = t.payload_bytes.size();
      jm["itut_t35"] = ji;
    } else if (m.metadata_type == static_cast<uint32_t>(MetadataType::kHdrCll)) {
      json jc;
      jc["max_cll"] = m.hdr_cll.max_cll;
      jc["max_fall"] = m.hdr_cll.max_fall;
      jm["hdr_cll"] = jc;
    } else if (m.metadata_type == static_cast<uint32_t>(MetadataType::kHdrMdcv)) {
      const auto& d = m.hdr_mdcv;
      json jd;
      json primaries = json::array();
      for (int i = 0; i < 3; ++i) {
        primaries.push_back(
          {{"x", d.primary_chromaticity_x[i]}, {"y", d.primary_chromaticity_y[i]}});
      }
      jd["primaries_RGB"] = primaries;
      jd["white_point"] = {{"x", d.white_point_chromaticity_x},
                           {"y", d.white_point_chromaticity_y}};
      jd["luminance_max"] = d.luminance_max;
      jd["luminance_min"] = d.luminance_min;
      jm["hdr_mdcv"] = jd;
    }
    return jm;
  }

  // Read a LEB128 value from file_data_[pos] without advancing pos.
  // On success, bytes_read > 0 and the returned value is valid.
  // On error, bytes_read == 0.
  uint64_t read_leb128(size_t pos, int& bytes_read) const {
    bytes_read = 0;
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
      if (pos + i >= file_size_)
        return 0;
      uint8_t b = file_data_[pos + i];
      value |= static_cast<uint64_t>(b & 0x7F) << (i * 7);
      ++bytes_read;
      if (!(b & 0x80))
        break;
    }
    return value;
  }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
  // Setup logging
  auto console = spdlog::stdout_color_mt("av1_tool");
  spdlog::set_default_logger(console);
  spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  spdlog::set_level(spdlog::level::warn);

  CLI::App app{"AV1 OBU Tool - slim AV1 bitstream parser"};

  bool verbose = false;
  bool json_output = false;
  bool analyze = false;
  double fps = 30.0;
  bool fps_is_default = true;
  std::string input_file;
  std::string output_file;
  std::vector<std::string> type_filters;
  std::vector<std::string> meta_type_filters;

  app.add_flag("-v,--verbose", verbose, "Enable verbose/debug logging");
  app.add_flag("-j,--json", json_output, "Output in JSON format");
  app.add_flag("-a,--analyze", analyze,
               "Analyse payload redundancy instead of dumping OBU details");
  app
    .add_option("--fps", fps,
                "Frame rate (frames/s) used to compute kbps in --analyze reports (default: 30)")
    ->capture_default_str()
    ->each([&fps_is_default](const std::string&) { fps_is_default = false; });
  app.add_option("file", input_file, "Input AV1 bitstream file")->required();
  app.add_option("-o,--output", output_file, "Output file (default: stdout)");
  app.add_option("-t,--type", type_filters,
                 "Filter by OBU type (name or number, repeatable). "
                 "E.g. --type METADATA --type SEQUENCE_HEADER");
  app.add_option("-m,--metadata-type", meta_type_filters,
                 "Filter by metadata type (name or number, repeatable, implies --type METADATA). "
                 "E.g. --metadata-type T35 --metadata-type HDR_CLL");

  CLI11_PARSE(app, argc, argv);

  if (verbose)
    spdlog::set_level(spdlog::level::debug);

  // Resolve OBU type filter strings -> set<ObuType>
  std::set<ObuType> obu_filter;
  for (const auto& s : type_filters) {
    ObuType t;
    if (!parse_obu_type_filter(s, t)) {
      spdlog::error(
        "Unknown OBU type '{}'. Valid names: SEQUENCE_HEADER, "
        "TEMPORAL_DELIMITER, FRAME_HEADER, TILE_GROUP, METADATA, "
        "FRAME, REDUNDANT_FRAME_HEADER, TILE_LIST, PADDING, "
        "or a numeric value 0-15.",
        s);
      return 1;
    }
    obu_filter.insert(t);
  }

  // Resolve metadata type filter strings -> set<uint32_t>
  std::set<uint32_t> meta_filter;
  for (const auto& s : meta_type_filters) {
    uint32_t t;
    if (!parse_metadata_type_filter(s, t)) {
      spdlog::error(
        "Unknown metadata type '{}'. Valid names: HDR_CLL, HDR_MDCV, "
        "SCALABILITY, ITUT_T35 (or T35), TIMECODE, or a numeric value.",
        s);
      return 1;
    }
    meta_filter.insert(t);
  }

  Av1Parser parser;

  // Auto-detect MP4 container by file extension
  auto has_mp4_ext = [](const std::string& path) {
    std::string lower;
    for (char c : path)
      lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.size() >= 4 &&
           (lower.substr(lower.size() - 4) == ".mp4" ||
            lower.substr(lower.size() - 4) == ".mov" ||
            lower.substr(lower.size() - 4) == ".m4v");
  };

  if (has_mp4_ext(input_file)) {
#ifdef AV1_TOOL_WITH_MP4
    if (!parser.parse_mp4(input_file))
      return 1;
#else
    spdlog::error("MP4 input requires building with BUILD_PACKAGER=ON");
    return 1;
#endif
  } else {
    if (!parser.parse_file(input_file))
      return 1;
  }

  // Helper lambda to write a string to stdout or file
  auto write_output = [&](const std::string& content) {
    if (!output_file.empty()) {
      std::ofstream ofs(output_file);
      if (!ofs) {
        spdlog::error("Cannot open output file: {}", output_file);
        return false;
      }
      ofs << content;
    } else {
      std::cout << content;
    }
    return true;
  };

  if (analyze) {
    if (json_output) {
      if (!write_output(
            parser.analyze_to_json(obu_filter, meta_filter, fps, fps_is_default).dump(2) + "\n"))
        return 1;
    } else {
      parser.dump_analysis(obu_filter, meta_filter, fps, fps_is_default);
    }
  } else if (json_output) {
    if (!write_output(parser.to_json(obu_filter, meta_filter).dump(2) + "\n"))
      return 1;
  } else {
    parser.dump(obu_filter, meta_filter);
  }

  return 0;
}
