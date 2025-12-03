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
#include <memory>
#include <string>
#include <vector>

#include <av2_obu/core/av2_types.h>
#include <av2_obu/core/obu_parser.h>

using av2_obu::OBUParser;
using av2_obu::OBUType;

class BitstreamSwitcher {
public:
  struct SwitchPoint {
    double timestamp_sec;  // desired time
    OBUType on_obu_type;   // shall-match OBU type
  };

  explicit BitstreamSwitcher(double fps);

  /// @brief Load N streams; all streams share the same FPS.
  /// @param names are paths; returns false if any init fails.
  /// @return true on success.
  bool attach_streams(const std::vector<std::string>& names);

  /// @brief Produce output by stitching A->B->C... using switch points.
  /// @param stream_order specifies which stream to use at each segment ( = switch_points + 1).
  /// @param switch_points specify when to switch and on which OBU type.
  /// @param output_path is the output file path.
  /// @note Assumes stream alignment
  /// @return true on success.
  bool stitch(const std::vector<size_t>& stream_order,
              const std::vector<SwitchPoint>& switch_points, const std::string& output_path);

private:
  struct Stream {
    std::string path;
    std::unique_ptr<OBUParser> parser;
  };

  /// @brief Find the OBU index nearest to timestamp on the specified type.
  /// @param s stream to search
  /// @param timestamp_sec target time in seconds
  /// @param type target OBU type to match
  /// @return index of the matching OBU
  size_t find_switch_index(const Stream& s, double timestamp_sec, OBUType type) const;

  /// @brief Copy byte ranges [from_index_begin, from_index_end) from `from` into `ofs`.
  /// @param from source stream
  /// @param from_index_begin starting OBU index (inclusive)
  /// @param from_index_end ending OBU index (exclusive)
  /// @param ofs output file stream
  /// @return true on success.
  bool copy_range(const Stream& from, size_t from_index_begin, size_t from_index_end,
                  std::ofstream& ofs) const;

  double fps_;
  std::vector<Stream> streams_;
};
