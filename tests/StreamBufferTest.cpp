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
  EXPECT_FALSE(buffer.GetStream(1));
  buffer.SetStream({1, Codec::H264, {}});
  EXPECT_TRUE(buffer.GetStream(1));

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

namespace
{

const std::vector<uint8_t> KEYFRAME{0, 0, 0, 1, 0x65, 0x88};
const std::vector<uint8_t> FRAME{0, 0, 0, 1, 0x41, 0x9a};
const std::vector<uint8_t> AUDIO{0xfc};

std::vector<std::pair<int, int64_t>> PopAll(CStreamBuffer& buffer)
{
  std::vector<std::pair<int, int64_t>> packets;
  MediaPacket packet;
  CStreamBuffer::Result result;
  while ((result = buffer.Pop(0ms, packet)) != CStreamBuffer::Result::NONE)
  {
    if (result == CStreamBuffer::Result::PACKET)
      packets.emplace_back(packet.streamId, packet.pts);
  }
  return packets;
}

} // namespace

TEST(StreamBufferTest, SkipsToKeyframeWhenPacketsWaitTooLong)
{
  CStreamBuffer buffer(1024, 0ms);
  buffer.SetStream({1, Codec::H264, {}});
  buffer.SetStream({2, Codec::OPUS, {}});
  buffer.Push({1, KEYFRAME, 1000});
  buffer.Push({2, AUDIO, 1010});
  buffer.Push({1, FRAME, 1040});
  buffer.Push({2, AUDIO, 1030});
  buffer.Push({1, KEYFRAME, 1080});
  buffer.Push({2, AUDIO, 1090});
  buffer.Push({1, FRAME, 1120});

  using Packets = std::vector<std::pair<int, int64_t>>;
  EXPECT_EQ(PopAll(buffer), (Packets{{1, 1000}, {2, 1010}, {1, 1040}}));

  // Without queued video there is nothing to skip; later skips add up
  buffer.Push({2, AUDIO, 1130});
  buffer.Push({1, KEYFRAME, 1160});
  buffer.Push({1, FRAME, 1200});
  buffer.Push({1, KEYFRAME, 1240});
  EXPECT_EQ(PopAll(buffer), (Packets{{1, 1080}}));
}

TEST(StreamBufferTest, KeepsAudioKodiStillNeeds)
{
  CStreamBuffer buffer(1024, 0ms);
  buffer.SetStream({1, Codec::H264, {}});
  buffer.SetStream({2, Codec::OPUS, {}});
  buffer.Push({1, KEYFRAME, 1000});
  buffer.Push({2, AUDIO, 1000});
  PopAll(buffer);

  // Less video time than audio time queued
  buffer.Push({2, AUDIO, 1020});
  buffer.Push({2, AUDIO, 1040});
  buffer.Push({1, FRAME, 1040});
  buffer.Push({2, AUDIO, 1060});
  buffer.Push({1, KEYFRAME, 1050});
  buffer.Push({2, AUDIO, 1080});

  using Packets = std::vector<std::pair<int, int64_t>>;
  EXPECT_EQ(PopAll(buffer), (Packets{{2, 1010}, {2, 1030}, {2, 1050}, {1, 1040}, {2, 1070}}));
}

TEST(StreamBufferTest, DropsAudioKodiAlreadyGot)
{
  CStreamBuffer buffer(1024, 0ms);
  buffer.SetStream({1, Codec::H264, {}});
  buffer.SetStream({2, Codec::OPUS, {}});
  buffer.Push({1, KEYFRAME, 1000});
  buffer.Push({2, AUDIO, 1000});
  PopAll(buffer);

  // More video time than audio time queued
  buffer.Push({2, AUDIO, 1020});
  buffer.Push({1, FRAME, 1040});
  buffer.Push({1, KEYFRAME, 1200});
  buffer.Push({2, AUDIO, 1040});
  buffer.Push({2, AUDIO, 1180});

  using Packets = std::vector<std::pair<int, int64_t>>;
  EXPECT_EQ(PopAll(buffer), (Packets{{1, 1040}, {2, 1020}}));
}

TEST(StreamBufferTest, KeepsPacketsThatWaitShortly)
{
  CStreamBuffer buffer(1024, 1h);
  buffer.SetStream({1, Codec::H264, {}});
  buffer.Push({1, KEYFRAME, 1000});
  buffer.Push({1, FRAME, 1040});
  buffer.Push({1, KEYFRAME, 1080});

  using Packets = std::vector<std::pair<int, int64_t>>;
  EXPECT_EQ(PopAll(buffer), (Packets{{1, 1000}, {1, 1040}, {1, 1080}}));
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
