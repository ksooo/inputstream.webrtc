/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtpReceiver.h"

#include "utils/Log.h"

#include <rtc/h264rtpdepacketizer.hpp>
#include <rtc/h265rtpdepacketizer.hpp>
#include <rtc/rtp.hpp>

namespace WEBRTC
{

namespace
{

constexpr auto MIN_KEYFRAME_REQUEST_INTERVAL = std::chrono::seconds(1);

} // namespace

void CRtpReceiver::SetCodecs(const std::map<int, CodecInfo>& codecs)
{
  std::lock_guard lock(m_mutex);
  m_codecs.clear();
  for (const auto& [payloadType, info] : codecs)
    m_codecs.emplace(payloadType, info.codec);
}

std::shared_ptr<rtc::MediaHandler> CRtpReceiver::GetDepacketizer(int payloadType)
{
  if (const auto it = m_depacketizers.find(payloadType); it != m_depacketizers.end())
    return it->second;

  const auto codec = m_codecs.find(payloadType);
  if (codec == m_codecs.end())
    return {};

  std::shared_ptr<rtc::MediaHandler> depacketizer;
  switch (codec->second)
  {
    case Codec::H264:
      depacketizer = std::make_shared<rtc::H264RtpDepacketizer>();
      break;
    case Codec::H265:
      depacketizer = std::make_shared<rtc::H265RtpDepacketizer>();
      break;
  }
  m_depacketizers.emplace(payloadType, depacketizer);
  return depacketizer;
}

void CRtpReceiver::RequestKeyframe(const rtc::message_callback& send)
{
  const auto now = std::chrono::steady_clock::now();
  if (now - m_lastKeyframeRequest < MIN_KEYFRAME_REQUEST_INTERVAL)
    return;
  m_lastKeyframeRequest = now;
  requestKeyframe(send);
}

void CRtpReceiver::incoming(rtc::message_vector& messages, const rtc::message_callback& send)
{
  std::lock_guard lock(m_mutex);

  rtc::message_vector frames;
  for (auto& message : messages)
  {
    if (message->type != rtc::Message::Binary || message->size() < sizeof(rtc::RtpHeader))
      continue;

    const auto* header = reinterpret_cast<const rtc::RtpHeader*>(message->data());
    if (const unsigned int lost = m_sequence.Update(header->seqNumber()); lost > 0)
    {
      Log(LogLevel::LEVEL_DEBUG, "Lost %u video packets", lost);
      RequestKeyframe(send);
    }

    const auto depacketizer = GetDepacketizer(header->payloadType());
    if (!depacketizer)
      continue;

    rtc::message_vector packet{std::move(message)};
    depacketizer->incoming(packet, send);
    for (auto& frame : packet)
      frames.emplace_back(std::move(frame));
  }
  messages.swap(frames);
}

} // namespace WEBRTC
