/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "StringUtils.h"

#include <algorithm>
#include <cctype>

namespace WEBRTC
{

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string Trim(const std::string& value)
{
  const size_t first = value.find_first_not_of(" \t");
  if (first == std::string::npos)
    return {};
  const size_t last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> Split(const std::string& value, char separator)
{
  std::vector<std::string> items;
  size_t start = 0;
  while (start <= value.size())
  {
    size_t end = value.find(separator, start);
    if (end == std::string::npos)
      end = value.size();
    std::string item = Trim(value.substr(start, end - start));
    if (!item.empty())
      items.emplace_back(std::move(item));
    start = end + 1;
  }
  return items;
}

} // namespace WEBRTC
