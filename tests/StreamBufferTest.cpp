/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/StreamBuffer.h"

#include <thread>

#include <gtest/gtest.h>

using namespace WEBRTC;
using namespace std::chrono_literals;

TEST(StreamBufferTest, AnnouncesStreamsOnce)
{
  CStreamBuffer buffer;
  EXPECT_FALSE(buffer.WaitForStreams(0ms));
  buffer.SetStream({1, Codec::H264, {}});
  EXPECT_TRUE(buffer.WaitForStreams(0ms));

  MediaPacket packet;
  EXPECT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::STREAMS_CHANGED);
  EXPECT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::NONE);

  buffer.SetStream({1, Codec::H265, {}});
  EXPECT_EQ(buffer.GetStreams().size(), 1u);
  EXPECT_EQ(buffer.GetStream(1)->codec, Codec::H265);
  // Kodi already asked for the streams
  EXPECT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::NONE);
}

TEST(StreamBufferTest, PacketsInOrderThenEnd)
{
  CStreamBuffer buffer;
  buffer.Push({1, {1}, 10});
  buffer.Push({1, {2}, 20});
  buffer.End();

  MediaPacket packet;
  ASSERT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(packet.pts, 10);
  ASSERT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(packet.pts, 20);
  EXPECT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::ENDED);
}

TEST(StreamBufferTest, DropsOldestWhenFull)
{
  CStreamBuffer buffer(4);
  buffer.Push({1, {1, 1}, 10});
  buffer.Push({1, {2, 2}, 20});
  buffer.Push({1, {3, 3}, 30});

  MediaPacket packet;
  ASSERT_EQ(buffer.Pop(0ms, packet), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(packet.pts, 20);
}

TEST(StreamBufferTest, AbortWakesWaitingPop)
{
  CStreamBuffer buffer;
  std::thread abort(
      [&buffer]
      {
        std::this_thread::sleep_for(50ms);
        buffer.Abort();
      });

  MediaPacket packet;
  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(buffer.Pop(5s, packet), CStreamBuffer::Result::NONE);
  EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
  abort.join();

  buffer.Flush();
  EXPECT_EQ(buffer.Pop(10ms, packet), CStreamBuffer::Result::NONE);
}
