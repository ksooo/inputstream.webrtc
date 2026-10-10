/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Connection.h"

#include "Signaling.h"
#include "stream/StreamBuffer.h"
#include "utils/Log.h"

#include <algorithm>
#include <utility>

namespace WEBRTC
{

namespace
{

constexpr std::chrono::milliseconds CHECK_INTERVAL = std::chrono::milliseconds(500);

} // namespace

CConnection::CConnection(SessionConfig config,
                         SignalingFactory signalingFactory,
                         std::shared_ptr<CStreamBuffer> buffer,
                         ConnectionTiming timing)
  : m_config(std::move(config)),
    m_signalingFactory(std::move(signalingFactory)),
    m_buffer(std::move(buffer)),
    m_timing(timing),
    m_start(std::chrono::steady_clock::now())
{
}

CConnection::~CConnection()
{
  Close();
}

bool CConnection::Open()
{
  if (!Connect())
  {
    CloseSession();
    return false;
  }
  m_thread = std::thread([this] { Run(); });
  return true;
}

void CConnection::Close()
{
  std::shared_ptr<CSession> session;
  {
    std::lock_guard lock(m_mutex);
    m_stopped = true;
    session = m_session;
  }
  m_changed.notify_all();
  if (session)
    session->Abort();
  if (m_thread.joinable())
    m_thread.join();
  CloseSession();
}

bool CConnection::Connect()
{
  auto session = std::make_shared<CSession>(m_config, m_buffer, m_start);
  {
    std::lock_guard lock(m_mutex);
    if (m_stopped)
      return false;
    m_session = session;
  }
  m_signaling = m_signalingFactory();
  if (!session->Connect(*m_signaling))
    return false;
  if (!session->WaitForVideo(m_timing.firstKeyframe))
  {
    Log(LogLevel::LEVEL_ERROR, "Received no video");
    return false;
  }
  return true;
}

void CConnection::CloseSession()
{
  std::shared_ptr<CSession> session;
  {
    std::lock_guard lock(m_mutex);
    session = std::move(m_session);
  }
  if (session)
    session->Close();
  if (m_signaling)
    m_signaling->Close();
  m_signaling.reset();
}

void CConnection::Run()
{
  while (!WaitUntilStopped(CHECK_INTERVAL))
  {
    if (m_session->IsAlive(m_timing.videoTimeout))
      continue;
    Log(LogLevel::LEVEL_INFO, "The connection was lost, reconnecting");
    if (!Reconnect())
      return;
  }
}

bool CConnection::Reconnect()
{
  const auto giveUp = std::chrono::steady_clock::now() + m_timing.giveUpAfter;
  auto delay = m_timing.firstRetryDelay;
  while (true)
  {
    CloseSession();
    if (Connect())
    {
      Log(LogLevel::LEVEL_INFO, "Reconnected");
      return true;
    }
    if (WaitUntilStopped(std::chrono::milliseconds(0)))
      return false;
    if (std::chrono::steady_clock::now() + delay >= giveUp)
    {
      Log(LogLevel::LEVEL_ERROR, "Giving up reconnecting");
      CloseSession();
      m_buffer->End();
      return false;
    }
    if (WaitUntilStopped(delay))
      return false;
    delay = std::min(delay * 2, m_timing.maxRetryDelay);
  }
}

bool CConnection::WaitUntilStopped(std::chrono::milliseconds timeout)
{
  std::unique_lock lock(m_mutex);
  return m_changed.wait_for(lock, timeout, [this] { return m_stopped; });
}

} // namespace WEBRTC
