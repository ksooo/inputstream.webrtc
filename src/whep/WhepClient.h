/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "session/Signaling.h"

#include <optional>
#include <string>

namespace WEBRTC
{

class IHttpTransport;
struct HttpRequest;

/*!
 * \brief Signaling through the WebRTC-HTTP Egress Protocol (WHEP). ICE servers announced in Link
 *        headers are not used; they arrive with the answer, after the candidates were gathered.
 */
class CWhepClient : public ISignaling
{
public:
  /*!
   * \param endpoint The WHEP endpoint URL, which may include Kodi's protocol options.
   */
  CWhepClient(IHttpTransport& transport, std::string endpoint, std::string bearerToken);
  ~CWhepClient() override;

  std::optional<std::string> Offer(const std::string& sdp, CandidateCallback onCandidate) override;
  void Close() override;

private:
  void AddAuthorization(HttpRequest& request) const;

  IHttpTransport& m_transport;
  const std::string m_endpoint;
  const std::string m_bearerToken;
  std::optional<std::string> m_session;
};

} // namespace WEBRTC
