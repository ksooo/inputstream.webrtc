/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Base64.h"

#include <cstdint>
#include <cstring>

namespace WEBRTC
{

namespace
{

constexpr const char* ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string Base64Encode(std::string_view data)
{
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

std::optional<std::string> Base64Decode(std::string_view text)
{
  while (!text.empty() && text.back() == '=')
    text.remove_suffix(1);

  std::string result;
  uint32_t block = 0;
  int bits = 0;
  for (const char c : text)
  {
    const char* position = std::strchr(ALPHABET, c);
    if (c == '\0' || !position)
      return {};
    block = (block << 6) | static_cast<uint32_t>(position - ALPHABET);
    bits += 6;
    if (bits >= 8)
    {
      bits -= 8;
      result += static_cast<char>((block >> bits) & 0xff);
    }
  }
  return result;
}

} // namespace WEBRTC
