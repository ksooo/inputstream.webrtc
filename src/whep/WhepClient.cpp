/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "WhepClient.h"

#include "HttpTransport.h"
#include "utils/Log.h"
#include "utils/Url.h"

#include <utility>

namespace WEBRTC
{

namespace
{

constexpr int HTTP_CREATED = 201;
constexpr size_t MAX_LOGGED_BODY = 200;

} // namespace

CWhepClient::CWhepClient(IHttpTransport& transport, std::string endpoint, std::string bearerToken)
  : m_transport(transport),
    m_endpoint(std::move(endpoint)),
    m_bearerToken(std::move(bearerToken))
{
}

CWhepClient::~CWhepClient()
{
  Close();
}

void CWhepClient::AddAuthorization(HttpRequest& request) const
{
  if (!m_bearerToken.empty())
    request.headers.emplace_back("Authorization", "Bearer " + m_bearerToken);
}

std::optional<std::string> CWhepClient::Offer(const std::string& sdp, CandidateCallback onCandidate)
{
  HttpRequest request{"POST", m_endpoint, {{"Content-Type", "application/sdp"}}, sdp};
  AddAuthorization(request);

  const auto response = m_transport.Send(request);
  if (!response)
    return {};

  if (response->status != HTTP_CREATED)
  {
    Log(LogLevel::LEVEL_ERROR, "WHEP server answered with status %d: %s", response->status,
        response->body.substr(0, MAX_LOGGED_BODY).c_str());
    return {};
  }
  if (response->body.empty())
  {
    Log(LogLevel::LEVEL_ERROR, "WHEP server sent no answer");
    return {};
  }

  for (const auto& link : response->links)
    Log(LogLevel::LEVEL_DEBUG, "Ignoring Link header of the WHEP server: %s", link.c_str());

  if (response->location.empty())
    Log(LogLevel::LEVEL_DEBUG, "WHEP server sent no session URL");
  else
  {
    const auto [url, options] = SplitOptions(m_endpoint);
    // Kodi reports the URL without user name and password; these are kept unless redirected
    const bool redirected =
        !response->effectiveUrl.empty() && response->effectiveUrl != RemoveUserInfo(url);
    m_session = ResolveUrl(redirected ? response->effectiveUrl : url, response->location) + options;
  }

  return response->body;
}

void CWhepClient::Close()
{
  if (!m_session)
    return;

  HttpRequest request{"DELETE", *m_session, {}, {}};
  AddAuthorization(request);
  m_session.reset();

  const auto response = m_transport.Send(request);
  if (response && (response->status < 200 || response->status >= 300))
    Log(LogLevel::LEVEL_WARNING, "WHEP server answered the end of the session with status %d",
        response->status);
}

} // namespace WEBRTC
