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

const std::chrono::steady_clock::time_point START;

std::chrono::steady_clock::time_point At(int milliseconds)
{
  return START + std::chrono::milliseconds(milliseconds);
}

std::vector<std::byte> MakeFrame(uint8_t value)
{
  return {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{value}};
}

} // namespace

TEST(MediaStreamTest, AnnouncesStreamWithFirstFrame)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {0, 0, 0, 1, 0x67}}}});

  EXPECT_FALSE(buffer->WaitForVideo(0ms));
  stream.OnFrame(MakeFrame(0x65), 96, 1000, At(0));
  stream.OnFrame(MakeFrame(0x41), 96, 1000 + 9000, At(100));

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
  EXPECT_EQ(first.pts, 0);
  EXPECT_EQ(second.pts, 100000);
}

TEST(MediaStreamTest, IgnoresUnknownPayloadType)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x65), 100, 1000, At(0));
  EXPECT_FALSE(buffer->WaitForVideo(0ms));
}

TEST(MediaStreamTest, CodecChangeAnnouncesStreamAgain)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}, {99, {Codec::H265, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x65), 96, 1000, At(0));
  buffer->GetStreams();
  stream.OnFrame(MakeFrame(0x26), 99, 4000, At(33));

  MediaPacket packet;
  EXPECT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::STREAMS_CHANGED);
  EXPECT_EQ(buffer->GetStream(1)->codec, Codec::H265);
}

TEST(MediaStreamTest, DropsFramesBeforeKeyframe)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x41), 96, 1000, At(0));
  EXPECT_FALSE(buffer->WaitForVideo(0ms));

  stream.OnFrame(MakeFrame(0x65), 96, 4000, At(33));
  EXPECT_TRUE(buffer->WaitForVideo(0ms));
  buffer->GetStreams();

  MediaPacket packet;
  ASSERT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(packet.data, (std::vector<uint8_t>{0, 0, 0, 1, 0x65}));
  EXPECT_EQ(buffer->Pop(0ms, packet), CStreamBuffer::Result::NONE);
}

TEST(MediaStreamTest, ParameterSetsFromKeyframe)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{99, {Codec::H265, 90000, {}}}});

  const std::vector<std::byte> keyframe{
      std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x40}, std::byte{0x01},
      std::byte{0}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x26}, std::byte{0x01}};
  stream.OnFrame(keyframe, 99, 1000, At(0));

  EXPECT_EQ(buffer->GetStream(1)->extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x40, 0x01}));
}

TEST(MediaStreamTest, ParameterSetsFromDescriptionFirst)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {0, 0, 0, 1, 0x67, 0x64}}}});

  const std::vector<std::byte> keyframe{
      std::byte{0},    std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x67},
      std::byte{0x42}, std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0x65}};
  stream.OnFrame(keyframe, 96, 1000, At(0));

  EXPECT_EQ(buffer->GetStream(1)->extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x67, 0x64}));
}

TEST(MediaStreamTest, AnnouncesAudioWithFirstFrame)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(2, buffer, START);
  stream.SetCodecs({{0, {Codec::PCMU, 8000, {}, 1}}});

  const std::vector<std::byte> frame(160, std::byte{0xff});
  stream.OnFrame(frame, 0, 0, At(0));
  stream.OnFrame(frame, 0, 160, At(20));

  const auto info = buffer->GetStream(2);
  ASSERT_TRUE(info);
  EXPECT_EQ(info->codec, Codec::PCMU);
  EXPECT_EQ(info->sampleRate, 8000u);
  EXPECT_EQ(info->channels, 1u);
  EXPECT_TRUE(info->extraData.empty());

  buffer->GetStreams();
  MediaPacket first;
  MediaPacket second;
  ASSERT_EQ(buffer->Pop(0ms, first), CStreamBuffer::Result::PACKET);
  ASSERT_EQ(buffer->Pop(0ms, second), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(second.pts - first.pts, 20000);
}

TEST(MediaStreamTest, DropsBufferedAudio)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(2, buffer, START);
  stream.SetCodecs({{111, {Codec::OPUS, 48000, {}, 2}}});

  // Three buffered frames of 20 ms arrive at once, then one in time
  const std::vector<std::byte> frame(10, std::byte{0xfc});
  stream.OnFrame(frame, 111, 0, At(0));
  stream.OnFrame(frame, 111, 960, At(1));
  stream.OnFrame(frame, 111, 1920, At(2));
  stream.OnFrame(frame, 111, 2880, At(22));

  buffer->GetStreams();
  MediaPacket first;
  MediaPacket second;
  ASSERT_EQ(buffer->Pop(0ms, first), CStreamBuffer::Result::PACKET);
  ASSERT_EQ(buffer->Pop(0ms, second), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(first.pts, 0);
  EXPECT_EQ(second.pts, 22000);
  EXPECT_EQ(buffer->Pop(0ms, first), CStreamBuffer::Result::NONE);
}

TEST(MediaStreamTest, KeepsBufferedVideo)
{
  auto buffer = std::make_shared<CStreamBuffer>();
  CMediaStream stream(1, buffer, START);
  stream.SetCodecs({{96, {Codec::H264, 90000, {}}}});

  stream.OnFrame(MakeFrame(0x65), 96, 0, At(0));
  stream.OnFrame(MakeFrame(0x41), 96, 3000, At(1));

  buffer->GetStreams();
  MediaPacket first;
  MediaPacket second;
  ASSERT_EQ(buffer->Pop(0ms, first), CStreamBuffer::Result::PACKET);
  ASSERT_EQ(buffer->Pop(0ms, second), CStreamBuffer::Result::PACKET);
  EXPECT_EQ(second.pts, 1000);
}
