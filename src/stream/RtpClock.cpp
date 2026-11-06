/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtpClock.h"

#include "utils/Log.h"

#include <cstdlib>

namespace WEBRTC
{

namespace
{

// Frames ahead of their time by less are jitter, not buffered frames
constexpr int64_t EARLY_TOLERANCE = 10000;
constexpr int64_t CATCH_UP_END = 1000000;
// Beyond the jitter of the arrival; Kodi drops the timestamps of streams more than a second apart
constexpr int64_t MAX_DEVIATION = 1000000;

} // namespace

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
    if (pts > arrival + EARLY_TOLERANCE)
    {
      m_offset -= pts - arrival;
      m_inTimeSince.reset();
      return {arrival, true};
    }
    if (!m_inTimeSince)
      m_inTimeSince = arrival;
    else if (arrival - *m_inTimeSince >= CATCH_UP_END)
      m_catchingUp = false;
  }
  if (std::abs(pts - arrival) > MAX_DEVIATION)
  {
    Log(LogLevel::LEVEL_DEBUG, "RTP timestamps jumped by %lld ms",
        static_cast<long long>((pts - arrival) / 1000));
    m_offset -= pts - arrival;
    return {arrival, false};
  }
  return {pts, false};
}

} // namespace WEBRTC
