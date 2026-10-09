/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "StreamBuffer.h"

#include "utils/Log.h"

#include <utility>

namespace WEBRTC
{

CStreamBuffer::CStreamBuffer(size_t maxBytes) : m_maxBytes(maxBytes)
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
    m_bytes += packet.data.size();
    m_packets.emplace_back(std::move(packet));
    size_t dropped = 0;
    while (m_bytes > m_maxBytes && m_packets.size() > 1)
    {
      m_bytes -= m_packets.front().data.size();
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
}

bool CStreamBuffer::WaitForStreams(std::chrono::milliseconds timeout)
{
  std::unique_lock lock(m_mutex);
  m_changed.wait_for(lock, timeout, [this] { return !m_streams.empty() || m_ended; });
  return !m_streams.empty();
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
    packet = std::move(m_packets.front());
    m_packets.pop_front();
    m_bytes -= packet.data.size();
    return Result::PACKET;
  }
  return m_ended ? Result::ENDED : Result::NONE;
}

} // namespace WEBRTC
