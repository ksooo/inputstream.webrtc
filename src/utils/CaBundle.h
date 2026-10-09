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

namespace WEBRTC
{

/*!
 * \brief Finds the file with the trusted root certificates: the one Kodi announces in
 *        SSL_CERT_FILE, otherwise the bundle of the Linux distribution.
 */
std::optional<std::string> FindCaBundle();

std::optional<std::string> FindCaBundle(const char* sslCertFile,
                                        const std::vector<std::string>& systemBundles);

} // namespace WEBRTC
