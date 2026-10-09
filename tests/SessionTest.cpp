/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "session/Session.h"
#include "session/Signaling.h"

#include <chrono>
#include <future>
#include <memory>

#include <gtest/gtest.h>
#include <rtc/peerconnection.hpp>

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
  explicit CLoopbackSignaling(bool unreachable = false) : m_unreachable(unreachable) {}
  ~CLoopbackSignaling() override { Close(); }

  std::optional<std::string> Offer(const std::string& sdp, CandidateCallback onCandidate) override
  {
    m_offer = sdp;

    auto answer = std::make_shared<std::promise<std::string>>();
    rtc::Configuration configuration;
    configuration.bindAddress = BIND_ADDRESS;
    m_peer = std::make_shared<rtc::PeerConnection>(configuration);
    m_peer->onLocalDescription([answer](rtc::Description description)
                               { answer->set_value(std::string(description)); });
    if (!m_unreachable)
      m_peer->onLocalCandidate([onCandidate](rtc::Candidate candidate)
                               { onCandidate(std::move(candidate)); });
    m_peer->setRemoteDescription(rtc::Description(sdp, rtc::Description::Type::Offer));

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

private:
  const bool m_unreachable;
  std::shared_ptr<rtc::PeerConnection> m_peer;
  std::string m_offer;
};

} // namespace

TEST(SessionTest, ConnectsWithVideoAndAudio)
{
  CLoopbackSignaling signaling;
  CSession session({{}, true, 10s, BIND_ADDRESS});

  ASSERT_TRUE(session.Connect(signaling));
  EXPECT_NE(signaling.GetOffer().find("m=video"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("m=audio"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("a=recvonly"), std::string::npos);
  EXPECT_NE(signaling.GetOffer().find("profile-level-id=64001f"), std::string::npos);
  EXPECT_TRUE(session.GetVideoTrack());
  EXPECT_TRUE(session.GetAudioTrack());

  session.Close();
  signaling.Close();
}

TEST(SessionTest, VideoOnly)
{
  CLoopbackSignaling signaling;
  CSession session({{}, false, 10s, BIND_ADDRESS});

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
  CSession session({{}, true, 2s, BIND_ADDRESS});

  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(session.Connect(signaling));
  EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}
