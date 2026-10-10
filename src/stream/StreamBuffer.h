/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "Codecs.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace WEBRTC
{

struct StreamInfo
{
  int id;
  Codec codec;
  std::vector<uint8_t> extraData;
  uint32_t sampleRate{0}; // audio only
  unsigned int channels{0}; // audio only
};

struct MediaPacket
{
  int streamId;
  std::vector<uint8_t> data;
  int64_t pts; // microseconds
};

/*!
 * \brief Hands the packets of all streams from the receiving threads to Kodi.
 */
class CStreamBuffer
{
public:
  enum class Result
  {
    PACKET,
    NONE,
    STREAMS_CHANGED,
    ENDED
  };

  explicit CStreamBuffer(size_t maxBytes = 16 * 1024 * 1024);

  /*!
   * \brief Adds a stream or replaces the one with the same id.
   */
  void SetStream(StreamInfo info);

  /*!
   * \brief Queues a packet; drops the oldest ones while the queue is full.
   */
  void Push(MediaPacket packet);

  /*!
   * \brief Marks the end of the session; Pop() reports it once the queue is empty.
   */
  void End();

  /*!
   * \brief Lets a waiting Pop() return immediately, also all following ones until Flush().
   */
  void Abort();
  void Flush();

  /*!
   * \brief The streams; also takes note that Kodi knows about them.
   */
  std::vector<StreamInfo> GetStreams();
  std::optional<StreamInfo> GetStream(int id) const;

  Result Pop(std::chrono::milliseconds timeout, MediaPacket& packet);

private:
  struct QueuedPacket
  {
    MediaPacket packet;
    std::chrono::steady_clock::time_point queued;
  };

  void UpdateStatistics(std::chrono::steady_clock::time_point queued);

  const size_t m_maxBytes;
  mutable std::mutex m_mutex;
  std::condition_variable m_changed;
  std::map<int, StreamInfo> m_streams;
  std::deque<QueuedPacket> m_packets;
  size_t m_bytes{0};
  bool m_streamsChanged{false};
  bool m_ended{false};
  bool m_aborted{false};
  std::chrono::steady_clock::time_point m_statisticsStart{std::chrono::steady_clock::now()};
  unsigned int m_readPackets{0};
  std::chrono::microseconds m_totalWait{0};
  std::chrono::microseconds m_maxWait{0};
};

} // namespace WEBRTC
