/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "LogStub.h"
#include "LoopbackSignaling.h"
#include "session/Connection.h"
#include "stream/StreamBuffer.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace WEBRTC;
using namespace std::chrono_literals;

namespace
{

const ConnectionTiming FAST{5s, 300ms, 100ms, 500ms, 1min};

// Creates the remote peers, like a camera would for each connection, and sends an IDR frame every
// 40 ms through the latest one that answered, while sending is on
class CCamera
{
public:
  CCamera()
  {
    m_thread = std::thread(
        [this]
        {
          const std::vector<uint8_t> frame{0, 0, 0, 1, 0x65, 0x88, 0x84};
          uint32_t timestamp = 0;
          while (!m_stopped)
          {
            if (m_sending)
            {
              if (const auto peer = GetAnswered())
                peer->SendVideoFrame(frame, timestamp += 3600);
            }
            std::this_thread::sleep_for(40ms);
          }
        });
  }

  ~CCamera()
  {
    m_stopped = true;
    m_thread.join();
  }

  CConnection::SignalingFactory GetFactory()
  {
    return [this]
    {
      auto peer = std::make_shared<CLoopbackSignaling>(m_unreachable);
      {
        std::lock_guard lock(m_mutex);
        m_peers.push_back(peer);
      }
      return std::make_unique<CForwardingSignaling>(peer,
                                                    [this, peer]
                                                    {
                                                      std::lock_guard lock(m_mutex);
                                                      m_answered = peer;
                                                    });
    };
  }

  std::shared_ptr<CLoopbackSignaling> GetLatest()
  {
    std::lock_guard lock(m_mutex);
    return m_peers.empty() ? nullptr : m_peers.back();
  }

  std::shared_ptr<CLoopbackSignaling> GetAnswered()
  {
    std::lock_guard lock(m_mutex);
    return m_answered;
  }

  size_t GetConnections()
  {
    std::lock_guard lock(m_mutex);
    return m_peers.size();
  }

  bool WaitForConnections(size_t count)
  {
    for (int i = 0; i < 100 && GetConnections() < count; ++i)
      std::this_thread::sleep_for(50ms);
    return GetConnections() >= count;
  }

  void SetSending(bool sending) { m_sending = sending; }
  void SetUnreachable(bool unreachable) { m_unreachable = unreachable; }

private:
  // The connection owns its signaling, the camera keeps the peer to send frames
  class CForwardingSignaling : public ISignaling
  {
  public:
    CForwardingSignaling(std::shared_ptr<CLoopbackSignaling> peer, std::function<void()> onAnswered)
      : m_peer(std::move(peer)),
        m_onAnswered(std::move(onAnswered))
    {
    }
    std::optional<std::string> Offer(const std::string& sdp, CandidateCallback onCandidate) override
    {
      auto answer = m_peer->Offer(sdp, std::move(onCandidate));
      if (answer)
        m_onAnswered();
      return answer;
    }
    void Close() override { m_peer->Close(); }

  private:
    const std::shared_ptr<CLoopbackSignaling> m_peer;
    const std::function<void()> m_onAnswered;
  };

  std::mutex m_mutex;
  std::vector<std::shared_ptr<CLoopbackSignaling>> m_peers;
  std::shared_ptr<CLoopbackSignaling> m_answered; // the latest peer that can send
  std::atomic<bool> m_sending{true};
  std::atomic<bool> m_unreachable{false};
  std::atomic<bool> m_stopped{false};
  std::thread m_thread;
};

std::vector<int64_t> PopPresentationTimes(CStreamBuffer& buffer)
{
  std::vector<int64_t> times;
  MediaPacket packet;
  while (true)
  {
    const auto result = buffer.Pop(0ms, packet);
    if (result == CStreamBuffer::Result::PACKET)
      times.push_back(packet.pts);
    else if (result != CStreamBuffer::Result::STREAMS_CHANGED)
      return times;
  }
}

// The presentation times that arrive within 5 s
std::vector<int64_t> WaitForPresentationTimes(CStreamBuffer& buffer)
{
  std::vector<int64_t> times;
  for (int i = 0; i < 100 && times.empty(); ++i)
  {
    std::this_thread::sleep_for(50ms);
    times = PopPresentationTimes(buffer);
  }
  return times;
}

} // namespace

TEST(ConnectionTest, ReconnectsWhenVideoStops)
{
  CCamera camera;
  auto buffer = std::make_shared<CStreamBuffer>();
  CConnection connection({{}, false, 5s, BIND_ADDRESS}, camera.GetFactory(), buffer, FAST);
  ASSERT_TRUE(connection.Open());
  std::this_thread::sleep_for(200ms);
  const auto before = PopPresentationTimes(*buffer);
  ASSERT_FALSE(before.empty());

  // Longer than the video timeout and the check interval
  camera.SetSending(false);
  std::this_thread::sleep_for(1s);
  camera.SetSending(true);
  ASSERT_TRUE(camera.WaitForConnections(2));

  const auto after = WaitForPresentationTimes(*buffer);
  ASSERT_FALSE(after.empty());
  // The presentation times continue from the first connection
  EXPECT_GT(after.front(), before.back());
  connection.Close();
}

TEST(ConnectionTest, ReconnectsWhenPeerCloses)
{
  CCamera camera;
  auto buffer = std::make_shared<CStreamBuffer>();
  CConnection connection({{}, false, 5s, BIND_ADDRESS}, camera.GetFactory(), buffer, FAST);
  ASSERT_TRUE(connection.Open());

  camera.GetLatest()->Close();
  ASSERT_TRUE(camera.WaitForConnections(2));
  // From the first connection
  PopPresentationTimes(*buffer);
  EXPECT_FALSE(WaitForPresentationTimes(*buffer).empty());
  connection.Close();
}

TEST(ConnectionTest, GivesUp)
{
  CCamera camera;
  auto buffer = std::make_shared<CStreamBuffer>();
  ConnectionTiming timing = FAST;
  timing.giveUpAfter = 1s;
  CConnection connection({{}, false, 500ms, BIND_ADDRESS}, camera.GetFactory(), buffer, timing);
  ASSERT_TRUE(connection.Open());

  camera.SetUnreachable(true);
  camera.SetSending(false);
  PopPresentationTimes(*buffer);
  MediaPacket packet;
  EXPECT_EQ(buffer->Pop(10s, packet), CStreamBuffer::Result::ENDED);
  EXPECT_GT(camera.GetConnections(), 2u);
  connection.Close();
}

TEST(ConnectionTest, ClosesWhileReconnecting)
{
  CCamera camera;
  auto buffer = std::make_shared<CStreamBuffer>();
  ConnectionTiming timing = FAST;
  timing.firstRetryDelay = 1min;
  CConnection connection({{}, false, 500ms, BIND_ADDRESS}, camera.GetFactory(), buffer, timing);
  ASSERT_TRUE(connection.Open());

  camera.SetUnreachable(true);
  camera.SetSending(false);
  ASSERT_TRUE(camera.WaitForConnections(2));
  std::this_thread::sleep_for(1s);

  const auto start = std::chrono::steady_clock::now();
  connection.Close();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}
