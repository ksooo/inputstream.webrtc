/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "StreamProperties.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(StreamPropertiesTest, Defaults)
{
  const auto properties = ParseStreamProperties({});
  ASSERT_TRUE(properties);
  EXPECT_EQ(properties->signaling, SignalingType::WHEP);
  EXPECT_TRUE(properties->audio);
  EXPECT_TRUE(properties->iceServers.empty());
  EXPECT_TRUE(properties->bearerToken.empty());
}

TEST(StreamPropertiesTest, HomeAssistant)
{
  const auto properties = ParseStreamProperties({{"inputstream.webrtc.signaling", "homeassistant"},
                                                 {"inputstream.webrtc.entity_id", "camera.garden"},
                                                 {"inputstream.webrtc.bearer_token", "token"},
                                                 {"inputstream.webrtc.audio", "false"}});
  ASSERT_TRUE(properties);
  EXPECT_EQ(properties->signaling, SignalingType::HOME_ASSISTANT);
  EXPECT_EQ(properties->entityId, "camera.garden");
  EXPECT_EQ(properties->bearerToken, "token");
  EXPECT_FALSE(properties->audio);
}

TEST(StreamPropertiesTest, HomeAssistantNeedsEntityAndToken)
{
  EXPECT_FALSE(ParseStreamProperties({{"inputstream.webrtc.signaling", "homeassistant"},
                                      {"inputstream.webrtc.bearer_token", "token"}}));
  EXPECT_FALSE(ParseStreamProperties({{"inputstream.webrtc.signaling", "homeassistant"},
                                      {"inputstream.webrtc.entity_id", "camera.garden"}}));
}

TEST(StreamPropertiesTest, IceServers)
{
  const auto properties = ParseStreamProperties(
      {{"inputstream.webrtc.ice_servers", " stun:a:3478 ,, turn:user:pass@b:3478 "}});
  ASSERT_TRUE(properties);
  EXPECT_EQ(properties->iceServers,
            (std::vector<std::string>{"stun:a:3478", "turn:user:pass@b:3478"}));
}

TEST(StreamPropertiesTest, RejectsInvalidValues)
{
  EXPECT_FALSE(ParseStreamProperties({{"inputstream.webrtc.signaling", "janus"}}));
  EXPECT_FALSE(ParseStreamProperties({{"inputstream.webrtc.audio", "no"}}));
}
