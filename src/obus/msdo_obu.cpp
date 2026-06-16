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

#include <av2_obu/core/bitstream_reader.h>
#include <av2_obu/obus/msdo_obu.h>

namespace av2_obu {

bool MSDOOBU::parse_payload(std::ifstream& ifs) {
  spdlog::debug("Parsing MSDO payload ({} bytes)", position_.payload_size);

  raw_payload_.resize(position_.payload_size);
  if (!ifs.read(reinterpret_cast<char*>(raw_payload_.data()), position_.payload_size)) {
    spdlog::error("Failed to read MSDO payload");
    return false;
  }

  BitstreamReader br(raw_payload_);

  // multistream_decoder_operation_obu()
  uint32_t num_streams_minus_2 = br.read_bits(3);
  num_streams_ = num_streams_minus_2 + 2;
  multistream_profile_idc_ = br.read_bits(5);
  multistream_level_idx_ = br.read_bits(5);
  multistream_tier_ = br.read_bit();
  multistream_even_allocation_flag_ = br.read_bit();
  if (!multistream_even_allocation_flag_) {
    multistream_large_picture_idc_ = br.read_bits(3);
  }

  streams_.resize(num_streams_);
  for (uint32_t i = 0; i < num_streams_; i++) {
    streams_[i].sub_xlayer_id = br.read_bits(5);
    streams_[i].sub_stream_max_profile = br.read_bits(5);
    streams_[i].sub_stream_max_level = br.read_bits(5);
    streams_[i].sub_stream_max_tier = br.read_bit();
  }

  multistream_doh_constraint_flag_ = br.read_bit();

  // Parse trailing_bits (non-extensible OBU)
  if (!parse_obu_trailing_bits(br))
    return false;

  spdlog::debug("MSDO: {} streams", num_streams_);
  return true;
}

json MSDOOBU::to_json() const {
  json j = BaseOBU::to_json();
  j["type_name"] = "MSDO";
  if (parsed_) {
    j["num_streams"] = num_streams_;
    j["multistream_profile_idc"] = multistream_profile_idc_;
    j["multistream_level_idx"] = multistream_level_idx_;
    j["multistream_tier"] = multistream_tier_;
    j["multistream_even_allocation_flag"] = multistream_even_allocation_flag_;
    if (!multistream_even_allocation_flag_) {
      j["multistream_large_picture_idc"] = multistream_large_picture_idc_;
    }
    j["multistream_doh_constraint_flag"] = multistream_doh_constraint_flag_;
    json streams_json = json::array();
    for (const auto& s : streams_) {
      streams_json.push_back({{"sub_xlayer_id", s.sub_xlayer_id},
                              {"sub_stream_max_profile", s.sub_stream_max_profile},
                              {"sub_stream_max_level", s.sub_stream_max_level},
                              {"sub_stream_max_tier", s.sub_stream_max_tier}});
    }
    j["streams"] = streams_json;
  }
  return j;
}

}  // namespace av2_obu
