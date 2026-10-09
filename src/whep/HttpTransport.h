/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace WEBRTC
{

struct HttpRequest
{
  std::string method;
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

struct HttpResponse
{
  int status{0};
  std::string effectiveUrl; // after redirects
  std::string location;
  std::vector<std::string> links;
  std::string body;
};

class IHttpTransport
{
public:
  virtual ~IHttpTransport() = default;

  /*!
   * \return The response, also for error statuses; nothing if there is none.
   */
  virtual std::optional<HttpResponse> Send(const HttpRequest& request) = 0;
};

} // namespace WEBRTC
