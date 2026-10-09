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

/*!
 * \brief Returns and forgets the messages logged so far.
 */
std::vector<std::string> TakeLogMessages();

} // namespace WEBRTC
