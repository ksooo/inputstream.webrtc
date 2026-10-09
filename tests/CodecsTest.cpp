/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/Codecs.h"

#include <gtest/gtest.h>
#include <rtc/description.hpp>

using namespace WEBRTC;

TEST(CodecsTest, KodiNames)
{
  EXPECT_STREQ(GetKodiCodecName(Codec::H264), "h264");
  EXPECT_STREQ(GetKodiCodecName(Codec::H265), "hevc");
}

TEST(CodecsTest, ReadsCodecsOfMedia)
{
  const rtc::Description description(
      "v=0\r\n"
      "o=- 1 1 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE video audio\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 96 99 100\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=mid:video\r\n"
      "a=sendonly\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=fmtp:96 packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM48gA==\r\n"
      "a=rtpmap:99 H265/90000\r\n"
      "a=rtpmap:100 VP8/90000\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 0.0.0.0\r\n"
      "a=mid:audio\r\n"
      "a=sendonly\r\n"
      "a=rtpmap:111 opus/48000/2\r\n",
      rtc::Description::Type::Answer);

  const auto codecs = GetCodecs(description, "video");
  ASSERT_EQ(codecs.size(), 2u);
  EXPECT_EQ(codecs.at(96).codec, Codec::H264);
  EXPECT_EQ(codecs.at(96).clockRate, 90000u);
  EXPECT_FALSE(codecs.at(96).extraData.empty());
  EXPECT_EQ(codecs.at(99).codec, Codec::H265);
  EXPECT_TRUE(codecs.at(99).extraData.empty());
  EXPECT_TRUE(GetCodecs(description, "audio").empty());
}

TEST(CodecsTest, H264ParameterSets)
{
  const auto extraData =
      GetParameterSets(Codec::H264, {"packetization-mode=1; sprop-parameter-sets=Z0IAHw==,aM4="});
  EXPECT_EQ(extraData,
            (std::vector<uint8_t>{0, 0, 0, 1, 0x67, 0x42, 0x00, 0x1f, 0, 0, 0, 1, 0x68, 0xce}));
}

TEST(CodecsTest, H265ParameterSets)
{
  const auto extraData =
      GetParameterSets(Codec::H265, {"sprop-pps=RAE=;sprop-vps=QAE=;sprop-sps=QgE="});
  EXPECT_EQ(extraData, (std::vector<uint8_t>{0, 0, 0, 1, 0x40, 0x01, 0, 0, 0, 1, 0x42, 0x01, 0, 0,
                                             0, 1, 0x44, 0x01}));
}

TEST(CodecsTest, NoParameterSets)
{
  EXPECT_TRUE(GetParameterSets(Codec::H264, {"packetization-mode=1"}).empty());
  EXPECT_TRUE(GetParameterSets(Codec::H264, {"sprop-parameter-sets=!!"}).empty());
}

TEST(CodecsTest, Keyframes)
{
  const std::vector<uint8_t> h264Idr{0, 0, 0, 1, 0x65, 0x88};
  const std::vector<uint8_t> h264Sps{0, 0, 1, 0x67, 0x42, 0, 0, 0, 1, 0x41, 0x9a};
  const std::vector<uint8_t> h264Slice{0, 0, 0, 1, 0x41, 0x9a};
  EXPECT_TRUE(IsKeyframe(Codec::H264, h264Idr.data(), h264Idr.size()));
  EXPECT_TRUE(IsKeyframe(Codec::H264, h264Sps.data(), h264Sps.size()));
  EXPECT_FALSE(IsKeyframe(Codec::H264, h264Slice.data(), h264Slice.size()));

  const std::vector<uint8_t> h265Vps{0, 0, 0, 1, 0x40, 0x01};
  const std::vector<uint8_t> h265Cra{0, 0, 0, 1, 0x2a, 0x01};
  const std::vector<uint8_t> h265Trail{0, 0, 0, 1, 0x02, 0x01};
  EXPECT_TRUE(IsKeyframe(Codec::H265, h265Vps.data(), h265Vps.size()));
  EXPECT_TRUE(IsKeyframe(Codec::H265, h265Cra.data(), h265Cra.size()));
  EXPECT_FALSE(IsKeyframe(Codec::H265, h265Trail.data(), h265Trail.size()));
}

TEST(CodecsTest, ExtractsH264ParameterSets)
{
  const std::vector<uint8_t> frame{0, 0,    0,    1, 0x67, 0x42, 0x00, 0x1f, 0,    0,
                                   1, 0x68, 0xce, 0, 0,    0,    1,    0x65, 0x88, 0x84};
  EXPECT_EQ(ExtractParameterSets(Codec::H264, frame.data(), frame.size()),
            (std::vector<uint8_t>{0, 0, 0, 1, 0x67, 0x42, 0x00, 0x1f, 0, 0, 0, 1, 0x68, 0xce}));
}

TEST(CodecsTest, ExtractsH265ParameterSets)
{
  const std::vector<uint8_t> frame{0, 0, 0, 1, 0x40, 0x01, 0x0c, 0, 0, 0, 1, 0x42, 0x01, 0x01,
                                   0, 0, 0, 1, 0x44, 0x01, 0xc1, 0, 0, 0, 1, 0x26, 0x01, 0xaf};
  EXPECT_EQ(ExtractParameterSets(Codec::H265, frame.data(), frame.size()),
            (std::vector<uint8_t>{0,    0,    0,    1, 0x40, 0x01, 0x0c, 0,    0,    0,   1,
                                  0x42, 0x01, 0x01, 0, 0,    0,    1,    0x44, 0x01, 0xc1}));
}

TEST(CodecsTest, NoParameterSetsInFrame)
{
  const std::vector<uint8_t> frame{0, 0, 0, 1, 0x41, 0x9a, 0x00};
  EXPECT_TRUE(ExtractParameterSets(Codec::H264, frame.data(), frame.size()).empty());
}
