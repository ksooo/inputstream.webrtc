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

void CMediaStream::OnFrame(const std::vector<std::byte>& data, int payloadType, uint32_t timestamp)
{
  std::lock_guard lock(m_mutex);

  const auto codec = m_codecs.find(payloadType);
  if (codec == m_codecs.end())
    return;

  if (payloadType != m_payloadType)
  {
    m_payloadType = payloadType;
    m_keyframeReceived = false;
  }

  const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
  if (!m_keyframeReceived)
  {
    if (!IsKeyframe(codec->second.codec, bytes, data.size()))
      return;
    Log(LogLevel::LEVEL_INFO, "Receiving %s", GetKodiCodecName(codec->second.codec));
    m_keyframeReceived = true;
    std::vector<uint8_t> extraData = codec->second.extraData;
    if (extraData.empty())
      extraData = ExtractParameterSets(codec->second.codec, bytes, data.size());
    m_buffer->SetStream({m_streamId, codec->second.codec, std::move(extraData)});
  }

  if (!m_clock)
  {
    const auto start = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - m_sessionStart);
    m_clock.emplace(codec->second.clockRate, start.count());
  }

  m_buffer->Push({m_streamId, std::vector<uint8_t>(bytes, bytes + data.size()),
                  m_clock->ToPresentationTime(timestamp)});
}

} // namespace WEBRTC
