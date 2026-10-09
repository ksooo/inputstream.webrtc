/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "LogStub.h"
#include "session/Session.h"
#include "session/Signaling.h"
#include "stream/StreamBuffer.h"
#include "utils/RtcLog.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <rtc/h264rtppacketizer.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/plihandler.hpp>

using namespace WEBRTC;
using namespace std::chrono_literals;

namespace
{

// Keeps the traffic on the loopback interface, which firewalls let through
constexpr const char* BIND_ADDRESS = "127.0.0.1";

// Answers the offer with a peer connection in the same process, like a camera would
class CLoopbackSignaling : public ISignaling
{
public:
  static constexpr uint32_t VIDEO_SSRC = 42;

  explicit CLoopbackSignaling(bool unreachable = false, int tcpCandidates = 0)
    : m_unreachable(unreachable),
      m_tcpCandidates(tcpCandidates)
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

    auto answer = std::make_shared<std::promise<std::string>>();
    auto videoTrack = std::make_shared<std::promise<std::shared_ptr<rtc::Track>>>();
    rtc::Configuration configuration;
    configuration.bindAddress = BIND_ADDRESS;
    configuration.disableAutoNegotiation = true;
    m_peer = std::make_shared<rtc::PeerConnection>(configuration);
    m_peer->onLocalDescription([answer](rtc::Description description)
                               { answer->set_value(std::string(description)); });
    m_peer->onTrack(
        [videoTrack](std::shared_ptr<rtc::Track> track)
        {
          if (track->mid() == "video")
            videoTrack->set_value(track);
        });
    if (!m_unreachable)
      m_peer->onLocalCandidate([onCandidate](rtc::Candidate candidate)
                               { onCandidate(std::move(candidate)); });
    m_peer->setRemoteDescription(rtc::Description(sdp, rtc::Description::Type::Offer));

    auto trackFuture = videoTrack->get_future();
    if (trackFuture.wait_for(5s) != std::future_status::ready)
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
    m_peer->setLocalDescription(rtc::Description::Type::Answer);

    auto future = answer->get_future();
    if (future.wait_for(5s) != std::future_status::ready)
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
      std::this_thread::sleep_for(10ms);
    if (!m_videoTrack->isOpen())
      return false;
    const auto* bytes = reinterpret_cast<const std::byte*>(frame.data());
    m_videoTrack->sendFrame(rtc::binary(bytes, bytes + frame.size()), rtc::FrameInfo(timestamp));
    return true;
  }

  bool WaitForKeyframeRequest()
  {
    std::unique_lock lock(m_mutex);
    return m_changed.wait_for(lock, 5s, [this] { return m_keyframeRequests > 0; });
  }

private:
  const bool m_unreachable;
  const int m_tcpCandidates;
  std::shared_ptr<rtc::PeerConnection> m_peer;
  std::shared_ptr<rtc::Track> m_videoTrack;
  std::string m_offer;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  int m_keyframeRequests{0};
};

std::shared_ptr<CStreamBuffer> MakeBuffer()
{
  return std::make_shared<CStreamBuffer>();
}

} // namespace

TEST(SessionTest, ConnectsWithVideoAndAudio)
{
  CLoopbackSignaling signaling;
  CSession session({{}, true, 10s, BIND_ADDRESS}, MakeBuffer());

  ASSERT_TRUE(session.Connect(signaling));
  EXPECT_NE(signaling.GetOffer().find("m=video"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("m=audio"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("a=recvonly"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("profile-level-id=64001f"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("H265/90000"), std::string::npos);
  EXPECT_EQ(signaling.GetOffer().find("VP8/90000"), std::string::npos);
  EXPECT_TRUE(session.GetVideoTrack());
  EXPECT_TRUE(session.GetAudioTrack());

  session.Close();
  signaling.Close();
}

TEST(SessionTest, VideoOnly)
{
  CLoopbackSignaling signaling;
  CSession session({{}, false, 10s, BIND_ADDRESS}, MakeBuffer());

  ASSERT_TRUE(session.Connect(signaling));
  EXPECT_EQ(signaling.GetOffer().find("m=audio"), std::string::npos);
  EXPECT_TRUE(session.GetVideoTrack());
  EXPECT_FALSE(session.GetAudioTrack());

  session.Close();
  signaling.Close();
}

TEST(SessionTest, TimesOutWhenPeerIsUnreachable)
{
  CLoopbackSignaling signaling(true);
  CSession session({{}, true, 2s, BIND_ADDRESS}, MakeBuffer());

  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(session.Connect(signaling));
  EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

TEST(SessionTest, IgnoresTcpCandidates)
{
  InitRtcLog();
  TakeLogMessages();
  CLoopbackSignaling signaling(false, 30);
  CSession session({{}, false, 10s, BIND_ADDRESS}, MakeBuffer());

  ASSERT_TRUE(session.Connect(signaling));
  session.Close();
  signaling.Close();
  for (const auto& message : TakeLogMessages())
    EXPECT_EQ(message.find("maximum number of candidates"), std::string::npos) << message;
}

TEST(SessionTest, ReceivesVideo)
{
  // SPS, PPS and the start of an IDR slice
  const std::vector<uint8_t> frame{0,    0, 0, 1,    0x67, 0x42, 0xe0, 0x1f, 0xda, 0x01,
                                   0x40, 0, 0, 0,    1,    0x68, 0xce, 0x3c, 0x80, 0,
                                   0,    0, 1, 0x65, 0x88, 0x84, 0x00, 0x33, 0xff};
  CLoopbackSignaling signaling;
  auto buffer = MakeBuffer();
  CSession session({{}, false, 10s, BIND_ADDRESS}, buffer);
  ASSERT_TRUE(session.Connect(signaling));

  // Like a camera, answer the keyframe request
  auto video = std::async(std::launch::async, [&session] { return session.WaitForVideo(5s); });
  ASSERT_TRUE(signaling.WaitForKeyframeRequest());
  ASSERT_TRUE(signaling.SendVideoFrame(frame, 3000));
  ASSERT_TRUE(signaling.SendVideoFrame(frame, 6000));
  ASSERT_TRUE(video.get());

  const auto streams = buffer->GetStreams();
  ASSERT_EQ(streams.size(), 1u);
  EXPECT_EQ(streams[0].codec, Codec::H264);

  std::vector<MediaPacket> packets;
  MediaPacket packet;
  for (int i = 0; i < 100 && packets.size() < 2; ++i)
  {
    if (buffer->Pop(50ms, packet) == CStreamBuffer::Result::PACKET)
      packets.emplace_back(packet);
  }
  ASSERT_EQ(packets.size(), 2u);
  EXPECT_EQ(packets[0].streamId, streams[0].id);
  EXPECT_EQ(packets[0].data, frame);
  // 3000 ticks of the 90 kHz clock
  EXPECT_EQ(packets[1].pts - packets[0].pts, 33333);

  session.Close();
  EXPECT_EQ(buffer->Pop(50ms, packet), CStreamBuffer::Result::ENDED);
  signaling.Close();
}

TEST(SessionTest, WaitForVideoWhenTrackIsNotOpen)
{
  CLoopbackSignaling signaling(true);
  CSession session({{}, false, 1s, BIND_ADDRESS}, MakeBuffer());
  ASSERT_FALSE(session.Connect(signaling));

  bool received = true;
  EXPECT_NO_THROW(received = session.WaitForVideo(1s));
  EXPECT_FALSE(received);
}
