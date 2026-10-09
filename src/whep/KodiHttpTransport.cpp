/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "KodiHttpTransport.h"

#include "utils/Base64.h"
#include "utils/Log.h"

#include <cstdlib>

#include <kodi/Filesystem.h>

namespace WEBRTC
{

namespace
{

// The status code of a response line such as "HTTP/1.1 201 Created"
int ParseStatus(const std::string& responseLine)
{
  const size_t space = responseLine.find(' ');
  if (space == std::string::npos)
    return 0;
  return std::atoi(responseLine.c_str() + space + 1);
}

} // namespace

std::optional<HttpResponse> CKodiHttpTransport::Send(const HttpRequest& request)
{
  kodi::vfs::CFile file;
  if (!file.CURLCreate(request.url))
    return {};

  file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "failonerror", "false");
  file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "seekable", "0");
  if (request.method == "POST")
    file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "postdata", Base64Encode(request.body));
  else if (request.method != "GET")
    file.CURLAddOption(ADDON_CURL_OPTION_PROTOCOL, "customrequest", request.method);
  for (const auto& [name, value] : request.headers)
    file.CURLAddOption(ADDON_CURL_OPTION_HEADER, name, value);

  if (!file.CURLOpen(ADDON_READ_NO_CACHE))
  {
    Log(LogLevel::LEVEL_ERROR, "No response to the %s request", request.method.c_str());
    return {};
  }

  HttpResponse response;
  response.status = ParseStatus(file.GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_PROTOCOL, ""));
  response.effectiveUrl = file.GetPropertyValue(ADDON_FILE_PROPERTY_EFFECTIVE_URL, "");
  response.location = file.GetPropertyValue(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "location");
  response.links = file.GetPropertyValues(ADDON_FILE_PROPERTY_RESPONSE_HEADER, "link");

  char buffer[4096];
  ssize_t read;
  while ((read = file.Read(buffer, sizeof(buffer))) > 0)
    response.body.append(buffer, static_cast<size_t>(read));

  return response;
}

} // namespace WEBRTC
