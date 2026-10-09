/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "HttpTransport.h"

namespace WEBRTC
{

/*!
 * \brief HTTP through Kodi's curl, which also applies the protocol options of the URL.
 */
class CKodiHttpTransport : public IHttpTransport
{
public:
  std::optional<HttpResponse> Send(const HttpRequest& request) override;
};

} // namespace WEBRTC
