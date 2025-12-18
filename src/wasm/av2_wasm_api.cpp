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

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <av2_obu/av2_obu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace av2_obu;

namespace {
// Thread-local error message storage
thread_local std::string g_last_error;

// Set error message and return nullptr
const char* set_error(const std::string& msg) {
  g_last_error = msg;
  return nullptr;
}
}  // namespace

extern "C" {

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* av2_parse_to_json(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    return set_error("Invalid input: null data or zero size");
  }

  try {
    // Write data to temporary file in Emscripten's virtual filesystem
    // The temp file approach works fine for browser use cases
    const char* temp_path = "/tmp/av2_wasm_temp.bin";
    std::ofstream ofs(temp_path, std::ios::binary);
    if (!ofs) {
      return set_error("Failed to create temporary file");
    }
    ofs.write(reinterpret_cast<const char*>(data), size);
    ofs.close();

    // Parse bitstream
    OBUParser parser;
    if (!parser.parse_file(temp_path)) {
      return set_error("Failed to parse AV2 bitstream");
    }

    // Generate JSON
    auto json_obj = parser.to_json();
    std::string json_str = json_obj.dump();

    // Allocate C string (caller must free with av2_free_json)
    char* result = static_cast<char*>(malloc(json_str.size() + 1));
    if (!result) {
      return set_error("Memory allocation failed");
    }
    std::memcpy(result, json_str.c_str(), json_str.size() + 1);

    return result;
  } catch (const std::exception& e) {
    return set_error(std::string("Exception: ") + e.what());
  } catch (...) {
    return set_error("Unknown exception during parsing");
  }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void av2_free_json(const char* json_str) {
  if (json_str) {
    free(const_cast<char*>(json_str));
  }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* av2_get_last_error() {
  return g_last_error.empty() ? "No error" : g_last_error.c_str();
}

}  // extern "C"
