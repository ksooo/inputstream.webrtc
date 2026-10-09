/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ha/HaSignaling.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <variant>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <rtc/websocketserver.hpp>

using namespace WEBRTC;
using namespace std::chrono_literals;

namespace
{

constexpr auto TIMEOUT = 5s;
const std::string CANDIDATE_1 = "candidate:1 1 UDP 2122317823 192.168.1.10 50000 typ host";
const std::string CANDIDATE_2 = "candidate:2 1 UDP 2122317567 192.168.1.10 50001 typ host";

rtc::WebSocketServerConfiguration MakeServerConfiguration()
{
  rtc::WebSocketServerConfiguration configuration;
  configuration.port = 0;
  configuration.bindAddress = "127.0.0.1";
  return configuration;
}

// Answers like the camera WebRTC commands of Home Assistant
class CFakeHomeAssistant
{
public:
  CFakeHomeAssistant() : m_server(MakeServerConfiguration())
  {
    m_server.onClient([this](std::shared_ptr<rtc::WebSocket> client) { OnClient(client); });
  }

  ~CFakeHomeAssistant()
  {
    m_server.stop();
    std::vector<std::shared_ptr<rtc::WebSocket>> clients;
    {
      std::lock_guard lock(m_mutex);
      clients = m_clients;
    }
    for (const auto& client : clients)
    {
      client->resetCallbacks();
      client->close();
    }
  }

  std::string GetUrl() const { return "http://127.0.0.1:" + std::to_string(m_server.port()); }

  std::vector<nlohmann::json> GetReceived(const std::string& type)
  {
    std::lock_guard lock(m_mutex);
    std::vector<nlohmann::json> messages;
    for (const auto& message : m_received)
    {
      if (message.value("type", "") == type)
        messages.emplace_back(message);
    }
    return messages;
  }

  bool WaitForClose()
  {
    std::unique_lock lock(m_mutex);
    return m_changed.wait_for(lock, TIMEOUT, [this] { return m_clientClosed; });
  }

  std::string m_token{"token"};
  bool m_offerFails{false};

private:
  void OnClient(const std::shared_ptr<rtc::WebSocket>& client)
  {
    std::weak_ptr<rtc::WebSocket> weakClient = client;
    client->onOpen(
        [weakClient]
        {
          if (auto client = weakClient.lock())
            client->send(nlohmann::json{{"type", "auth_required"}}.dump());
        });
    client->onMessage(
        [this, weakClient](rtc::message_variant data)
        {
          auto client = weakClient.lock();
          if (client && std::holds_alternative<std::string>(data))
            OnMessage(*client, nlohmann::json::parse(std::get<std::string>(data)));
        });
    client->onClosed(
        [this]
        {
          {
            std::lock_guard lock(m_mutex);
            m_clientClosed = true;
          }
          m_changed.notify_all();
        });

    std::lock_guard lock(m_mutex);
    m_clients.emplace_back(client);
  }

  void OnMessage(rtc::WebSocket& client, const nlohmann::json& message)
  {
    {
      std::lock_guard lock(m_mutex);
      m_received.emplace_back(message);
    }

    const std::string type = message.value("type", "");
    const int id = message.value("id", 0);
    if (type == "auth")
    {
      if (message.value("access_token", "") == m_token)
        client.send(nlohmann::json{{"type", "auth_ok"}}.dump());
      else
        client.send(nlohmann::json{{"type", "auth_invalid"}, {"message", "Invalid access"}}.dump());
    }
    else if (type == "camera/webrtc/get_client_config")
    {
      const nlohmann::json iceServers = nlohmann::json::array(
          {{{"urls", "stun:stun.example.com:3478"}},
           {{"urls", {"turn:turn.example.com:3478?transport=udp", "http://invalid"}},
            {"username", "user"},
            {"credential", "pass"}}});
      client.send(nlohmann::json{{"id", id},
                                 {"type", "result"},
                                 {"success", true},
                                 {"result", {{"configuration", {{"iceServers", iceServers}}}}}}
                      .dump());
    }
    else if (type == "camera/webrtc/offer")
    {
      client.send(nlohmann::json{{"id", id}, {"type", "result"}, {"success", true}}.dump());
      SendEvent(client, id, {{"type", "session"}, {"session_id", "1"}});
      if (m_offerFails)
      {
        SendEvent(client, id,
                  {{"type", "error"}, {"code", "webrtc_offer_failed"}, {"message", "No stream"}});
        return;
      }
      SendEvent(
          client, id,
          {{"type", "candidate"}, {"candidate", {{"candidate", CANDIDATE_1}, {"sdpMid", "0"}}}});
      SendEvent(client, id,
                {{"type", "answer"}, {"answer", "answer for " + message.value("offer", "")}});
      SendEvent(client, id, {{"type", "candidate"}, {"candidate", {{"candidate", CANDIDATE_2}}}});
    }
  }

