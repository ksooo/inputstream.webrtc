/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <utility>

namespace WEBRTC
{

/*!
 * \brief Splits Kodi's protocol options, e.g. "|User-Agent=Kodi", from a URL.
 * \return The URL and the options including the leading '|', or an empty string.
 */
std::pair<std::string, std::string> SplitOptions(const std::string& url);

/*!
 * \brief Resolves a URL reference, e.g. from a Location header, against a base URL (RFC 3986).
 */
std::string ResolveUrl(const std::string& base, const std::string& reference);

/*!
 * \brief Removes the user name and password from a URL.
 */
std::string RemoveUserInfo(const std::string& url);

} // namespace WEBRTC
