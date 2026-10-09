/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <optional>

namespace WEBRTC
{

/*!
 * \brief Converts the 32 bit RTP timestamps of a stream into presentation times in microseconds
 *        since the session start. The first frame is presented at its arrival. While the source
 *        then sends buffered frames faster than real time, as go2rtc does with the frames since
 *        the last keyframe, the presentation times follow the arrival, so the buffered frames do
 *        not delay the playback. From the first frame that arrives in time, the RTP timestamps
 *        rule.
 */
class CRtpClock
{
public:
  struct Time
  {
    int64_t pts;
    bool catchingUp; // a buffered frame, sent faster than real time
  };

  explicit CRtpClock(uint32_t clockRate);

  /*!
   * \param arrival The arrival of the frame in microseconds since the session start.
   */
  Time ToPresentationTime(uint32_t timestamp, int64_t arrival);

private:
  const uint32_t m_clockRate;
  std::optional<uint32_t> m_last;
  int64_t m_elapsed{0}; // in clock ticks since the first timestamp
  int64_t m_offset{0}; // presentation time of the first timestamp
  bool m_catchingUp{true};
};

} // namespace WEBRTC
