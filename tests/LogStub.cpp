/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "LogStub.h"

#include "utils/Log.h"

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <utility>

namespace WEBRTC
{

namespace
{

std::mutex g_mutex;
std::vector<std::string> g_messages;

} // namespace

void WriteLog(LogLevel level, const std::string& message)
{
  {
    std::lock_guard lock(g_mutex);
    g_messages.emplace_back(message);
  }
  if (std::getenv("WEBRTC_TEST_LOG"))
    std::cerr << static_cast<int>(level) << ": " << message << std::endl;
}

std::vector<std::string> TakeLogMessages()
{
  std::lock_guard lock(g_mutex);
  return std::exchange(g_messages, {});
}

} // namespace WEBRTC
