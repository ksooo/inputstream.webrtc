/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <vector>

namespace WEBRTC
{

std::string ToLower(std::string value);

/*!
 * \brief Removes leading and trailing spaces and tabs.
 */
std::string Trim(const std::string& value);

/*!
 * \brief Splits a list into its trimmed, non-empty items.
 */
std::vector<std::string> Split(const std::string& value, char separator);

} // namespace WEBRTC
