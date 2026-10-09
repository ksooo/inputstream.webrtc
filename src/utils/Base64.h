/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <string_view>

namespace WEBRTC
{

std::string Base64Encode(std::string_view data);

} // namespace WEBRTC
