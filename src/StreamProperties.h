/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace WEBRTC
{

enum class SignalingType
{
  WHEP,
  HOME_ASSISTANT
};

struct StreamProperties
{
  SignalingType signaling{SignalingType::WHEP};
  std::string entityId;
  std::string bearerToken;
  std::vector<std::string> iceServers;
  bool audio{true};
};

/*!
 * \brief Reads the inputstream.webrtc.* list item properties.
 * \return The properties, or nothing if a value is invalid or a required one is missing.
 */
std::optional<StreamProperties> ParseStreamProperties(
    const std::map<std::string, std::string>& properties);

} // namespace WEBRTC
