/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>

namespace WEBRTC
{

/*!
 * \brief Removes the TCP candidates from a session description.
 */
std::string RemoveTcpCandidates(const std::string& sdp);

} // namespace WEBRTC
