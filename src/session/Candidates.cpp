/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Candidates.h"

#include "utils/StringUtils.h"

#include <sstream>

namespace WEBRTC
{

namespace
{

bool IsTcpCandidate(const std::string& line)
{
  if (!line.starts_with("a=candidate:"))
    return false;

  // a=candidate:<foundation> <component> <transport> ...
  std::istringstream fields(line);
  std::string foundation;
  std::string component;
  std::string transport;
  fields >> foundation >> component >> transport;
  return ToLower(transport) == "tcp";
}

} // namespace

std::string RemoveTcpCandidates(const std::string& sdp)
{
  std::string result;
  size_t start = 0;
  while (start < sdp.size())
  {
    size_t end = sdp.find('\n', start);
    end = end == std::string::npos ? sdp.size() : end + 1;
    const std::string line = sdp.substr(start, end - start);
    if (!IsTcpCandidate(line))
      result += line;
    start = end;
  }
  return result;
}

} // namespace WEBRTC
