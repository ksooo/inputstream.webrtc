/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtcLog.h"

#include <string>

namespace WEBRTC
{

LogLevel ToLogLevel(rtc::LogLevel level)
{
  switch (level)
  {
    case rtc::LogLevel::Fatal:
    case rtc::LogLevel::Error:
      return LogLevel::LEVEL_ERROR;
    case rtc::LogLevel::Warning:
      return LogLevel::LEVEL_WARNING;
    case rtc::LogLevel::Info:
      return LogLevel::LEVEL_INFO;
    default:
      return LogLevel::LEVEL_DEBUG;
  }
}

void InitRtcLog()
{
  rtc::InitLogger(rtc::LogLevel::Debug, [](rtc::LogLevel level, std::string message)
                  { WriteLog(ToLogLevel(level), "libdatachannel: " + message); });
}

} // namespace WEBRTC
