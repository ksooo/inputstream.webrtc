/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "stream/Codecs.h"
#include "stream/SequenceTracker.h"

#include <chrono>
#include <map>
#include <memory>
#include <mutex>

#include <rtc/mediahandler.hpp>

namespace WEBRTC
{

/*!
 * \brief Assembles the frames of a track with the depacketizer for the codec of each packet,
 *        and requests a keyframe when video packets were lost. Belongs in front of the
 *        RtcpReceivingSession in the chain of handlers, which receives first and sends the request.
 */
class CRtpReceiver : public rtc::MediaHandler
{
public:
  void SetCodecs(const std::map<int, CodecInfo>& codecs);

  void incoming(rtc::message_vector& messages, const rtc::message_callback& send) override;

private:
  std::shared_ptr<rtc::MediaHandler> GetDepacketizer(int payloadType);
  void RequestKeyframe(const rtc::message_callback& send);

  std::mutex m_mutex;
  std::map<int, Codec> m_codecs;
  std::map<int, std::shared_ptr<rtc::MediaHandler>> m_depacketizers;
  CSequenceTracker m_sequence;
  std::chrono::steady_clock::time_point m_lastKeyframeRequest;
};

} // namespace WEBRTC