  static void SendEvent(rtc::WebSocket& client, int id, const nlohmann::json& event)
  {
    client.send(nlohmann::json{{"id", id}, {"type", "event"}, {"event", event}}.dump());
  }

  rtc::WebSocketServer m_server;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  std::vector<std::shared_ptr<rtc::WebSocket>> m_clients;
  std::vector<nlohmann::json> m_received;
  bool m_clientClosed{false};
};

class CCandidateCollector
{
public:
  ISignaling::CandidateCallback GetCallback()
  {
    return [this](rtc::Candidate candidate)
    {
      {
        std::lock_guard lock(m_mutex);
        m_candidates.emplace_back(std::move(candidate));
      }
      m_changed.notify_all();
    };
  }

  std::vector<rtc::Candidate> WaitFor(size_t count)
  {
    std::unique_lock lock(m_mutex);
    m_changed.wait_for(lock, TIMEOUT, [&] { return m_candidates.size() >= count; });
    return m_candidates;
  }

private:
  std::mutex m_mutex;
  std::condition_variable m_changed;
  std::vector<rtc::Candidate> m_candidates;
};

} // namespace

TEST(HaSignalingTest, GetsIceServersAndAnswer)
{
  CFakeHomeAssistant homeAssistant;
  CHaSignaling signaling(homeAssistant.GetUrl(), "token", "camera.garden", {}, TIMEOUT);

  const auto servers = signaling.GetIceServers();
  ASSERT_EQ(servers.size(), 2u);
  EXPECT_EQ(servers[0].type, rtc::IceServer::Type::Stun);
  EXPECT_EQ(servers[1].type, rtc::IceServer::Type::Turn);
  EXPECT_EQ(servers[1].username, "user");
  EXPECT_EQ(servers[1].password, "pass");

  CCandidateCollector collector;
  EXPECT_EQ(signaling.Offer("offer", collector.GetCallback()), "answer for offer");

  const auto candidates = collector.WaitFor(2);
  ASSERT_EQ(candidates.size(), 2u);
  EXPECT_EQ(candidates[0].mid(), "0");
  EXPECT_EQ(candidates[0].candidate(), CANDIDATE_1);
  EXPECT_EQ(candidates[1].candidate(), CANDIDATE_2);

  const auto auth = homeAssistant.GetReceived("auth");
  ASSERT_EQ(auth.size(), 1u);
  EXPECT_EQ(auth[0].value("access_token", ""), "token");
  const auto offers = homeAssistant.GetReceived("camera/webrtc/offer");
  ASSERT_EQ(offers.size(), 1u);
  EXPECT_EQ(offers[0].value("entity_id", ""), "camera.garden");
  EXPECT_EQ(offers[0].value("offer", ""), "offer");

  signaling.Close();
  EXPECT_TRUE(homeAssistant.WaitForClose());
}

TEST(HaSignalingTest, RejectedToken)
{
  CFakeHomeAssistant homeAssistant;
  CHaSignaling signaling(homeAssistant.GetUrl(), "wrong", "camera.garden", {}, TIMEOUT);

  EXPECT_TRUE(signaling.GetIceServers().empty());
  EXPECT_FALSE(signaling.Offer("offer", nullptr));
  EXPECT_TRUE(homeAssistant.GetReceived("camera/webrtc/offer").empty());
}

TEST(HaSignalingTest, OfferFails)
{
  CFakeHomeAssistant homeAssistant;
  homeAssistant.m_offerFails = true;
  CHaSignaling signaling(homeAssistant.GetUrl(), "token", "camera.garden", {}, TIMEOUT);

  EXPECT_FALSE(signaling.Offer("offer", nullptr));
}

TEST(HaSignalingTest, RefusesHttpsWithoutCertificates)
{
  CFakeHomeAssistant homeAssistant;
  std::string url = homeAssistant.GetUrl();
  url.replace(0, 4, "https");
  CHaSignaling signaling(url, "token", "camera.garden", {}, TIMEOUT);

  EXPECT_FALSE(signaling.Offer("offer", nullptr));
  EXPECT_TRUE(homeAssistant.GetReceived("auth").empty());
}

TEST(HaSignalingTest, WebSocketUrl)
{
  EXPECT_EQ(CHaSignaling::GetWebSocketUrl("http://ha.local:8123"),
            "ws://ha.local:8123/api/websocket");
  EXPECT_EQ(CHaSignaling::GetWebSocketUrl("https://abc.ui.nabu.casa/"),
            "wss://abc.ui.nabu.casa/api/websocket");
  EXPECT_EQ(CHaSignaling::GetWebSocketUrl("http://ha.local:8123/api/websocket|User-Agent=Kodi"),
            "ws://ha.local:8123/api/websocket");
  EXPECT_FALSE(CHaSignaling::GetWebSocketUrl("rtsp://ha.local"));
  EXPECT_FALSE(CHaSignaling::GetWebSocketUrl("http://"));
}
