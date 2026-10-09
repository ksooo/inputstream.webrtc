/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Base64.h"

#include <cstdint>

namespace WEBRTC
{

std::string Base64Encode(std::string_view data)
{
  constexpr const char* ALPHABET =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string result;
  result.reserve((data.size() + 2) / 3 * 4);

  size_t i = 0;
  for (; i + 2 < data.size(); i += 3)
  {
    const uint32_t block = (static_cast<uint8_t>(data[i]) << 16) |
                           (static_cast<uint8_t>(data[i + 1]) << 8) |
                           static_cast<uint8_t>(data[i + 2]);
    result += ALPHABET[(block >> 18) & 0x3f];
    result += ALPHABET[(block >> 12) & 0x3f];
    result += ALPHABET[(block >> 6) & 0x3f];
    result += ALPHABET[block & 0x3f];
  }

  if (i + 1 == data.size())
  {
    const uint32_t block = static_cast<uint8_t>(data[i]) << 16;
    result += ALPHABET[(block >> 18) & 0x3f];
    result += ALPHABET[(block >> 12) & 0x3f];
    result += "==";
  }
  else if (i + 2 == data.size())
  {
    const uint32_t block =
        (static_cast<uint8_t>(data[i]) << 16) | (static_cast<uint8_t>(data[i + 1]) << 8);
    result += ALPHABET[(block >> 18) & 0x3f];
    result += ALPHABET[(block >> 12) & 0x3f];
    result += ALPHABET[(block >> 6) & 0x3f];
    result += '=';
  }

  return result;
}

} // namespace WEBRTC
