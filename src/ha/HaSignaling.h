/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "session/Signaling.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>
#include <rtc/websocket.hpp>

namespace WEBRTC
{

/*!
 * \brief Signaling through the camera WebRTC commands of the Home Assistant WebSocket API.
 *        The connection has to stay open while the session is used, Home Assistant ends the
 *        camera session when it closes.
 */
class CHaSignaling : public ISignaling
{
public:
  /*!
   * \param url The Home Assistant URL, e.g. http://homeassistant.local:8123.
   * \param caFile The trusted root certificates; required for https.
   */
  CHaSignaling(std::string url,
               std::string token,
               std::string entityId,
               std::optional<std::string> caFile,
               std::chrono::milliseconds timeout);
  ~CHaSignaling() override;

  std::vector<rtc::IceServer> GetIceServers() override;
  std::optional<std::string> Offer(const std::string& sdp, CandidateCallback onCandidate) override;
  void Close() override;

  /*!
   * \brief The WebSocket API URL for a Home Assistant URL.
   */
  static std::optional<std::string> GetWebSocketUrl(const std::string& url);

private:
  struct State;

  bool Connect();
  bool Send(const nlohmann::json& message);
  std::optional<nlohmann::json> WaitForMessage(
      const std::function<bool(const nlohmann::json&)>& matches);
  std::optional<nlohmann::json> Request(nlohmann::json request);

  const std::string m_url;
  const std::string m_token;
  const std::string m_entityId;
  const std::optional<std::string> m_caFile;
  const std::chrono::milliseconds m_timeout;
  std::shared_ptr<State> m_state;
  std::shared_ptr<rtc::WebSocket> m_webSocket;
  std::optional<bool> m_connected;
  int m_nextId{1};
};

} // namespace WEBRTC
