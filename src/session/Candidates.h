/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <atomic>
#include <string>

#include <rtc/candidate.hpp>

namespace WEBRTC
{

/*!
 * \brief Removes the TCP candidates from a session description.
 */
std::string RemoveTcpCandidates(const std::string& sdp);

/*!
 * \brief Limits the number of remote relay candidates.
 *
 * libjuice accepts a limited number of remote candidates, including the peer reflexive ones it
 * learns while connecting. A server with many addresses announces a relay candidate for each of
 * them and each TURN server, which would use up that limit.
 */
class CRelayCandidateLimit
{
public:
  static constexpr int MAX_RELAY_CANDIDATES = 10;

  /*!
   * \brief Whether to use a remote candidate; thread-safe.
   */
  bool Accept(const rtc::Candidate& candidate);

private:
  std::atomic<int> m_relayCandidates{0};
};

} // namespace WEBRTC
