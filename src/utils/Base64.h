/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace WEBRTC
{

std::string Base64Encode(std::string_view data);

/*!
 * \brief Decodes standard Base64 with or without padding.
 * \return The data, or nothing if the input contains other characters.
 */
std::optional<std::string> Base64Decode(std::string_view text);

} // namespace WEBRTC
