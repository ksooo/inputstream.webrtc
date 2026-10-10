/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "LogStub.h"
#include "LoopbackSignaling.h"
#include "session/IceServers.h"
#include "session/Session.h"
#include "session/Signaling.h"
#include "stream/StreamBuffer.h"
#include "utils/RtcLog.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>
#include <rtc/global.hpp>

using namespace WEBRTC;
using namespace std::chrono_literals;

namespace
{

// A UDP port that receives but never answers, like an unreachable TURN server
class CSilentServer
{
public:
  CSilentServer()
  {
    // Initializes the sockets on Windows
    rtc::Preload();
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    socklen_t length = sizeof(address);
    getsockname(m_socket, reinterpret_cast<sockaddr*>(&address), &length);
    m_port = ntohs(address.sin_port);
  }

  ~CSilentServer()
  {
#ifdef _WIN32
    closesocket(m_socket);
#else
    close(m_socket);
#endif
  }

  uint16_t GetPort() const { return m_port; }

private:
#ifdef _WIN32
  SOCKET m_socket;
#else
  int m_socket;
#endif
  uint16_t m_port{0};
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

TEST(SessionTest, AcceptsManyCandidates)
{
  InitRtcLog();
  TakeLogMessages();
  // go2rtc announces about 40, depending on the network interfaces of its host
  CLoopbackSignaling signaling(false, 0, 40);
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
  // In real time, not like buffered frames
  std::this_thread::sleep_for(100ms);
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

TEST(SessionTest, DoesNotWaitForSilentTurnServer)
{
  CSilentServer turnServer;
  CLoopbackSignaling signaling;
  CSession session(
      {ParseIceServers({"turn:user:pass@127.0.0.1:" + std::to_string(turnServer.GetPort())}), false,
       10s, BIND_ADDRESS},
      MakeBuffer());

  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(session.Connect(signaling));
  EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

TEST(SessionTest, ReceivesAudio)
{
  // An Opus frame with a TOC byte for 20 ms of CELT audio
  const std::vector<uint8_t> frame{0xfc, 0xff, 0xfe};
  CLoopbackSignaling signaling;
  auto buffer = MakeBuffer();
  CSession session({{}, true, 10s, BIND_ADDRESS}, buffer);
  ASSERT_TRUE(session.Connect(signaling));

  // Like a source, send in real time. Early frames may arrive together, like buffered ones.
  std::vector<MediaPacket> packets;
  MediaPacket packet;
  for (uint32_t timestamp = 960; packets.size() < 3 && timestamp <= 250 * 960; timestamp += 960)
  {
    ASSERT_TRUE(signaling.SendAudioFrame(frame, timestamp));
    std::this_thread::sleep_for(20ms);
    if (buffer->Pop(0ms, packet) == CStreamBuffer::Result::PACKET)
      packets.emplace_back(packet);
  }
  ASSERT_EQ(packets.size(), 3u);
  EXPECT_EQ(packets[0].streamId, 2);
  EXPECT_EQ(packets[0].data, frame);
  // 960 samples at 48 kHz
  EXPECT_EQ(packets[2].pts - packets[1].pts, 20000);

  const auto stream = buffer->GetStream(2);
  ASSERT_TRUE(stream);
  EXPECT_EQ(stream->codec, Codec::OPUS);
  EXPECT_EQ(stream->sampleRate, 48000u);
  EXPECT_EQ(stream->channels, 2u);

  session.Close();
  signaling.Close();
}
