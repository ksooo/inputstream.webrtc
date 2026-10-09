/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "HaSignaling.h"

#include "session/IceServers.h"
#include "utils/Log.h"

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string_view>
#include <utility>
#include <variant>

namespace WEBRTC
{

namespace
{

std::string GetString(const nlohmann::json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string())
    return {};
  return it->get<std::string>();
}

std::string GetErrorMessage(const nlohmann::json& result)
{
  const auto it = result.find("error");
  if (it == result.end() || !it->is_object())
    return "unknown error";
  return GetString(*it, "code") + ": " + GetString(*it, "message");
}

} // namespace

struct CHaSignaling::State
{
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<nlohmann::json> messages;
  bool closed{false};
  int offerId{0};
  CandidateCallback onCandidate;
  std::optional<std::string> answer;
  bool offerFailed{false};

  void HandleMessage(const std::string& text);
  void HandleOfferEvent(const nlohmann::json& event);
};

void CHaSignaling::State::HandleMessage(const std::string& text)
{
  nlohmann::json message = nlohmann::json::parse(text, nullptr, false);
  if (!message.is_object())
  {
    Log(LogLevel::LEVEL_WARNING, "Ignoring invalid message from Home Assistant");
    return;
  }

  if (GetString(message, "type") == "event")
  {
    const auto event = message.find("event");
    bool forOffer;
    {
      std::lock_guard lock(mutex);
      forOffer = offerId != 0 && message.value("id", 0) == offerId;
    }
    if (forOffer && event != message.end() && event->is_object())
      HandleOfferEvent(*event);
    return;
  }

  {
    std::lock_guard lock(mutex);
    messages.emplace_back(std::move(message));
  }
  changed.notify_all();
}

void CHaSignaling::State::HandleOfferEvent(const nlohmann::json& event)
{
  const std::string type = GetString(event, "type");
  if (type == "answer")
  {
    {
      std::lock_guard lock(mutex);
      answer = GetString(event, "answer");
    }
    changed.notify_all();
  }
  else if (type == "candidate")
  {
    const auto candidate = event.find("candidate");
    if (candidate == event.end() || !candidate->is_object())
      return;
    const std::string line = GetString(*candidate, "candidate");
    if (line.empty())
      return;
    const std::string mid = GetString(*candidate, "sdpMid");

    CandidateCallback callback;
    {
      std::lock_guard lock(mutex);
      callback = onCandidate;
    }
    if (!callback)
      return;
    try
    {
      callback(mid.empty() ? rtc::Candidate(line) : rtc::Candidate(line, mid));
    }
    catch (const std::exception& e)
    {
      Log(LogLevel::LEVEL_WARNING, "Ignoring candidate from Home Assistant: %s", e.what());
    }
  }
  else if (type == "error")
  {
    Log(LogLevel::LEVEL_ERROR, "Home Assistant: %s: %s", GetString(event, "code").c_str(),
        GetString(event, "message").c_str());
    {
      std::lock_guard lock(mutex);
      offerFailed = true;
    }
    changed.notify_all();
  }
  else if (type == "session")
  {
    Log(LogLevel::LEVEL_DEBUG, "Home Assistant session %s", GetString(event, "session_id").c_str());
  }
}

CHaSignaling::CHaSignaling(std::string url,
                           std::string token,
                           std::string entityId,
                           std::optional<std::string> caFile,
                           std::chrono::milliseconds timeout)
  : m_url(std::move(url)),
    m_token(std::move(token)),
    m_entityId(std::move(entityId)),
    m_caFile(std::move(caFile)),
    m_timeout(timeout),
    m_state(std::make_shared<State>())
{
}

CHaSignaling::~CHaSignaling()
{
  Close();
}

std::optional<std::string> CHaSignaling::GetWebSocketUrl(const std::string& url)
{
  std::string base = url.substr(0, url.find('|'));
  const size_t separator = base.find("://");
  if (separator == std::string::npos)
    return {};

  const std::string scheme = base.substr(0, separator);
  std::string rest = base.substr(separator + 3);
  while (!rest.empty() && rest.back() == '/')
    rest.pop_back();
  if (rest.empty())
    return {};

  std::string webSocketScheme;
  if (scheme == "http" || scheme == "ws")
    webSocketScheme = "ws";
  else if (scheme == "https" || scheme == "wss")
    webSocketScheme = "wss";
  else
    return {};

  constexpr std::string_view API_PATH = "/api/websocket";
  if (rest.size() < API_PATH.size() ||
      rest.compare(rest.size() - API_PATH.size(), API_PATH.size(), API_PATH) != 0)
    rest += API_PATH;

  return webSocketScheme + "://" + rest;
}

