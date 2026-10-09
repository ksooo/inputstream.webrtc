/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtpClock.h"

namespace WEBRTC
{

CRtpClock::CRtpClock(uint32_t clockRate, int64_t start) : m_clockRate(clockRate), m_start(start)
{
}

int64_t CRtpClock::ToPresentationTime(uint32_t timestamp)
{
  // The signed difference handles the wrap around and timestamps slightly in the past
  if (m_last)
    m_elapsed += static_cast<int32_t>(timestamp - *m_last);
  m_last = timestamp;
  return m_start + m_elapsed * 1000000 / m_clockRate;
}

} // namespace WEBRTC
