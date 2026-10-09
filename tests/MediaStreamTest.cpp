/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/MediaStream.h"
#include "stream/StreamBuffer.h"

#include <gtest/gtest.h>

using namespace WEBRTC;
using namespace std::chrono_literals;

namespace
{

std::vector<std::byte> MakeFrame(uint8_t value)
{
  return {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{value}};
}

} // namespace

TEST(MediaStreamTest, AnnouncesStreamWithFirstFrame)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{96, {Codec::H264, 90000, {0, 0, 0, 1, 0x67}}}});

  EXPECT_FALSE(buffer->WaitForStreams(0ms));
  stream.OnFrame(MakeFrame(0x65), 96, 1000);
  stream.OnFrame(MakeFrame(0x41), 96, 1000 + 9000);

  const auto streams = buffer->GetStreams();
  ASSERT_EQ(streams.size(), 1u);
  EXPECT_EQ(streams[0].id, 1);
  EXPECT_EQ(streams[0].codec, Codec::H264);
  EXPECT_EQ(streams[0].extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x67}));

  MediaPacket first;
  MediaPacket second;
  ASSERT_EQ(buffer->Pop(0ms, first), CStreamBuffer::Result::PACKET);
  ASSERT_EQ(buffer->Pop(0ms, second), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(first.data, (std::vector<uint8_t>{0, 0, 0, 1, 0x65}));
  EXPECT_GE(first.pts, 0);
  EXPECT_EQ(second.pts - first.pts, 100000);
}

TEST(MediaStreamTest, IgnoresUnknownPayloadType)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x65), 100, 1000);
  EXPECT_FALSE(buffer->WaitForStreams(0ms));
}

TEST(MediaStreamTest, CodecChangeAnnouncesStreamAgain)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}, {99, {Codec::H265, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x65), 96, 1000);
  buffer->GetStreams();
  stream.OnFrame(MakeFrame(0x26), 99, 4000);

  MediaPacket packet;
  EXPECT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::STREAMS_CHANGED);
  EXPECT_EQ(buffer->GetStream(1)->codec, Codec::H265);
}

TEST(MediaStreamTest, DropsFramesBeforeKeyframe)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x41), 96, 1000);
  EXPECT_FALSE(buffer->WaitForStreams(0ms));

  stream.OnFrame(MakeFrame(0x65), 96, 4000);
  EXPECT_TRUE(buffer->WaitForStreams(0ms));
  buffer->GetStreams();

  MediaPacket packet;
  ASSERT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(packet.data, (std::vector<uint8_t>{0, 0, 0, 1, 0x65}));
  EXPECT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::NONE);
}

TEST(MediaStreamTest, ParameterSetsFromKeyframe)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{99, {Codec::H265, 90000, {}}}});

  const std::vector<std::byte> keyframe{
      std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x40}, std::byte{0x01},
      std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x26}, std::byte{0x01}};
  stream.OnFrame(keyframe, 99, 1000);

  EXPECT_EQ(buffer->GetStream(1)->extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x40, 0x01}));
}

TEST(MediaStreamTest, ParameterSetsFromDescriptionFirst)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, std::chrono::steady_clock::now());
  stream.SetCodecs({{96, {Codec::H264, 90000, {0, 0, 0, 1, 0x67, 0x64}}}});

  const std::vector<std::byte> keyframe{
      std::byte{0},    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x67},
      std::byte{0x42}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x65}};
  stream.OnFrame(keyframe, 96, 1000);

  EXPECT_EQ(buffer->GetStream(1)->extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x67, 0x64}));
}
