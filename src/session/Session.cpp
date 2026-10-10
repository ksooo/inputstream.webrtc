/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Session.h"

#include "Candidates.h"
#include "IceServers.h"
#include "RtpReceiver.h"
#include "Signaling.h"
#include "stream/MediaStream.h"
#include "stream/StreamBuffer.h"
#include "utils/Log.h"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

#include <rtc/rtcpreceivingsession.hpp>

namespace WEBRTC
{

namespace
{

constexpr const char* VIDEO_MID = "video";
constexpr const char* AUDIO_MID = "audio";
constexpr int VIDEO_STREAM_ID = 1;
constexpr int AUDIO_STREAM_ID = 2;
constexpr std::chrono::milliseconds KEYFRAME_REQUEST_INTERVAL{500};
constexpr std::chrono::milliseconds MAX_GATHERING_TIME{2000};

constexpr int PAYLOAD_TYPE_H264_BASELINE = 96;
constexpr int PAYLOAD_TYPE_H264_MAIN = 97;
constexpr int PAYLOAD_TYPE_H264_HIGH = 98;
constexpr int PAYLOAD_TYPE_H265 = 99;
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
  Log(LogLevel::LEVEL_DEBUG, "Remote candidate: %s", std::string(candidate).c_str());

  // libjuice does not connect over TCP, but counts TCP candidates towards its candidate limit
  if (candidate.transportType() != rtc::Candidate::TransportType::Udp)
    return;

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
  bool aborted{false};
  std::optional<std::chrono::steady_clock::time_point> lastVideo;
  std::vector<rtc::Candidate> pendingCandidates;
  std::weak_ptr<rtc::PeerConnection> peerConnection;
};

CSession::CSession(SessionConfig config,
                   std::shared_ptr<CStreamBuffer> buffer,
                   std::chrono::steady_clock::time_point start)
  : m_config(std::move(config)),
    m_buffer(std::move(buffer)),
    m_state(std::make_shared<State>()),
    m_start(start)
{
}

CSession::~CSession()
{
  Close();
}

CSession::ReceivingTrack CSession::AddReceivingTrack(const rtc::Description::Media& description,
                                                     int streamId)
{
  ReceivingTrack receivingTrack;
  receivingTrack.track = m_peerConnection->addTrack(description);
  receivingTrack.receiver = std::make_shared<CRtpReceiver>();
  receivingTrack.track->setMediaHandler(receivingTrack.receiver);
  receivingTrack.track->chainMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
  receivingTrack.stream = std::make_shared<CMediaStream>(streamId, m_buffer, m_start);
  receivingTrack.track->onFrame(
      [stream = receivingTrack.stream, state = m_state,
       video = streamId == VIDEO_STREAM_ID](rtc::binary data, rtc::FrameInfo info)
      {
        const auto arrival = std::chrono::steady_clock::now();
        if (!stream->OnFrame(data, info.payloadType, info.timestamp, arrival) || !video)
          return;
        {
          std::lock_guard lock(state->mutex);
          state->lastVideo = arrival;
        }
        state->changed.notify_all();
      });
  return receivingTrack;
}

void CSession::SetCodecs(ReceivingTrack& receivingTrack, const std::string& mid)
{
  if (!receivingTrack.track)
    return;
  const auto codecs = GetCodecs(*m_peerConnection->remoteDescription(), mid);
  receivingTrack.receiver->SetCodecs(codecs);
  receivingTrack.stream->SetCodecs(codecs);
}

bool CSession::Connect(ISignaling& signaling)
{
  const auto deadline = std::chrono::steady_clock::now() + m_config.timeout;

  try
  {
    rtc::Configuration configuration;
    auto iceServers = m_config.iceServers;
    for (auto& server : signaling.GetIceServers())
      iceServers.emplace_back(std::move(server));
    configuration.iceServers = RemoveTcpTurnServers(std::move(iceServers));
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

    rtc::Description::Video video(VIDEO_MID, rtc::Description::Direction::RecvOnly);
    video.addH264Codec(PAYLOAD_TYPE_H264_BASELINE, H264Profile("42e01f"));
    video.addH264Codec(PAYLOAD_TYPE_H264_MAIN, H264Profile("4d001f"));
    video.addH264Codec(PAYLOAD_TYPE_H264_HIGH, H264Profile("64001f"));
    video.addH265Codec(PAYLOAD_TYPE_H265);
    m_video = AddReceivingTrack(video, VIDEO_STREAM_ID);

    if (m_config.audio)
    {
      rtc::Description::Audio audio(AUDIO_MID, rtc::Description::Direction::RecvOnly);
      audio.addOpusCodec(PAYLOAD_TYPE_OPUS);
      audio.addPCMUCodec(PAYLOAD_TYPE_PCMU);
      audio.addPCMACodec(PAYLOAD_TYPE_PCMA);
      m_audio = AddReceivingTrack(audio, AUDIO_STREAM_ID);
    }

    m_peerConnection->setLocalDescription(rtc::Description::Type::Offer);
    {
      // A STUN or TURN server that does not answer must not hold up the offer; the local
      // candidates are there at once
      std::unique_lock lock(m_state->mutex);
      if (!m_state->changed.wait_until(
              lock, std::min(deadline, std::chrono::steady_clock::now() + MAX_GATHERING_TIME),
              [this] { return m_state->gatheringComplete || m_state->aborted; }))
        Log(LogLevel::LEVEL_INFO, "Sending the offer before all ICE candidates were gathered");
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
        rtc::Description(RemoveTcpCandidates(*answer), rtc::Description::Type::Answer));

    std::vector<rtc::Candidate> pendingCandidates;
    {
      std::lock_guard lock(m_state->mutex);
      m_state->remoteDescriptionSet = true;
      pendingCandidates = std::move(m_state->pendingCandidates);
    }
    for (auto& candidate : pendingCandidates)
      AddRemoteCandidate(*m_peerConnection, std::move(candidate));

    LogNegotiatedCodecs();
    SetCodecs(m_video, VIDEO_MID);
    SetCodecs(m_audio, AUDIO_MID);

    std::unique_lock lock(m_state->mutex);
    const bool settled = m_state->changed.wait_until(
        lock, deadline,
        [this]
        {
          return m_state->connectionState == rtc::PeerConnection::State::Connected ||
                 m_state->connectionState == rtc::PeerConnection::State::Failed ||
                 m_state->connectionState == rtc::PeerConnection::State::Closed || m_state->aborted;
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

bool CSession::WaitForVideo(std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (m_video.track)
  {
    try
    {
      m_video.track->requestKeyframe();
    }
    catch (const std::exception& e)
    {
      // Until the track has its SRTP transport, shortly after connecting
      Log(LogLevel::LEVEL_DEBUG, "Unable to request a keyframe: %s", e.what());
    }
    std::unique_lock lock(m_state->mutex);
    if (m_state->changed.wait_until(
            lock, std::min(deadline, std::chrono::steady_clock::now() + KEYFRAME_REQUEST_INTERVAL),
            [this] { return m_state->lastVideo || m_state->aborted; }))
      return m_state->lastVideo.has_value();
    if (std::chrono::steady_clock::now() >= deadline)
      break;
  }
  return false;
}

bool CSession::IsAlive(std::chrono::milliseconds videoTimeout) const
{
  std::lock_guard lock(m_state->mutex);
  return m_state->connectionState == rtc::PeerConnection::State::Connected && m_state->lastVideo &&
         std::chrono::steady_clock::now() - *m_state->lastVideo < videoTimeout;
}

void CSession::Abort()
{
  {
    std::lock_guard lock(m_state->mutex);
    m_state->aborted = true;
  }
  m_state->changed.notify_all();
}

void CSession::Close()
{
  if (m_peerConnection)
  {
    m_peerConnection->close();
    m_peerConnection.reset();
  }
  m_video = {};
  m_audio = {};
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
