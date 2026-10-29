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
#include <optional>
#include <string>

namespace av2_obu {

class OBUParser;
class TemporalUnit;

// CICP color metadata for the ISOBMFF 'colr' / nclx box.
struct ColrInfo {
  uint32_t colour_primaries;
  uint32_t transfer_characteristics;
  uint32_t matrix_coefficients;
  uint32_t full_range_flag;  // 0 or 1
};

// Search order: CI OBU > LCR (lowest xlayer with color info) > OPS.
// Returns nullopt if no source carries color metadata.
std::optional<ColrInfo> extract_colr_info(const OBUParser& parser);

// Same search order as extract_colr_info(), scoped to a single TU's OBUs.
// Used to pick colr for a new sample entry created at a CVS boundary.
std::optional<ColrInfo> extract_colr_info_from_tu(const TemporalUnit& tu);

// Resolves a CICP profile name (e.g. "bt709") to its ColrInfo. nullopt for unknown names
std::optional<ColrInfo> colr_profile_by_name(const std::string& name);

// Comma-separated list of profile names recognised by colr_profile_by_name().
std::string supported_colr_profile_names();

}  // namespace av2_obu
