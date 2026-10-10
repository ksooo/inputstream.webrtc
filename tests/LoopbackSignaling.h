/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "session/Signaling.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/plihandler.hpp>
#include <rtc/rtppacketizer.hpp>

namespace WEBRTC
{

// Keeps the traffic on the loopback interface, which firewalls let through
inline constexpr const char* BIND_ADDRESS = "127.0.0.1";

// Answers the offer with a peer connection in the same process, like a camera would
class CLoopbackSignaling : public ISignaling
{
public:
  static constexpr uint32_t VIDEO_SSRC = 42;
  static constexpr uint32_t AUDIO_SSRC = 43;

  explicit CLoopbackSignaling(bool unreachable = false,
                              int tcpCandidates = 0,
                              int udpCandidates = 0)
    : m_unreachable(unreachable),
      m_tcpCandidates(tcpCandidates),
      m_udpCandidates(udpCandidates)
  {
  }
  ~CLoopbackSignaling() override { Close(); }

  std::optional<std::string> Offer(const std::string& sdp, CandidateCallback onCandidate) override
  {
    m_offer = sdp;
    for (int i = 0; i < m_tcpCandidates; ++i)
      onCandidate(rtc::Candidate("candidate:" + std::to_string(i) + " 1 tcp 1671430143 127.0.0.1 " +
                                     std::to_string(9000 + i) + " typ host tcptype passive",
                                 "video"));
    for (int i = 0; i < m_udpCandidates; ++i)
      onCandidate(rtc::Candidate("candidate:" + std::to_string(100 + i) +
                                     " 1 udp 2122260223 127.0.0.1 " + std::to_string(9100 + i) +
                                     " typ host",
                                 "video"));

    auto answer = std::make_shared<std::promise<std::string>>();
    auto videoTrack = std::make_shared<std::promise<std::shared_ptr<rtc::Track>>>();
    auto audioTrack = std::make_shared<std::promise<std::shared_ptr<rtc::Track>>>();
    rtc::Configuration configuration;
    configuration.bindAddress = BIND_ADDRESS;
    configuration.disableAutoNegotiation = true;
    m_peer = std::make_shared<rtc::PeerConnection>(configuration);
    m_peer->onLocalDescription([answer](rtc::Description description)
                               { answer->set_value(std::string(description)); });
    m_peer->onTrack(
        [videoTrack, audioTrack](std::shared_ptr<rtc::Track> track)
        {
          if (track->mid() == "video")
            videoTrack->set_value(track);
          else if (track->mid() == "audio")
            audioTrack->set_value(track);
        });
    if (!m_unreachable)
      m_peer->onLocalCandidate([onCandidate](rtc::Candidate candidate)
                               { onCandidate(std::move(candidate)); });
    m_peer->setRemoteDescription(rtc::Description(sdp, rtc::Description::Type::Offer));

    auto trackFuture = videoTrack->get_future();
    if (trackFuture.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      return {};
    m_videoTrack = trackFuture.get();
    auto description = m_videoTrack->description();
    description.addSSRC(VIDEO_SSRC, "camera");
    m_videoTrack->setDescription(description);
    m_videoTrack->setMediaHandler(std::make_shared<rtc::H264RtpPacketizer>(
        rtc::NalUnit::Separator::LongStartSequence,
        std::make_shared<rtc::RtpPacketizationConfig>(VIDEO_SSRC, "camera", 96, 90000)));
    m_videoTrack->chainMediaHandler(std::make_shared<rtc::PliHandler>(
        [this]
        {
          {
            std::lock_guard lock(m_mutex);
            ++m_keyframeRequests;
          }
          m_changed.notify_all();
        }));
    if (sdp.find("m=audio") != std::string::npos)
    {
      auto audioFuture = audioTrack->get_future();
      if (audioFuture.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
        return {};
      m_audioTrack = audioFuture.get();
      auto audioDescription = m_audioTrack->description();
      audioDescription.addSSRC(AUDIO_SSRC, "camera");
      m_audioTrack->setDescription(audioDescription);
      m_audioTrack->setMediaHandler(std::make_shared<rtc::OpusRtpPacketizer>(
          std::make_shared<rtc::RtpPacketizationConfig>(AUDIO_SSRC, "camera", 111, 48000)));
    }
    m_peer->setLocalDescription(rtc::Description::Type::Answer);

    auto future = answer->get_future();
    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      return {};
    std::string result = future.get();
    if (m_unreachable)
      Close();
    return result;
  }

  void Close() override
  {
    if (m_peer)
    {
      m_peer->resetCallbacks();
      m_peer->close();
    }
  }

  const std::string& GetOffer() const { return m_offer; }

  bool SendVideoFrame(const std::vector<uint8_t>& frame, uint32_t timestamp)
  {
    for (int i = 0; i < 100 && !m_videoTrack->isOpen(); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!m_videoTrack->isOpen())
      return false;
    const auto* bytes = reinterpret_cast<const std::byte*>(frame.data());
    m_videoTrack->sendFrame(rtc::binary(bytes, bytes + frame.size()), rtc::FrameInfo(timestamp));
    return true;
  }

  bool SendAudioFrame(const std::vector<uint8_t>& frame, uint32_t timestamp)
  {
    for (int i = 0; i < 100 && !m_audioTrack->isOpen(); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!m_audioTrack->isOpen())
      return false;
    const auto* bytes = reinterpret_cast<const std::byte*>(frame.data());
    m_audioTrack->sendFrame(rtc::binary(bytes, bytes + frame.size()), rtc::FrameInfo(timestamp));
    return true;
  }

  bool WaitForKeyframeRequest()
  {
    std::unique_lock lock(m_mutex);
    return m_changed.wait_for(lock, std::chrono::seconds(5),
                              [this] { return m_keyframeRequests > 0; });
  }

private:
  const bool m_unreachable;
  const int m_tcpCandidates;
  const int m_udpCandidates;
  std::shared_ptr<rtc::PeerConnection> m_peer;
  std::shared_ptr<rtc::Track> m_videoTrack;
  std::shared_ptr<rtc::Track> m_audioTrack;
  std::string m_offer;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  int m_keyframeRequests{0};
};

} // namespace WEBRTC
