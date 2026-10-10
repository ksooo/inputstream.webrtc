/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "MediaStream.h"

#include "StreamBuffer.h"
#include "utils/Log.h"

#include <utility>

namespace WEBRTC
{

CMediaStream::CMediaStream(int streamId,
                           std::shared_ptr<CStreamBuffer> buffer,
                           std::chrono::steady_clock::time_point sessionStart)
  : m_streamId(streamId),
    m_buffer(std::move(buffer)),
    m_sessionStart(sessionStart)
{
}

void CMediaStream::SetCodecs(std::map<int, CodecInfo> codecs)
{
  std::lock_guard lock(m_mutex);
  m_codecs = std::move(codecs);
}

void CMediaStream::OnFrame(const std::vector<std::byte>& data,
                           int payloadType,
                           uint32_t timestamp,
                           std::chrono::steady_clock::time_point arrival)
{
  std::lock_guard lock(m_mutex);

  const auto codec = m_codecs.find(payloadType);
  if (codec == m_codecs.end())
    return;

  if (payloadType != m_payloadType)
  {
    m_payloadType = payloadType;
    m_announced = false;
  }

  const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
  if (!m_announced)
  {
    const CodecInfo& info = codec->second;
    StreamInfo stream{m_streamId, info.codec, info.extraData};
    if (IsVideo(info.codec))
    {
      if (!IsKeyframe(info.codec, bytes, data.size()))
        return;
      if (stream.extraData.empty())
        stream.extraData = ExtractParameterSets(info.codec, bytes, data.size());
    }
    else
    {
      stream.sampleRate = info.clockRate;
      stream.channels = info.channels;
    }
    Log(LogLevel::LEVEL_INFO, "Receiving %s", GetKodiCodecName(info.codec));
    m_announced = true;
    m_buffer->SetStream(std::move(stream));
  }

  if (!m_clock)
    m_clock.emplace(codec->second.clockRate);

  const int64_t sinceStart =
      std::chrono::duration_cast<std::chrono::microseconds>(arrival - m_sessionStart).count();
  const auto time = m_clock->ToPresentationTime(timestamp, sinceStart);
  // Buffered audio is of no use; video frames are needed to decode the following ones
  if (time.catchingUp && !IsVideo(codec->second.codec))
    return;

  const int64_t pts =
      IsVideo(codec->second.codec) && !time.catchingUp ? m_smoother.Smooth(time.pts) : time.pts;
  m_buffer->Push({m_streamId, std::vector<uint8_t>(bytes, bytes + data.size()), pts});
}

} // namespace WEBRTC
