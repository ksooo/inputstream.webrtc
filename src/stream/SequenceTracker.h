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
 * \brief Detects lost RTP packets from the sequence numbers of a stream.
 */
class CSequenceTracker
{
public:
  /*!
   * \return The number of packets missing before this one; 0 also for reordered and repeated ones.
   */
  unsigned int Update(uint16_t sequenceNumber);

private:
  std::optional<uint16_t> m_highest;
};

} // namespace WEBRTC
