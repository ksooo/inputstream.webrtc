/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Session.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace WEBRTC
{

class CStreamBuffer;
class ISignaling;

struct ConnectionTiming
{
  std::chrono::milliseconds firstKeyframe{std::chrono::seconds(5)};
  std::chrono::milliseconds videoTimeout{std::chrono::seconds(5)};
  std::chrono::milliseconds firstRetryDelay{std::chrono::seconds(1)};
  std::chrono::milliseconds maxRetryDelay{std::chrono::seconds(10)};
  std::chrono::milliseconds giveUpAfter{std::chrono::minutes(5)};
};

/*!
 * \brief Keeps a session connected. When the connection is lost or no video arrives any more, a
 *        new signaling and session connect, until that succeeds or the connection gives up and
 *        ends the stream buffer. The streams continue in the same stream buffer, with
 *        presentation times that count from the first connection.
 */
class CConnection
{
public:
  using SignalingFactory = std::function<std::unique_ptr<ISignaling>()>;

  CConnection(SessionConfig config,
              SignalingFactory signalingFactory,
              std::shared_ptr<CStreamBuffer> buffer,
              ConnectionTiming timing = {});
  ~CConnection();

  CConnection(const CConnection&) = delete;
  CConnection& operator=(const CConnection&) = delete;

  /*!
   * \brief Connects and waits for video; from then on, the connection is kept up.
   */
  bool Open();
  void Close();

private:
  bool Connect();
  void CloseSession();
  void Run();
  bool Reconnect();
  bool WaitUntilStopped(std::chrono::milliseconds timeout);

  const SessionConfig m_config;
  const SignalingFactory m_signalingFactory;
  const std::shared_ptr<CStreamBuffer> m_buffer;
  const ConnectionTiming m_timing;
  const std::chrono::steady_clock::time_point m_start;
  std::unique_ptr<ISignaling> m_signaling;
  std::mutex m_mutex;
  std::condition_variable m_changed;
  std::shared_ptr<CSession> m_session;
  bool m_stopped{false};
  std::thread m_thread;
};

} // namespace WEBRTC
