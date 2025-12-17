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

#include <spdlog/spdlog.h>

#include <cstdio>

#include <av2_obu/obus/metadata_unit.h>

namespace av2_obu {

bool MetadataUnit::parse_simple_header(BitstreamReader& br) {
  // Parse short metadata unit header
  muh_layer_idc_ = static_cast<uint8_t>(br.read_bits(3));
  muh_cancel_flag_ = static_cast<uint8_t>(br.read_bits(1));
  muh_persistence_idc_ = static_cast<uint8_t>(br.read_bits(3));
  metadata_type_ = br.read_leb128();

  // Simple header has implicit values
  muh_header_size_ = 1;
  muh_payload_size_ = 0; // Not signaled
  muh_priority_ = 0;     // Default

  spdlog::debug("  MetadataUnit (simple header):");
  spdlog::debug("    muh_layer_idc: {}", muh_layer_idc_);
  spdlog::debug("    muh_cancel_flag: {}", muh_cancel_flag_);
  spdlog::debug("    muh_persistence_idc: {}", muh_persistence_idc_);
  spdlog::debug("    metadata_type: {} ({})", metadata_type_, to_string(get_metadata_type()));

  return true;
}

bool MetadataUnit::parse_group_header(BitstreamReader& br, uint32_t obu_xlayer_id) {
  // Parse metadata group unit header 
  metadata_type_ = br.read_leb128();

  uint8_t header_byte = static_cast<uint8_t>(br.read_bits(8));
  muh_header_size_ = (header_byte >> 1) & 0x7F;
  muh_cancel_flag_ = header_byte & 0x1;

  uint32_t headerRemainingBytes = muh_header_size_;

  spdlog::debug("  MetadataUnit (group header):");
  spdlog::debug("    metadata_type: {} ({})", metadata_type_, to_string(get_metadata_type()));
  spdlog::debug("    muh_header_size: {}", muh_header_size_);
  spdlog::debug("    muh_cancel_flag: {}", muh_cancel_flag_);

  if (!muh_cancel_flag_) {
    // Read muh_payload_size
    uint32_t payload_size_start = br.bits_read();
    muh_payload_size_ = br.read_leb128();
    uint32_t payload_size_end = br.bits_read();
    uint32_t leb128Bytes = (payload_size_end - payload_size_start) / 8;
    headerRemainingBytes -= leb128Bytes;

    // Read layer_idc (3), persistence_idc (3), priority high (2)
    uint8_t byte2 = static_cast<uint8_t>(br.read_bits(8));
    muh_layer_idc_ = (byte2 >> 5) & 0x7;
    muh_persistence_idc_ = (byte2 >> 2) & 0x7;
    uint32_t priority_high = byte2 & 0x3;

    // Read priority low (6), reserved (2)
    uint8_t byte3 = static_cast<uint8_t>(br.read_bits(8));
    uint32_t priority_low = (byte3 >> 2) & 0x3F;
    muh_priority_ = (priority_high << 6) | priority_low;
    muh_reserved_zero_2bits_ = byte3 & 0x3;

    headerRemainingBytes -= 2;

    spdlog::debug("    muh_payload_size: {}", muh_payload_size_);
    spdlog::debug("    muh_layer_idc: {}", muh_layer_idc_);
    spdlog::debug("    muh_persistence_idc: {}", muh_persistence_idc_);
    spdlog::debug("    muh_priority: {}", muh_priority_);

    // Handle layer mapping
    if (muh_layer_idc_ == static_cast<uint32_t>(LayerIdc::LAYER_VALUES)) {
      if (obu_xlayer_id == 31) {
        muh_xlayer_map_ = static_cast<uint32_t>(br.read_bits(32));
        headerRemainingBytes -= 4;

        spdlog::debug("    muh_xlayer_map: 0x{:08X}", muh_xlayer_map_);

        for (uint32_t n = 0; n < 31; n++) {
          if (muh_xlayer_map_ & (0x1 << n)) {
            uint8_t mlayer_map = static_cast<uint8_t>(br.read_bits(8));
            muh_mlayer_maps_.push_back(mlayer_map);
            headerRemainingBytes -= 1;
          }
        }
      } else {
        uint8_t mlayer_map = static_cast<uint8_t>(br.read_bits(8));
        muh_mlayer_maps_.push_back(mlayer_map);
        headerRemainingBytes -= 1;
      }
    }
  }

  // Read remaining header extension bytes
  for (uint32_t j = 0; j < headerRemainingBytes; j++) {
    uint8_t ext_byte = static_cast<uint8_t>(br.read_bits(8));
    muh_header_extension_bytes_.push_back(ext_byte);
  }

  if (!muh_header_extension_bytes_.empty()) {
    spdlog::debug("    Read {} header extension bytes", muh_header_extension_bytes_.size());
  }

  return true;
}

bool MetadataUnit::parse_payload(BitstreamReader& br) {
  MetadataType type = get_metadata_type();
  spdlog::debug("  Parsing metadata_unit payload for type: {}", to_string(type));

  // For metadata group units, we know the exact payload size
  // Track position to ensure we consume the correct number of bytes
  uint32_t start_bit_pos = 0;
  bool has_payload_size = (muh_payload_size_ > 0);

  if (has_payload_size) {
    start_bit_pos = static_cast<uint32_t>(br.bits_read());
    spdlog::debug("    Payload size: {} bytes ({} bits)", muh_payload_size_, muh_payload_size_ * 8);
  }

  switch (type) {
    case MetadataType::HDR_CLL:
      spdlog::debug("    Parsing HDR_CLL metadata");
      max_cll_ = static_cast<uint16_t>(br.read_bits(16));
      max_fall_ = static_cast<uint16_t>(br.read_bits(16));
      spdlog::debug("      max_cll: {}", max_cll_);
      spdlog::debug("      max_fall: {}", max_fall_);
      break;

    case MetadataType::HDR_MDCV:
      spdlog::debug("    Parsing HDR_MDCV metadata");
      for (int i = 0; i < 3; i++) {
        primary_chromaticity_x_[i] = static_cast<uint16_t>(br.read_bits(16));
        primary_chromaticity_y_[i] = static_cast<uint16_t>(br.read_bits(16));
      }
      white_point_chromaticity_x_ = static_cast<uint16_t>(br.read_bits(16));
      white_point_chromaticity_y_ = static_cast<uint16_t>(br.read_bits(16));
      luminance_max_ = static_cast<uint32_t>(br.read_bits(32));
      luminance_min_ = static_cast<uint32_t>(br.read_bits(32));

      spdlog::debug("      primary_chromaticity (G): ({}, {})", primary_chromaticity_x_[0], primary_chromaticity_y_[0]);
      spdlog::debug("      primary_chromaticity (B): ({}, {})", primary_chromaticity_x_[1], primary_chromaticity_y_[1]);
      spdlog::debug("      primary_chromaticity (R): ({}, {})", primary_chromaticity_x_[2], primary_chromaticity_y_[2]);
      spdlog::debug("      white_point_chromaticity: ({}, {})", white_point_chromaticity_x_, white_point_chromaticity_y_);
      spdlog::debug("      luminance_max: {}", luminance_max_);
      spdlog::debug("      luminance_min: {}", luminance_min_);
      break;

    case MetadataType::SCALABILITY:
      spdlog::debug("    TODO: Parse SCALABILITY metadata");
      break;

    case MetadataType::ITUT_T35:
      spdlog::debug("    Parsing ITUT_T35 metadata");
      {
        itu_t_t35_country_code_ = static_cast<uint8_t>(br.read_bits(8));
        spdlog::debug("      itu_t_t35_country_code: 0x{:02x}", itu_t_t35_country_code_);

        uint32_t payload_bytes_remaining = 0;
        if (has_payload_size && muh_payload_size_ > 0) {
          payload_bytes_remaining = muh_payload_size_ - 1;  // Minus country_code byte
        }

        if (itu_t_t35_country_code_ == 0xFF) {
          itu_t_t35_country_code_extension_byte_ = static_cast<uint8_t>(br.read_bits(8));
          spdlog::debug("      itu_t_t35_country_code_extension_byte: 0x{:02x}",
                        itu_t_t35_country_code_extension_byte_);
          if (payload_bytes_remaining > 0) {
            payload_bytes_remaining--;
          }
        }

        // For USA (0xB5) and Canada (0x20), parse terminal_provider_code
        if ((itu_t_t35_country_code_ == 0xB5 || itu_t_t35_country_code_ == 0x20) &&
            payload_bytes_remaining >= 2) {
          uint8_t provider_high = static_cast<uint8_t>(br.read_bits(8));
          uint8_t provider_low = static_cast<uint8_t>(br.read_bits(8));
          itu_t_t35_terminal_provider_code_ = (static_cast<uint16_t>(provider_high) << 8) | provider_low;
          spdlog::debug("      itu_t_t35_terminal_provider_code: 0x{:04x}",
                        itu_t_t35_terminal_provider_code_);
          payload_bytes_remaining -= 2;
        }

        // Read remaining payload bytes
        if (has_payload_size && payload_bytes_remaining > 0) {
          itu_t_t35_payload_bytes_.clear();
          for (uint32_t i = 0; i < payload_bytes_remaining; i++) {
            itu_t_t35_payload_bytes_.push_back(static_cast<uint8_t>(br.read_bits(8)));
          }
          spdlog::debug("      itu_t_t35_payload_bytes: {} bytes", itu_t_t35_payload_bytes_.size());
        } else if (!has_payload_size) {
          // Without known payload size, we can't safely read the rest
          spdlog::debug("      itu_t_t35_payload_bytes: (size unknown, not parsed)");
        }
      }
      break;

    case MetadataType::TIMECODE:
      spdlog::debug("    Parsing TIMECODE metadata");
      counting_type_ = static_cast<uint8_t>(br.read_bits(5));
      full_timestamp_flag_ = static_cast<uint8_t>(br.read_bits(1));
      discontinuity_flag_ = static_cast<uint8_t>(br.read_bits(1));
      cnt_dropped_flag_ = static_cast<uint8_t>(br.read_bits(1));
      n_frames_ = static_cast<uint16_t>(br.read_bits(9));

      spdlog::debug("      counting_type: {}", int(counting_type_));
      spdlog::debug("      full_timestamp_flag: {}", int(full_timestamp_flag_));
      spdlog::debug("      discontinuity_flag: {}", int(discontinuity_flag_));
      spdlog::debug("      cnt_dropped_flag: {}", int(cnt_dropped_flag_));
      spdlog::debug("      n_frames: {}", n_frames_);

      if (full_timestamp_flag_) {
        seconds_value_ = static_cast<uint8_t>(br.read_bits(6));
        minutes_value_ = static_cast<uint8_t>(br.read_bits(6));
        hours_value_ = static_cast<uint8_t>(br.read_bits(5));
        spdlog::debug("      seconds_value: {}", int(seconds_value_));
        spdlog::debug("      minutes_value: {}", int(minutes_value_));
        spdlog::debug("      hours_value: {}", int(hours_value_));
      } else {
        seconds_flag_ = static_cast<uint8_t>(br.read_bits(1));
        spdlog::debug("      seconds_flag: {}", int(seconds_flag_));
        if (seconds_flag_) {
          seconds_value_ = static_cast<uint8_t>(br.read_bits(6));
          minutes_flag_ = static_cast<uint8_t>(br.read_bits(1));
          spdlog::debug("      seconds_value: {}", int(seconds_value_));
          spdlog::debug("      minutes_flag: {}", int(minutes_flag_));
          if (minutes_flag_) {
            minutes_value_ = static_cast<uint8_t>(br.read_bits(6));
            hours_flag_ = static_cast<uint8_t>(br.read_bits(1));
            spdlog::debug("      minutes_value: {}", int(minutes_value_));
            spdlog::debug("      hours_flag: {}", int(hours_flag_));
            if (hours_flag_) {
              hours_value_ = static_cast<uint8_t>(br.read_bits(5));
              spdlog::debug("      hours_value: {}", int(hours_value_));
            }
          }
        }
      }

      time_offset_length_ = static_cast<uint8_t>(br.read_bits(5));
      spdlog::debug("      time_offset_length: {}", int(time_offset_length_));
      if (time_offset_length_ > 0) {
        time_offset_value_ = static_cast<uint32_t>(br.read_bits(time_offset_length_));
        spdlog::debug("      time_offset_value: {}", time_offset_value_);
      }
      break;

    case MetadataType::BANDING_HINTS:
      spdlog::debug("    TODO: Parse BANDING_HINTS metadata");
      break;

    case MetadataType::ICC_PROFILE:
      spdlog::debug("    TODO: Parse ICC_PROFILE metadata");
      break;

    case MetadataType::SCAN_TYPE:
      spdlog::debug("    TODO: Parse SCAN_TYPE metadata");
      break;

    case MetadataType::HASH:
      spdlog::debug("    Parsing HASH metadata (decoded frame hash)");
      {
        // Parse header byte
        hash_type_ = static_cast<uint8_t>(br.read_bits(4));
        per_plane_ = static_cast<uint8_t>(br.read_bits(1));
        has_grain_ = static_cast<uint8_t>(br.read_bits(1));
        hash_reserved_ = static_cast<uint8_t>(br.read_bits(2));

        spdlog::debug("      hash_type: {}", int(hash_type_));
        spdlog::debug("      per_plane: {}", int(per_plane_));
        spdlog::debug("      has_grain: {}", int(has_grain_));
        spdlog::debug("      reserved: {}", int(hash_reserved_));

        // Determine number of hashes to read
        uint32_t num_hashes = 1;  // Default: single frame_hash
        if (per_plane_) {
          // Per-plane hashes: determine num_planes
          if (has_payload_size && muh_payload_size_ > 0) {
            // payload_size = 1 byte header + (num_planes * 16 bytes)
            uint32_t hash_bytes = muh_payload_size_ - 1;
            num_hashes = hash_bytes / 16;
            spdlog::debug("      num_planes (from payload size): {}", num_hashes);
          } else {
            // Default assumption: 3 planes for YUV
            num_hashes = 3;
            spdlog::debug("      num_planes (assumed): {}", num_hashes);
          }
        } else {
          spdlog::debug("      Single frame_hash (all planes combined)");
        }

        // Read hashes (16 bytes each, little-endian)
        hashes_.clear();
        for (uint32_t i = 0; i < num_hashes; i++) {
          std::array<uint8_t, 16> hash;
          for (int j = 0; j < 16; j++) {
            hash[j] = static_cast<uint8_t>(br.read_bits(8));
          }
          hashes_.push_back(hash);

          // Log hash in hex format
          const char* hash_label = per_plane_ ? "plane_hash" : "frame_hash";
          spdlog::debug("      {}[{}]: {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                        hash_label, i, hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
                        hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
        }
      }
      break;

    default:
      spdlog::debug("    Unknown or reserved metadata type");
      break;
  }

  // For metadata group units with known payload size, ensure we consume exactly the right amount
  if (has_payload_size) {
    uint32_t current_bit_pos = static_cast<uint32_t>(br.bits_read());
    uint32_t bits_consumed = current_bit_pos - start_bit_pos;
    uint32_t expected_bits = muh_payload_size_ * 8;

    // Check for non-byte-aligned parsing
    if (bits_consumed % 8 != 0) {
      spdlog::debug("    Metadata payload parsing ended at non-byte-aligned position ({} bits, {} remainder)",
                    bits_consumed, bits_consumed % 8);
    }

    if (bits_consumed < expected_bits) {
      uint32_t bits_to_skip = expected_bits - bits_consumed;
      spdlog::debug("    Skipping {} remaining payload bits ({} bytes)", bits_to_skip, bits_to_skip / 8);

      // Skip remaining bits
      while (bits_to_skip >= 32) {
        br.read_bits(32);
        bits_to_skip -= 32;
      }
      if (bits_to_skip > 0) {
        br.read_bits(bits_to_skip);
      }
    } else if (bits_consumed > expected_bits) {
      spdlog::warn("    Consumed {} bits but expected {} bits - payload parsing may be incorrect",
                   bits_consumed, expected_bits);
      return false;
    }

    spdlog::debug("    Successfully consumed {} bytes of payload", muh_payload_size_);
  }

  return true;
}

void MetadataUnit::dump() const {
  spdlog::debug("    MetadataUnit {{");
  spdlog::debug("      metadata_type: {} ({})", metadata_type_, to_string(get_metadata_type()));
  spdlog::debug("      muh_header_size: {}", muh_header_size_);
  spdlog::debug("      muh_cancel_flag: {}", muh_cancel_flag_);

  if (!muh_cancel_flag_) {
    if (muh_payload_size_ > 0) {
      spdlog::debug("      muh_payload_size: {}", muh_payload_size_);
    }
    spdlog::debug("      muh_layer_idc: {}", muh_layer_idc_);
    spdlog::debug("      muh_persistence_idc: {}", muh_persistence_idc_);
    if (muh_priority_ > 0) {
      spdlog::debug("      muh_priority: {}", muh_priority_);
    }

    // Display metadata payload fields
    MetadataType type = get_metadata_type();
    if (type == MetadataType::HDR_CLL) {
      spdlog::debug("      max_cll: {}", max_cll_);
      spdlog::debug("      max_fall: {}", max_fall_);
    } else if (type == MetadataType::HDR_MDCV) {
      spdlog::debug("      primary_chromaticity (G): ({}, {})", primary_chromaticity_x_[0], primary_chromaticity_y_[0]);
      spdlog::debug("      primary_chromaticity (B): ({}, {})", primary_chromaticity_x_[1], primary_chromaticity_y_[1]);
      spdlog::debug("      primary_chromaticity (R): ({}, {})", primary_chromaticity_x_[2], primary_chromaticity_y_[2]);
      spdlog::debug("      white_point_chromaticity: ({}, {})", white_point_chromaticity_x_, white_point_chromaticity_y_);
      spdlog::debug("      luminance_max: {}", luminance_max_);
      spdlog::debug("      luminance_min: {}", luminance_min_);
    } else if (type == MetadataType::ITUT_T35) {
      spdlog::debug("      itu_t_t35_country_code: 0x{:02x}", itu_t_t35_country_code_);
      if (itu_t_t35_country_code_ == 0xFF) {
        spdlog::debug("      itu_t_t35_country_code_extension_byte: 0x{:02x}",
                      itu_t_t35_country_code_extension_byte_);
      }
      if (itu_t_t35_terminal_provider_code_ != 0) {
        spdlog::debug("      itu_t_t35_terminal_provider_code: 0x{:04x}",
                      itu_t_t35_terminal_provider_code_);
      }
      if (!itu_t_t35_payload_bytes_.empty()) {
        spdlog::debug("      itu_t_t35_payload_bytes: {} bytes", itu_t_t35_payload_bytes_.size());
      }
    } else if (type == MetadataType::TIMECODE) {
      spdlog::debug("      counting_type: {}", int(counting_type_));
      spdlog::debug("      full_timestamp_flag: {}", int(full_timestamp_flag_));
      spdlog::debug("      discontinuity_flag: {}", int(discontinuity_flag_));
      spdlog::debug("      cnt_dropped_flag: {}", int(cnt_dropped_flag_));
      spdlog::debug("      n_frames: {}", n_frames_);
      if (full_timestamp_flag_ || seconds_flag_) {
        spdlog::debug("      seconds_value: {}", int(seconds_value_));
      }
      if (full_timestamp_flag_ || minutes_flag_) {
        spdlog::debug("      minutes_value: {}", int(minutes_value_));
      }
      if (full_timestamp_flag_ || hours_flag_) {
        spdlog::debug("      hours_value: {}", int(hours_value_));
      }
      if (time_offset_length_ > 0) {
        spdlog::debug("      time_offset_length: {}", int(time_offset_length_));
        spdlog::debug("      time_offset_value: {}", time_offset_value_);
      }
    } else if (type == MetadataType::HASH) {
      spdlog::debug("      hash_type: {}", int(hash_type_));
      spdlog::debug("      per_plane: {}", int(per_plane_));
      spdlog::debug("      has_grain: {}", int(has_grain_));
      const char* hash_label = per_plane_ ? "plane_hash" : "frame_hash";
      for (size_t i = 0; i < hashes_.size(); i++) {
        const auto& hash = hashes_[i];
        spdlog::debug("      {}[{}]: {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                      hash_label, i, hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
                      hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
      }
    }
  }

  spdlog::debug("    }}");
}

nlohmann::ordered_json MetadataUnit::to_json() const {
  nlohmann::ordered_json j = {{"metadata_type", metadata_type_},
                              {"metadata_type_name", to_string(get_metadata_type())},
                              {"muh_header_size", muh_header_size_},
                              {"muh_cancel_flag", muh_cancel_flag_}};

  if (!muh_cancel_flag_) {
    if (muh_payload_size_ > 0) {
      j["muh_payload_size"] = muh_payload_size_;
    }
    j["muh_layer_idc"] = muh_layer_idc_;
    j["muh_persistence_idc"] = muh_persistence_idc_;
    if (muh_priority_ > 0) {
      j["muh_priority"] = muh_priority_;
    }

    if (muh_layer_idc_ == static_cast<uint32_t>(LayerIdc::LAYER_VALUES)) {
      if (muh_xlayer_map_ != 0) {
        j["muh_xlayer_map"] = muh_xlayer_map_;
      }
      if (!muh_mlayer_maps_.empty()) {
        j["muh_mlayer_maps"] = muh_mlayer_maps_;
      }
    }

    // Add metadata payload fields to JSON
    MetadataType type = get_metadata_type();
    if (type == MetadataType::HDR_CLL) {
      j["max_cll"] = max_cll_;
      j["max_fall"] = max_fall_;
    } else if (type == MetadataType::HDR_MDCV) {
      // Create arrays for primary chromaticities
      nlohmann::json primaries = nlohmann::json::array();
      for (int i = 0; i < 3; i++) {
        primaries.push_back({
          {"x", primary_chromaticity_x_[i]},
          {"y", primary_chromaticity_y_[i]}
        });
      }
      j["primary_chromaticities"] = primaries;
      j["white_point"] = {
        {"x", white_point_chromaticity_x_},
        {"y", white_point_chromaticity_y_}
      };
      j["luminance_max"] = luminance_max_;
      j["luminance_min"] = luminance_min_;
    } else if (type == MetadataType::ITUT_T35) {
      j["itu_t_t35_country_code"] = itu_t_t35_country_code_;
      if (itu_t_t35_country_code_ == 0xFF) {
        j["itu_t_t35_country_code_extension_byte"] = itu_t_t35_country_code_extension_byte_;
      }
      if (itu_t_t35_terminal_provider_code_ != 0) {
        j["itu_t_t35_terminal_provider_code"] = itu_t_t35_terminal_provider_code_;
      }
      if (!itu_t_t35_payload_bytes_.empty()) {
        // Format payload as hex string for readability
        // Truncate large payloads to keep JSON manageable
        const size_t MAX_PREVIEW_BYTES = 64;
        std::string hex_payload;
        char buf[3];

        size_t bytes_to_show = std::min(itu_t_t35_payload_bytes_.size(), MAX_PREVIEW_BYTES);
        for (size_t i = 0; i < bytes_to_show; i++) {
          snprintf(buf, sizeof(buf), "%02x", itu_t_t35_payload_bytes_[i]);
          hex_payload += buf;
        }

        if (itu_t_t35_payload_bytes_.size() > MAX_PREVIEW_BYTES) {
          hex_payload += "... (truncated)";
        }

        j["itu_t_t35_payload_bytes"] = hex_payload;
        j["itu_t_t35_payload_size"] = itu_t_t35_payload_bytes_.size();
      }
    } else if (type == MetadataType::TIMECODE) {
      j["counting_type"] = counting_type_;
      j["full_timestamp_flag"] = full_timestamp_flag_;
      j["discontinuity_flag"] = discontinuity_flag_;
      j["cnt_dropped_flag"] = cnt_dropped_flag_;
      j["n_frames"] = n_frames_;
      if (full_timestamp_flag_ || seconds_flag_) {
        j["seconds_value"] = seconds_value_;
      }
      if (full_timestamp_flag_ || minutes_flag_) {
        j["minutes_value"] = minutes_value_;
      }
      if (full_timestamp_flag_ || hours_flag_) {
        j["hours_value"] = hours_value_;
      }
      if (!full_timestamp_flag_) {
        j["seconds_flag"] = seconds_flag_;
        if (seconds_flag_) {
          j["minutes_flag"] = minutes_flag_;
          if (minutes_flag_) {
            j["hours_flag"] = hours_flag_;
          }
        }
      }
      if (time_offset_length_ > 0) {
        j["time_offset_length"] = time_offset_length_;
        j["time_offset_value"] = time_offset_value_;
      }
    } else if (type == MetadataType::HASH) {
      j["hash_type"] = hash_type_;
      j["per_plane"] = per_plane_;
      j["has_grain"] = has_grain_;
      if (hash_reserved_ != 0) {
        j["reserved"] = hash_reserved_;
      }

      // Format hashes as hex strings with appropriate field name
      nlohmann::json hash_array = nlohmann::json::array();
      for (const auto& hash : hashes_) {
        char hex_string[33];  // 32 hex chars + null terminator
        snprintf(hex_string, sizeof(hex_string),
                 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
                 hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
        hash_array.push_back(std::string(hex_string));
      }

      // Use appropriate field name based on per_plane flag
      if (per_plane_) {
        j["plane_hashes"] = hash_array;
      } else {
        // Single frame hash - export as single value, not array
        if (!hash_array.empty()) {
          j["frame_hash"] = hash_array[0];
        }
      }
    }
  }

  if (!muh_header_extension_bytes_.empty()) {
    j["muh_header_extension_bytes"] = muh_header_extension_bytes_;
  }

  return j;
}

}  // namespace av2_obu
