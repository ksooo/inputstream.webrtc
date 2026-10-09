/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtpClock.h"

namespace WEBRTC
{

CRtpClock::CRtpClock(uint32_t clockRate) : m_clockRate(clockRate)
{
}

CRtpClock::Time CRtpClock::ToPresentationTime(uint32_t timestamp, int64_t arrival)
{
  if (!m_last)
  {
    m_last = timestamp;
    m_offset = arrival;
    return {arrival, false};
  }

  // The signed difference handles the wrap around and timestamps slightly in the past
  m_elapsed += static_cast<int32_t>(timestamp - *m_last);
  m_last = timestamp;
  const int64_t pts = m_offset + m_elapsed * 1000000 / m_clockRate;

  if (m_catchingUp)
  {
    if (pts > arrival)
    {
      m_offset -= pts - arrival;
      return {arrival, true};
    }
    m_catchingUp = false;
  }
  return {pts, false};
}

} // namespace WEBRTC
