/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <rtc/configuration.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/track.hpp>

namespace WEBRTC
{

class CMediaStream;
class CRtpReceiver;
class CStreamBuffer;
class ISignaling;

struct SessionConfig
{
  std::vector<rtc::IceServer> iceServers;
  bool audio{true};
  std::chrono::milliseconds timeout{std::chrono::seconds(10)};
  std::optional<std::string> bindAddress; // the only local address used for ICE
};

/*!
 * \brief A WebRTC connection that receives video and optionally audio. The received video is
 *        passed to the stream buffer, which also learns about the end of the connection.
 */
class CSession
{
public:
  CSession(SessionConfig config, std::shared_ptr<CStreamBuffer> buffer);
  ~CSession();

  CSession(const CSession&) = delete;
  CSession& operator=(const CSession&) = delete;

  /*!
   * \brief Negotiates the session through the signaling and waits until it is connected.
   */
  bool Connect(ISignaling& signaling);

  /*!
   * \brief Requests keyframes until the first one arrives; a request sent right after connecting
   *        can get lost while the remote side still completes the connection.
   */
  bool WaitForVideo(std::chrono::milliseconds timeout);

  void Close();

  std::shared_ptr<rtc::Track> GetVideoTrack() const { return m_videoTrack; }
  std::shared_ptr<rtc::Track> GetAudioTrack() const { return m_audioTrack; }

private:
  struct State;

  void LogNegotiatedCodecs() const;

  const SessionConfig m_config;
  const std::shared_ptr<CStreamBuffer> m_buffer;
  std::shared_ptr<State> m_state;
  std::shared_ptr<rtc::PeerConnection> m_peerConnection;
  std::shared_ptr<rtc::Track> m_videoTrack;
  std::shared_ptr<rtc::Track> m_audioTrack;
  std::shared_ptr<CRtpReceiver> m_videoReceiver;
  std::shared_ptr<CMediaStream> m_videoStream;
};

} // namespace WEBRTC
