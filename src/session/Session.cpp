/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Session.h"

#include "Signaling.h"
#include "utils/Log.h"

#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

namespace WEBRTC
{

namespace
{

constexpr int PAYLOAD_TYPE_H264_BASELINE = 96;
constexpr int PAYLOAD_TYPE_H264_MAIN = 97;
constexpr int PAYLOAD_TYPE_H264_HIGH = 98;
constexpr int PAYLOAD_TYPE_H265 = 99;
constexpr int PAYLOAD_TYPE_VP8 = 100;
constexpr int PAYLOAD_TYPE_VP9 = 101;
constexpr int PAYLOAD_TYPE_AV1 = 102;
constexpr int PAYLOAD_TYPE_OPUS = 111;
constexpr int PAYLOAD_TYPE_PCMU = 0;
constexpr int PAYLOAD_TYPE_PCMA = 8;

std::string H264Profile(const char* profileLevelId)
{
  return std::string("profile-level-id=") + profileLevelId +
         ";packetization-mode=1;level-asymmetry-allowed=1";
}

void AddRemoteCandidate(rtc::PeerConnection& peerConnection, rtc::Candidate candidate)
{
  try
  {
    peerConnection.addRemoteCandidate(std::move(candidate));
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_WARNING, "Ignoring remote candidate: %s", e.what());
  }
}

} // namespace

struct CSession::State
{
  std::mutex mutex;
  std::condition_variable changed;
  rtc::PeerConnection::State connectionState{rtc::PeerConnection::State::New};
  bool gatheringComplete{false};
  bool remoteDescriptionSet{false};
  std::vector<rtc::Candidate> pendingCandidates;
  std::weak_ptr<rtc::PeerConnection> peerConnection;
};

CSession::CSession(SessionConfig config)
  : m_config(std::move(config)),
    m_state(std::make_shared<State>())
{
}

CSession::~CSession()
{
  Close();
}

bool CSession::Connect(ISignaling& signaling)
{
  const auto deadline = std::chrono::steady_clock::now() + m_config.timeout;

  try
  {
    rtc::Configuration configuration;
    configuration.iceServers = m_config.iceServers;
    for (auto& server : signaling.GetIceServers())
      configuration.iceServers.emplace_back(std::move(server));
    configuration.bindAddress = m_config.bindAddress;
    configuration.disableAutoNegotiation = true;

    m_peerConnection = std::make_shared<rtc::PeerConnection>(configuration);
    m_state->peerConnection = m_peerConnection;

    m_peerConnection->onStateChange(
        [state = m_state](rtc::PeerConnection::State connectionState)
        {
          {
            std::lock_guard lock(state->mutex);
            state->connectionState = connectionState;
          }
          state->changed.notify_all();
        });
    m_peerConnection->onGatheringStateChange(
        [state = m_state](rtc::PeerConnection::GatheringState gatheringState)
        {
          if (gatheringState != rtc::PeerConnection::GatheringState::Complete)
            return;
          {
            std::lock_guard lock(state->mutex);
            state->gatheringComplete = true;
          }
          state->changed.notify_all();
        });

    rtc::Description::Video video("video", rtc::Description::Direction::RecvOnly);
    video.addH264Codec(PAYLOAD_TYPE_H264_BASELINE, H264Profile("42e01f"));
    video.addH264Codec(PAYLOAD_TYPE_H264_MAIN, H264Profile("4d001f"));
    video.addH264Codec(PAYLOAD_TYPE_H264_HIGH, H264Profile("64001f"));
    video.addH265Codec(PAYLOAD_TYPE_H265);
    video.addVP8Codec(PAYLOAD_TYPE_VP8);
    video.addVP9Codec(PAYLOAD_TYPE_VP9);
    video.addAV1Codec(PAYLOAD_TYPE_AV1);
    m_videoTrack = m_peerConnection->addTrack(video);

    if (m_config.audio)
    {
      rtc::Description::Audio audio("audio", rtc::Description::Direction::RecvOnly);
      audio.addOpusCodec(PAYLOAD_TYPE_OPUS);
      audio.addPCMUCodec(PAYLOAD_TYPE_PCMU);
      audio.addPCMACodec(PAYLOAD_TYPE_PCMA);
      m_audioTrack = m_peerConnection->addTrack(audio);
    }

    m_peerConnection->setLocalDescription(rtc::Description::Type::Offer);
    {
      std::unique_lock lock(m_state->mutex);
      if (!m_state->changed.wait_until(lock, deadline,
                                       [this] { return m_state->gatheringComplete; }))
      {
        Log(LogLevel::LEVEL_ERROR, "Timed out gathering ICE candidates");
        return false;
      }
    }

    const auto answer =
        signaling.Offer(std::string(*m_peerConnection->localDescription()),
                        [state = m_state](rtc::Candidate candidate)
                        {
                          std::shared_ptr<rtc::PeerConnection> peerConnection;
                          {
                            std::lock_guard lock(state->mutex);
                            if (!state->remoteDescriptionSet)
                            {
                              state->pendingCandidates.emplace_back(std::move(candidate));
                              return;
                            }
                            peerConnection = state->peerConnection.lock();
                          }
                          if (peerConnection)
                            AddRemoteCandidate(*peerConnection, std::move(candidate));
                        });
    if (!answer)
      return false;

    m_peerConnection->setRemoteDescription(
        rtc::Description(*answer, rtc::Description::Type::Answer));

    std::vector<rtc::Candidate> pendingCandidates;
    {
      std::lock_guard lock(m_state->mutex);
      m_state->remoteDescriptionSet = true;
      pendingCandidates = std::move(m_state->pendingCandidates);
    }
    for (auto& candidate : pendingCandidates)
      AddRemoteCandidate(*m_peerConnection, std::move(candidate));

    LogNegotiatedCodecs();

    std::unique_lock lock(m_state->mutex);
    const bool settled = m_state->changed.wait_until(
        lock, deadline,
        [this]
        {
          return m_state->connectionState == rtc::PeerConnection::State::Connected ||
                 m_state->connectionState == rtc::PeerConnection::State::Failed ||
                 m_state->connectionState == rtc::PeerConnection::State::Closed;
        });
    if (!settled)
    {
      Log(LogLevel::LEVEL_ERROR, "Timed out connecting");
      return false;
    }
    if (m_state->connectionState != rtc::PeerConnection::State::Connected)
    {
      Log(LogLevel::LEVEL_ERROR, "Connection failed");
      return false;
    }
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to connect: %s", e.what());
    return false;
  }

  Log(LogLevel::LEVEL_INFO, "Connected");
  return true;
}

void CSession::Close()
{
  if (m_peerConnection)
  {
    m_peerConnection->close();
    m_peerConnection.reset();
  }
  m_videoTrack.reset();
  m_audioTrack.reset();
}

void CSession::LogNegotiatedCodecs() const
{
  auto description = m_peerConnection->remoteDescription();
  if (!description)
    return;

  for (int i = 0; i < description->mediaCount(); ++i)
  {
    const auto entry = description->media(i);
    const auto* media = std::get_if<rtc::Description::Media*>(&entry);
    if (!media)
      continue;

    std::string codecs;
    for (const int payloadType : (*media)->payloadTypes())
    {
      const auto* rtpMap = (*media)->rtpMap(payloadType);
      if (!codecs.empty())
        codecs += ", ";
      codecs += rtpMap->format + "/" + std::to_string(rtpMap->clockRate);
    }
    Log(LogLevel::LEVEL_INFO, "Negotiated %s: %s", (*media)->mid().c_str(),
        codecs.empty() ? "nothing" : codecs.c_str());
  }
}

} // namespace WEBRTC
