/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "IceServers.h"

#include "utils/Log.h"

#include <exception>

namespace WEBRTC
{

std::optional<rtc::IceServer> ParseIceServer(const std::string& url)
{
  try
  {
    return rtc::IceServer(url);
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_WARNING, "Ignoring ICE server '%s': %s", url.c_str(), e.what());
    return {};
  }
}

std::vector<rtc::IceServer> ParseIceServers(const std::vector<std::string>& urls)
{
  std::vector<rtc::IceServer> servers;
  for (const auto& url : urls)
  {
    if (auto server = ParseIceServer(url))
      servers.emplace_back(std::move(*server));
  }
  return servers;
}

} // namespace WEBRTC
