/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Codecs.h"
#include "PtsSmoother.h"
#include "RtpClock.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace WEBRTC
{

class CStreamBuffer;

/*!
 * \brief Turns the frames of a track into packets for Kodi. The stream is announced with its
 *        first frame. For video, frames before the first keyframe of a codec are dropped; the
 *        stream is announced with that keyframe, and with its parameter sets unless the session
 *        description has them. Kodi needs them to open the stream.
 */
class CMediaStream
{
public:
  /*!
   * \param sessionStart The time all streams of the session count their presentation time from.
   */
  CMediaStream(int streamId,
               std::shared_ptr<CStreamBuffer> buffer,
               std::chrono::steady_clock::time_point sessionStart);

  void SetCodecs(std::map<int, CodecInfo> codecs);
  void OnFrame(const std::vector<std::byte>& data,
               int payloadType,
               uint32_t timestamp,
               std::chrono::steady_clock::time_point arrival);

private:
  const int m_streamId;
  const std::shared_ptr<CStreamBuffer> m_buffer;
  const std::chrono::steady_clock::time_point m_sessionStart;
  std::mutex m_mutex;
  std::map<int, CodecInfo> m_codecs;
  std::optional<int> m_payloadType;
  bool m_announced{false};
  std::optional<CRtpClock> m_clock;
  CPtsSmoother m_smoother; // video only
};

} // namespace WEBRTC
