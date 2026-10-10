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
 *        passed to the stream buffer.
 */
class CSession
{
public:
  /*!
   * \param start The time the presentation times of the streams count from.
   */
  CSession(SessionConfig config,
           std::shared_ptr<CStreamBuffer> buffer,
           std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now());
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

  /*!
   * \brief Whether the session is connected and received video within the timeout.
   */
  bool IsAlive(std::chrono::milliseconds videoTimeout) const;

  /*!
   * \brief Lets a waiting Connect() or WaitForVideo() return; from any thread.
   */
  void Abort();

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
  const std::chrono::steady_clock::time_point m_start;
  ReceivingTrack m_video;
  ReceivingTrack m_audio;
};

} // namespace WEBRTC
