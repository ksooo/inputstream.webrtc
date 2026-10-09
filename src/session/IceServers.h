/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <rtc/configuration.hpp>

namespace WEBRTC
{

/*!
 * \brief Parses an ICE server URL such as stun:host:3478 or turn:user:password@host:3478.
 * \return The server, or nothing if the URL is invalid.
 */
std::optional<rtc::IceServer> ParseIceServer(const std::string& url);

/*!
 * \brief Parses ICE server URLs, skipping invalid ones.
 */
std::vector<rtc::IceServer> ParseIceServers(const std::vector<std::string>& urls);

} // namespace WEBRTC
