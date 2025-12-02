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
#include <spdlog/spdlog.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>

namespace av2_obu {

// ========== LOGGING CONTROL ==========

// Set the log level for the av2 library
// Default is warn (only warnings and errors)
// CLI tools should set to info or debug as needed
void set_log_level(spdlog::level::level_enum level);
spdlog::level::level_enum get_log_level();

// ========== ENUMS ==========

// OBU Types - Latest AV2 specification
// Enum values match AVM with experimental macros enabled
enum class OBUType : uint32_t {
  UNKNOWN = 0,  // For invalid/unknown types
  SEQUENCE_HEADER = 1,
  TEMPORAL_DELIMITER = 2,
  MULTI_FRAME_HEADER = 3,  // CONFIG_MULTI_FRAME_HEADER
  CLK = 4,                 // CONFIG_F024_KEYOBU - Complete Layer Key
  OLK = 5,                 // CONFIG_F024_KEYOBU - Output Layer Key
  LEADING_TILE_GROUP = 6,  // CONFIG_F024_KEYOBU
  REGULAR_TILE_GROUP = 7,  // CONFIG_F024_KEYOBU
  METADATA = 8,
  METADATA_GROUP = 9,  // CONFIG_SHORT_METADATA
  SWITCH = 10,         // CONFIG_F106_OBU_SWITCH
  LEADING_SEF = 11,    // CONFIG_F106_OBU_SEF
  REGULAR_SEF = 12,
  LEADING_TIP = 13,  // CONFIG_F106_OBU_TIP
  REGULAR_TIP = 14,
  BUFFER_REMOVAL_TIMING = 15,       // CONFIG_CWG_F293_BUFFER_REMOVAL_TIMING
  LAYER_CONFIGURATION_RECORD = 16,  // CONFIG_MULTILAYER_HLS
  ATLAS_SEGMENT = 17,
  OPERATING_POINT_SET = 18,
  BRIDGE_FRAME = 19,            // CONFIG_CWG_F317
  MSDO = 20,                    // CONFIG_MULTI_STREAM
  RAS_FRAME = 21,               // CONFIG_RANDOM_ACCESS_SWITCH_FRAME
  QM = 22,                      // CONFIG_F255_QMOBU
  FGM = 23,                     // CONFIG_F153_FGM_OBU
  CONTENT_INTERPRETATION = 24,  // CONFIG_CWG_F270_CI_OBU
  PADDING = 25
};

// Metadata Types
enum class MetadataType : uint32_t {
  RESERVED = 0,
  HDR_CLL = 1,      // METADATA_TYPE_HDR_CLL
  HDR_MDCV = 2,     // METADATA_TYPE_HDR_MDCV
  SCALABILITY = 3,  // METADATA_TYPE_SCALABILITY
  ITUT_T35 = 4,     // METADATA_TYPE_ITUT_T35
  TIMECODE = 5,     // METADATA_TYPE_TIMECODE
  HASH = 6,         // METADATA_HASH
  BANDING_HINTS = 7,
  ICC_PROFILE = 8,
  SCAN_TYPE = 9,
  UNKNOWN_METADATA = 999  // For invalid/unknown metadata types
};

// Color primaries
enum ColorPrimaries : uint32_t {
  CP_BT_709 = 1,        // [ITU-R-BT.709]
  CP_UNSPECIFIED = 2,   // Unspecified
  CP_BT_470_M = 4,      // BT.470 System M (historical)
  CP_BT_470_B_G = 5,    // BT.470 System B, G (historical)
  CP_BT_601 = 6,        // [ITU-R-BT.601]
  CP_SMPTE_240 = 7,     // SMPTE 240
  CP_GENERIC_FILM = 8,  // Generic film (color filters using illuminant C)
  CP_BT_2020 = 9,       // BT.2020, BT.2100
  CP_XYZ = 10,          // SMPTE 428 (CIE 1921 XYZ)
  CP_SMPTE_431 = 11,    // SMPTE RP 431-2
  CP_SMPTE_432 = 12,    // SMPTE EG 432-1
  CP_EBU_3213 = 22      // EBU Tech. 3213-E
};

// Transfer characteristics
enum TransferCharacteristics : uint32_t {
  TC_RESERVED_0 = 0,       // For future use
  TC_BT_709 = 1,           // [ITU-R-BT.709]
  TC_UNSPECIFIED = 2,      // Unspecified
  TC_RESERVED_3 = 3,       // For future use
  TC_BT_470_M = 4,         // BT.470 System M (historical)
  TC_BT_470_B_G = 5,       // BT.470 System B, G (historical)
  TC_BT_601 = 6,           // [ITU-R-BT.601]
  TC_SMPTE_240 = 7,        // SMPTE 240 M
  TC_LINEAR = 8,           // Linear
  TC_LOG_100 = 9,          // Logarithmic (100 : 1 range)
  TC_LOG_100_SQRT10 = 10,  // Logarithmic (100 * Sqrt(10) : 1 range)
  TC_IEC_61966 = 11,       // IEC 61966-2-4
  TC_BT_1361 = 12,         // BT.1361
  TC_SRGB = 13,            // sRGB or sYCC
  TC_BT_2020_10_BIT = 14,  // BT.2020 10-bit systems [Rec.2020]
  TC_BT_2020_12_BIT = 15,  // BT.2020 12-bit systems [Rec.2020]
  TC_SMPTE_2084 = 16,      // SMPTE ST 2084, ITU BT.2100 PQ
  TC_SMPTE_428 = 17,       // SMPTE ST 428
  TC_HLG = 18              // BT.2100 HLG, ARIB STD-B67
};

// Matrix coefficients
enum MatrixCoefficients : uint32_t {
  MC_IDENTITY = 0,      // Identity matrix
  MC_BT_709 = 1,        // [ITU-R-BT.709]
  MC_UNSPECIFIED = 2,   // Unspecified
  MC_RESERVED_3 = 3,    // For future use
  MC_FCC = 4,           // US FCC 73.628
  MC_BT_470_B_G = 5,    // BT.470 System B, G (historical)
  MC_BT_601 = 6,        // [ITU-R-BT.601]
  MC_SMPTE_240 = 7,     // SMPTE 240 M
  MC_SMPTE_YCGCO = 8,   // YCgCo
  MC_BT_2020_NCL = 9,   // BT.2020 non-constant luminance, BT.2100 YCbCr
  MC_BT_2020_CL = 10,   // BT.2020 constant luminance [Rec.2020]
  MC_SMPTE_2085 = 11,   // SMPTE ST 2085 YDzDx
  MC_CHROMAT_NCL = 12,  // Chromaticity-derived non-constant luminance
  MC_CHROMAT_CL = 13,   // Chromaticity-derived constant luminance
  MC_ICTCP = 14         // BT.2100 ICtCp
};

// Chroma sample position
enum ChromaSamplePosition : uint32_t {
  CSP_LEFT = 0,        // 4:2:2: H-offset 0; 4:2:0: H-offset 0, V-offset 0.5
  CSP_CENTER = 1,      // 4:2:2: H-offset 0.5; 4:2:0: H-offset 0.5, V-offset 0.5
  CSP_TOPLEFT = 2,     // 4:2:0 only: H-offset 0, V-offset 0
  CSP_TOP = 3,         // 4:2:0 only: H-offset 0.5, V-offset 0
  CSP_BOTTOMLEFT = 4,  // 4:2:0 only: H-offset 0, V-offset 1
  CSP_BOTTOM = 5,      // 4:2:0 only: H-offset 0.5, V-offset 1
  CSP_UNSPECIFIED = 6  // Unknown or determined by the application
};

// Frame restoration type
enum FrameRestorationType : uint32_t {
  RESTORE_NONE = 0,
  RESTORE_PC_WIENER = 1,
  RESTORE_WIENER_NONSEP = 2,
  RESTORE_SWITCHABLE = 3
};

// OPFL refine modes (enable_opfl_refine)
enum OpflRefineType : uint32_t {
  REFINE_NONE = 0,
  REFINE_SWITCHABLE = 1,
  REFINE_ALL = 2,
  REFINE_AUTO = 3
};

// Motion mode types
enum MotionMode : uint32_t { INTERINTRA = 0, OBMC = 1, WARP = 2, DELTAWARP = 3, WARP_EXTEND = 4 };

// DRL reorder modes
enum DrlReorder : uint32_t {
  DRL_REORDER_DISABLED = 0,
  DRL_REORDER_CONSTRAINT = 1,
  DRL_REORDER_ALWAYS = 2
};

// ========== HELPER FUNCTIONS ==========

std::string to_string(OBUType type);
OBUType to_obu_type(uint32_t value);
std::ostream& operator<<(std::ostream& os, OBUType type);
bool is_valid_obu_type(uint32_t value);

std::string to_string(MetadataType type);
MetadataType to_metadata_type(uint32_t value);
std::ostream& operator<<(std::ostream& os, MetadataType type);
bool is_valid_metadata_type(uint32_t value);

// ========== CONSTANTS ==========

// Motion modes
constexpr uint32_t MOTION_MODES = 5;

// Delta DC quantization
constexpr uint32_t DELTA_DCQUANT_BITS = 5;
constexpr int32_t DELTA_DCQUANT_MAX = (1 << (DELTA_DCQUANT_BITS - 2));
constexpr int32_t DELTA_DCQUANT_MIN = (DELTA_DCQUANT_MAX - (1 << DELTA_DCQUANT_BITS) + 1);

// Temporal and multilayer
constexpr uint32_t MAX_NUM_TLAYERS = 4;  // Maximum number of temporal layers
constexpr uint32_t MAX_NUM_MLAYERS = 8;  // Maximum number of embedded layers

// Reference frames
constexpr uint32_t REFS_PER_FRAME = 7;         // Number of reference frames for Inter prediction
constexpr uint32_t MAX_REF_MV_STACK_SIZE = 6;  // Maximum number of motion vectors in the stack
constexpr uint32_t MAX_REF_BV_STACK_SIZE = 4;  // Maximum number of block vectors

// Chroma format
constexpr uint32_t CHROMA_FORMAT_420 = 0;
constexpr uint32_t CHROMA_FORMAT_400 = 1;  // Monochrome
constexpr uint32_t CHROMA_FORMAT_444 = 2;
constexpr uint32_t CHROMA_FORMAT_422 = 3;

// Quantizer limits (per AV2 spec)
constexpr uint32_t MAXQ_8_BITS = 255;  // Maximum quantizer when bit depth is 8
constexpr uint32_t MAXQ_OFFSET = 24;   // Increase in allowed quantizer
constexpr uint32_t MAXQ_10_BITS = MAXQ_8_BITS + 2 * MAXQ_OFFSET;  // Max quantizer (10 bit)
constexpr uint32_t MAXQ_12_BITS = MAXQ_8_BITS + 4 * MAXQ_OFFSET;  // Max quantizer (12 bit)

// CDEF on skip transform modes
constexpr uint32_t CDEF_ON_SKIP_TXFM_DISABLED = 0;
constexpr uint32_t CDEF_ON_SKIP_TXFM_ALWAYS_ON = 1;
constexpr uint32_t CDEF_ON_SKIP_TXFM_ADAPTIVE = 2;

// Screen content tools
constexpr uint32_t SELECT_SCREEN_CONTENT_TOOLS = 2;
constexpr uint32_t SELECT_INTEGER_MV = 2;

// Custom QM
constexpr uint32_t NUM_CUSTOM_QMS = 15;

}  // namespace av2_obu
