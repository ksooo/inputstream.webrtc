/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "StreamProperties.h"

#include "utils/Log.h"
#include "utils/StringUtils.h"

namespace WEBRTC
{

namespace
{

constexpr const char* PROPERTY_PREFIX = "inputstream.webrtc.";

std::optional<std::string> GetProperty(const std::map<std::string, std::string>& properties,
                                       const std::string& name)
{
  const auto it = properties.find(PROPERTY_PREFIX + name);
  if (it == properties.end())
    return {};
  return it->second;
}

} // namespace

std::optional<StreamProperties> ParseStreamProperties(
    const std::map<std::string, std::string>& properties)
{
  StreamProperties result;

  if (const auto signaling = GetProperty(properties, "signaling"))
  {
    if (*signaling == "whep")
      result.signaling = SignalingType::WHEP;
    else if (*signaling == "homeassistant")
      result.signaling = SignalingType::HOME_ASSISTANT;
    else
    {
      Log(LogLevel::LEVEL_ERROR, "Unknown signaling '%s'", signaling->c_str());
      return {};
    }
  }

  if (const auto audio = GetProperty(properties, "audio"))
  {
    if (*audio == "true")
      result.audio = true;
    else if (*audio == "false")
      result.audio = false;
    else
    {
      Log(LogLevel::LEVEL_ERROR, "Invalid audio value '%s', expected true or false",
          audio->c_str());
      return {};
    }
  }

  if (const auto iceServers = GetProperty(properties, "ice_servers"))
    result.iceServers = Split(*iceServers, ',');

  result.entityId = GetProperty(properties, "entity_id").value_or("");
  result.bearerToken = GetProperty(properties, "bearer_token").value_or("");

  if (result.signaling == SignalingType::HOME_ASSISTANT)
  {
    if (result.entityId.empty())
    {
      Log(LogLevel::LEVEL_ERROR, "Home Assistant needs the entity_id of the camera");
      return {};
    }
    if (result.bearerToken.empty())
    {
      Log(LogLevel::LEVEL_ERROR, "Home Assistant needs an access token in bearer_token");
      return {};
    }
  }

  return result;
}

} // namespace WEBRTC
