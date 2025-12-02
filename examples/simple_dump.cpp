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

// Simple example: Dump OBU information from an AV2 bitstream file
// Demonstrates basic usage of the av2_obu library

#include <iostream>

#include <av2_obu/av2_obu.h>

using namespace av2_obu;

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <bitstream.bin>" << std::endl;
    return 1;
  }

  const char* filename = argv[1];

  // Create parser instance
  OBUParser parser;

  // Parse the file
  if (!parser.parse_file(filename)) {
    std::cerr << "Failed to parse file: " << filename << std::endl;
    return 1;
  }

  // Print summary
  std::cout << "Successfully parsed " << parser.obu_count() << " OBUs" << std::endl;

  // Iterate through OBUs
  const auto& obus = parser.obus();
  for (size_t i = 0; i < obus.size(); ++i) {
    const auto& obu = obus[i];
    std::cout << "OBU #" << i << ": Type=" << obu->type_name()
              << ", Size=" << obu->position().payload_size << " bytes" << std::endl;
  }

  // Optionally convert to JSON
  auto json_output = parser.to_json();
  std::cout << "\nJSON output:" << std::endl;
  std::cout << json_output.dump(2) << std::endl;

  return 0;
}