bool CHaSignaling::Connect()
{
  if (m_connected)
    return *m_connected;
  m_connected = false;

  const auto url = GetWebSocketUrl(m_url);
  if (!url)
  {
    Log(LogLevel::LEVEL_ERROR, "Invalid Home Assistant URL");
    return false;
  }

  rtc::WebSocketConfiguration configuration;
  configuration.connectionTimeout = m_timeout;
  if (url->starts_with("wss://"))
  {
    if (!m_caFile)
    {
      Log(LogLevel::LEVEL_ERROR,
          "No trusted root certificates found, unable to verify the Home Assistant server");
      return false;
    }
    configuration.caCertificatePemFile = m_caFile;
  }

  try
  {
    m_webSocket = std::make_shared<rtc::WebSocket>(configuration);
    m_webSocket->onMessage(
        [state = m_state](rtc::message_variant data)
        {
          if (const auto* text = std::get_if<std::string>(&data))
            state->HandleMessage(*text);
        });
    m_webSocket->onError([](std::string error)
                         { Log(LogLevel::LEVEL_ERROR, "Home Assistant: %s", error.c_str()); });
    m_webSocket->onClosed(
        [state = m_state]
        {
          {
            std::lock_guard lock(state->mutex);
            state->closed = true;
          }
          state->changed.notify_all();
        });
    m_webSocket->open(*url);
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to connect to Home Assistant: %s", e.what());
    return false;
  }

  if (!WaitForMessage([](const nlohmann::json& message)
                      { return GetString(message, "type") == "auth_required"; }))
    return false;

  if (!Send({{"type", "auth"}, {"access_token", m_token}}))
    return false;

  const auto result = WaitForMessage(
      [](const nlohmann::json& message)
      {
        const std::string type = GetString(message, "type");
        return type == "auth_ok" || type == "auth_invalid";
      });
  if (!result)
    return false;
  if (GetString(*result, "type") != "auth_ok")
  {
    Log(LogLevel::LEVEL_ERROR, "Home Assistant rejected the access token: %s",
        GetString(*result, "message").c_str());
    return false;
  }

  m_connected = true;
  return true;
}

bool CHaSignaling::Send(const nlohmann::json& message)
{
  try
  {
    m_webSocket->send(message.dump());
    return true;
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to send to Home Assistant: %s", e.what());
    return false;
  }
}

std::optional<nlohmann::json> CHaSignaling::WaitForMessage(
    const std::function<bool(const nlohmann::json&)>& matches)
{
  std::unique_lock lock(m_state->mutex);
  std::optional<nlohmann::json> found;
  const bool done = m_state->changed.wait_for(lock, m_timeout,
                                              [&]
                                              {
                                                for (auto it = m_state->messages.begin();
                                                     it != m_state->messages.end(); ++it)
                                                {
                                                  if (matches(*it))
                                                  {
                                                    found = std::move(*it);
                                                    m_state->messages.erase(it);
                                                    return true;
                                                  }
                                                }
                                                return m_state->closed;
                                              });

  if (!found)
    Log(LogLevel::LEVEL_ERROR,
        done ? "Home Assistant closed the connection" : "Timed out waiting for Home Assistant");
  return found;
}

std::optional<nlohmann::json> CHaSignaling::Request(nlohmann::json request)
{
  const int id = m_nextId++;
  request["id"] = id;
  if (!Send(request))
    return {};

  auto result = WaitForMessage(
      [id](const nlohmann::json& message)
      { return GetString(message, "type") == "result" && message.value("id", 0) == id; });
  if (!result)
    return {};
  if (!result->value("success", false))
  {
    Log(LogLevel::LEVEL_ERROR, "Home Assistant: %s", GetErrorMessage(*result).c_str());
    return {};
  }
  return result;
}

std::vector<rtc::IceServer> CHaSignaling::GetIceServers()
{
  std::vector<rtc::IceServer> servers;
  if (!Connect())
    return servers;

  const auto result =
      Request({{"type", "camera/webrtc/get_client_config"}, {"entity_id", m_entityId}});
  if (!result)
    return servers;

  const nlohmann::json iceServers = result->value(
      nlohmann::json::json_pointer("/result/configuration/iceServers"), nlohmann::json::array());
  for (const auto& entry : iceServers)
  {
    if (!entry.is_object())
      continue;

    std::vector<std::string> urls;
    const auto urlsEntry = entry.find("urls");
    if (urlsEntry != entry.end() && urlsEntry->is_string())
      urls.emplace_back(urlsEntry->get<std::string>());
    else if (urlsEntry != entry.end() && urlsEntry->is_array())
    {
      for (const auto& url : *urlsEntry)
      {
        if (url.is_string())
          urls.emplace_back(url.get<std::string>());
      }
    }

    for (const auto& url : urls)
    {
      auto server = ParseIceServer(url);
      if (!server)
        continue;
      if (const std::string username = GetString(entry, "username"); !username.empty())
        server->username = username;
      if (const std::string credential = GetString(entry, "credential"); !credential.empty())
        server->password = credential;
      servers.emplace_back(std::move(*server));
    }
  }

  Log(LogLevel::LEVEL_DEBUG, "Home Assistant announced %zu ICE servers", servers.size());
  return servers;
}

std::optional<std::string> CHaSignaling::Offer(const std::string& sdp,
                                               CandidateCallback onCandidate)
{
  if (!Connect())
    return {};

  const int id = m_nextId;
  {
    std::lock_guard lock(m_state->mutex);
    m_state->offerId = id;
    m_state->onCandidate = std::move(onCandidate);
  }

  if (!Request({{"type", "camera/webrtc/offer"}, {"entity_id", m_entityId}, {"offer", sdp}}))
    return {};

  std::unique_lock lock(m_state->mutex);
  const bool done = m_state->changed.wait_for(
      lock, m_timeout,
      [this] { return m_state->answer || m_state->offerFailed || m_state->closed; });
  if (m_state->answer)
    return m_state->answer;

  if (!done)
    Log(LogLevel::LEVEL_ERROR, "Timed out waiting for the answer of Home Assistant");
  else if (m_state->closed)
    Log(LogLevel::LEVEL_ERROR, "Home Assistant closed the connection");
  return {};
}

void CHaSignaling::Close()
{
  {
    std::lock_guard lock(m_state->mutex);
    m_state->onCandidate = nullptr;
  }
  if (m_webSocket)
  {
    m_webSocket->close();
    m_webSocket.reset();
  }
}

} // namespace WEBRTC
