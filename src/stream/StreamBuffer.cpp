/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "StreamBuffer.h"

#include "utils/Log.h"

#include <algorithm>
#include <utility>

namespace WEBRTC
{

namespace
{

constexpr std::chrono::seconds STATISTICS_INTERVAL{5};

} // namespace

CStreamBuffer::CStreamBuffer(size_t maxBytes, std::chrono::milliseconds maxLatency)
  : m_maxBytes(maxBytes),
    m_maxLatency(maxLatency)
{
}

void CStreamBuffer::SetStream(StreamInfo info)
{
  {
    std::lock_guard lock(m_mutex);
    const int id = info.id;
    m_streams.insert_or_assign(id, std::move(info));
    m_streamsChanged = true;
  }
  m_changed.notify_all();
}

void CStreamBuffer::Push(MediaPacket packet)
{
  {
    std::lock_guard lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();
    if (!m_packets.empty() && now - m_packets.front().queued >= m_maxLatency &&
        IsVideoKeyframe(packet))
      SkipToKeyframe(packet.pts - m_ptsShift);
    packet.pts -= m_ptsShift;
    if (!IsVideoStream(packet.streamId))
    {
      const auto [last, inserted] = m_lastPts.try_emplace(packet.streamId, packet.pts);
      if (!inserted)
      {
        if (packet.pts <= last->second)
          return;
        last->second = packet.pts;
      }
    }
    m_bytes += packet.data.size();
    m_packets.push_back({std::move(packet), now});
    size_t dropped = 0;
    while (m_bytes > m_maxBytes && m_packets.size() > 1)
    {
      m_bytes -= m_packets.front().packet.data.size();
      m_packets.pop_front();
      ++dropped;
    }
    if (dropped > 0)
      Log(LogLevel::LEVEL_WARNING, "Dropped %zu packets that Kodi did not read in time", dropped);
  }
  m_changed.notify_all();
}

void CStreamBuffer::End()
{
  {
    std::lock_guard lock(m_mutex);
    m_ended = true;
  }
  m_changed.notify_all();
}

void CStreamBuffer::Abort()
{
  {
    std::lock_guard lock(m_mutex);
    m_aborted = true;
  }
  m_changed.notify_all();
}

void CStreamBuffer::Flush()
{
  std::lock_guard lock(m_mutex);
  m_packets.clear();
  m_bytes = 0;
  m_aborted = false;
  m_poppedPts.clear();
  m_lastPts.clear();
}

std::vector<StreamInfo> CStreamBuffer::GetStreams()
{
  std::lock_guard lock(m_mutex);
  m_streamsChanged = false;
  std::vector<StreamInfo> streams;
  for (const auto& [id, info] : m_streams)
    streams.emplace_back(info);
  return streams;
}

std::optional<StreamInfo> CStreamBuffer::GetStream(int id) const
{
  std::lock_guard lock(m_mutex);
  const auto it = m_streams.find(id);
  if (it == m_streams.end())
    return {};
  return it->second;
}

CStreamBuffer::Result CStreamBuffer::Pop(std::chrono::milliseconds timeout, MediaPacket& packet)
{
  std::unique_lock lock(m_mutex);
  m_changed.wait_for(lock, timeout, [this]
                     { return m_aborted || m_streamsChanged || !m_packets.empty() || m_ended; });

  if (m_streamsChanged)
  {
    m_streamsChanged = false;
    return Result::STREAMS_CHANGED;
  }
  if (!m_packets.empty())
  {
    const auto queued = m_packets.front().queued;
    packet = std::move(m_packets.front().packet);
    m_packets.pop_front();
    m_bytes -= packet.data.size();
    m_poppedPts.insert_or_assign(packet.streamId, packet.pts);
    UpdateStatistics(queued);
    return Result::PACKET;
  }
  return m_ended ? Result::ENDED : Result::NONE;
}

bool CStreamBuffer::IsVideoStream(int id) const
{
  const auto it = m_streams.find(id);
  return it != m_streams.end() && IsVideo(it->second.codec);
}

bool CStreamBuffer::IsVideoKeyframe(const MediaPacket& packet) const
{
  const auto it = m_streams.find(packet.streamId);
  return it != m_streams.end() && IsVideo(it->second.codec) &&
         IsKeyframe(it->second.codec, packet.data.data(), packet.data.size());
}

void CStreamBuffer::SkipToKeyframe(int64_t pts)
{
  std::optional<int64_t> first;
  for (const auto& queued : m_packets)
  {
    if (IsVideoStream(queued.packet.streamId))
      first = std::min(first.value_or(queued.packet.pts), queued.packet.pts);
  }
  if (!first || pts <= *first)
    return;

  const int64_t skipped = pts - *first;
  Log(LogLevel::LEVEL_DEBUG, "Skipped %lld ms to catch up with the live stream",
      static_cast<long long>(skipped / 1000));
  m_ptsShift += skipped;

  // The queued audio rarely spans the skipped video time, and a gap would stall Kodi's audio
  for (const auto& [id, info] : m_streams)
  {
    if (IsVideo(info.codec))
      continue;
    const auto popped = m_poppedPts.find(id);
    m_lastPts.insert_or_assign(id, popped != m_poppedPts.end() ? popped->second : *first - 1);
  }
  std::deque<QueuedPacket> kept;
  m_bytes = 0;
  for (auto& queued : m_packets)
  {
    MediaPacket& packet = queued.packet;
    if (IsVideoStream(packet.streamId))
      continue;
    packet.pts -= skipped;
    int64_t& last = m_lastPts[packet.streamId];
    if (packet.pts <= last)
      continue;
    last = packet.pts;
    m_bytes += packet.data.size();
    kept.emplace_back(std::move(queued));
  }
  m_packets = std::move(kept);
}

void CStreamBuffer::UpdateStatistics(std::chrono::steady_clock::time_point queued)
{
  const auto now = std::chrono::steady_clock::now();
  const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(now - queued);
  ++m_readPackets;
  m_totalWait += wait;
  m_maxWait = std::max(m_maxWait, wait);

  if (now - m_statisticsStart < STATISTICS_INTERVAL)
    return;
  Log(LogLevel::LEVEL_DEBUG,
      "Kodi read %u packets in %lld s, waiting %lld ms on average and at most %lld ms",
      m_readPackets,
      static_cast<long long>(
          std::chrono::duration_cast<std::chrono::seconds>(now - m_statisticsStart).count()),
      static_cast<long long>(m_totalWait.count() / m_readPackets / 1000),
      static_cast<long long>(m_maxWait.count() / 1000));
  m_statisticsStart = now;
  m_readPackets = 0;
  m_totalWait = {};
  m_maxWait = {};
}

} // namespace WEBRTC
