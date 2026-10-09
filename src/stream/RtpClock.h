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
 * \brief Converts the 32 bit RTP timestamps of a stream into presentation times in microseconds.
 */
class CRtpClock
{
public:
  /*!
   * \param start The presentation time of the first timestamp.
   */
  CRtpClock(uint32_t clockRate, int64_t start);

  int64_t ToPresentationTime(uint32_t timestamp);

private:
  const uint32_t m_clockRate;
  const int64_t m_start;
  std::optional<uint32_t> m_last;
  int64_t m_elapsed{0}; // in clock ticks since the first timestamp
};

} // namespace WEBRTC
