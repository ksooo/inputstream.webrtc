/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <rtc/candidate.hpp>
#include <rtc/configuration.hpp>

namespace WEBRTC
{

/*!
 * \brief Exchanges the session description with the remote peer.
 */
class ISignaling
{
public:
  using CandidateCallback = std::function<void(rtc::Candidate candidate)>;

  virtual ~ISignaling() = default;

  /*!
   * \brief The ICE servers announced by the remote side, needed before the offer is created.
   */
  virtual std::vector<rtc::IceServer> GetIceServers() { return {}; }

  /*!
   * \brief Sends the offer and waits for the answer.
   * \param onCandidate Called from any thread with remote candidates that are sent separately,
   *                    also after the answer; until Close().
   * \return The answer, or nothing on failure.
   */
  virtual std::optional<std::string> Offer(const std::string& sdp,
                                           CandidateCallback onCandidate) = 0;

  /*!
   * \brief Ends the session at the remote side.
   */
  virtual void Close() = 0;
};

} // namespace WEBRTC
