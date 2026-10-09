/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

#if defined(__GNUC__)
#define WEBRTC_PRINTF_FORMAT __attribute__((format(printf, 2, 3)))
#else
#define WEBRTC_PRINTF_FORMAT
#endif

namespace WEBRTC
{

enum class LogLevel
{
  LEVEL_DEBUG,
  LEVEL_INFO,
  LEVEL_WARNING,
  LEVEL_ERROR
};

/*!
 * \brief Writes a message to the Kodi log. The unit tests link a stand-in implementation.
 */
void WriteLog(LogLevel level, const std::string& message);

inline void WEBRTC_PRINTF_FORMAT Log(LogLevel level, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  va_list argsCopy;
  va_copy(argsCopy, args);
  const int length = std::vsnprintf(nullptr, 0, format, args);
  va_end(args);

  std::string message;
  if (length > 0)
  {
    message.resize(length);
    std::vsnprintf(message.data(), message.size() + 1, format, argsCopy);
  }
  va_end(argsCopy);

  WriteLog(level, message);
}

} // namespace WEBRTC
