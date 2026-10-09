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
 * \brief A WebRTC connection that receives video and optionally audio. The received streams are
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

  std::shared_ptr<rtc::Track> GetVideoTrack() const { return m_video.track; }
  std::shared_ptr<rtc::Track> GetAudioTrack() const { return m_audio.track; }

private:
  struct State;

  struct ReceivingTrack
  {
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<CRtpReceiver> receiver;
    std::shared_ptr<CMediaStream> stream;
  };

  ReceivingTrack AddReceivingTrack(const rtc::Description::Media& description, int streamId);
  void SetCodecs(ReceivingTrack& receivingTrack, const std::string& mid);

  void LogNegotiatedCodecs() const;

  const SessionConfig m_config;
  const std::shared_ptr<CStreamBuffer> m_buffer;
  std::shared_ptr<State> m_state;
  std::shared_ptr<rtc::PeerConnection> m_peerConnection;
  std::chrono::steady_clock::time_point m_start;
  ReceivingTrack m_video;
  ReceivingTrack m_audio;
};

} // namespace WEBRTC
