/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SequenceTracker.h"

namespace WEBRTC
{

unsigned int CSequenceTracker::Update(uint16_t sequenceNumber)
{
  if (!m_highest)
  {
    m_highest = sequenceNumber;
    return 0;
  }

  const auto difference = static_cast<int16_t>(sequenceNumber - *m_highest);
  if (difference <= 0)
    return 0;

  m_highest = sequenceNumber;
  return static_cast<unsigned int>(difference - 1);
}

} // namespace WEBRTC
