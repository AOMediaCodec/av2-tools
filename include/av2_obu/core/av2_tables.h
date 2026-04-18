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

#include <av2_obu/core/av2_types.h>

// AV2 Spec lookup tables (from spec attachments).
// Only array data — enums are in av2_types.h, helpers in av2_math.h.

namespace av2_obu {

// Num_4x4_Blocks_Wide[BLOCK_SIZES] (spec attachment)
constexpr uint32_t Num_4x4_Blocks_Wide[BLOCK_SIZES] = {
    1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 64, 64, 1, 4, 2, 8, 4, 16, 1, 8, 2,
    16};

// Mi_Width_Log2[BLOCK_SIZES] (spec attachment)
constexpr uint32_t Mi_Width_Log2[BLOCK_SIZES] = {
    0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 0, 2, 1, 3, 2, 4, 0, 3, 1, 4};

// Tile_Width_Scaling_Factor[tier][level] (spec attachment)
// Index: [seq_tier][seq_level_idx], reserved levels use 4
constexpr uint32_t Tile_Width_Scaling_Factor[2][31] = {
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 4, 4, 4, 4, 4, 4, 4, 4,
     4},
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 4, 4, 4, 4, 4, 4, 4,
     4, 4}};

// Tile_Area_Scaling_Factor[tier][level] (spec attachment)
constexpr uint32_t Tile_Area_Scaling_Factor[2][31] = {
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16, 4, 4, 4, 4, 4, 4, 4,
     4, 4},
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 16, 16, 16, 16, 32, 32, 32, 32, 4, 4, 4, 4, 4, 4,
     4, 4, 4}};

}  // namespace av2_obu
