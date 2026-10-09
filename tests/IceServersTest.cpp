/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "session/IceServers.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(IceServersTest, ParsesStunAndTurn)
{
  const auto servers = ParseIceServers(
      {"stun:stun.example.com:3478", "turn:user:pass@turn.example.com:3479?transport=udp"});
  ASSERT_EQ(servers.size(), 2u);
  EXPECT_EQ(servers[0].type, rtc::IceServer::Type::Stun);
  EXPECT_EQ(servers[0].hostname, "stun.example.com");
  EXPECT_EQ(servers[0].port, 3478);
  EXPECT_EQ(servers[1].type, rtc::IceServer::Type::Turn);
  EXPECT_EQ(servers[1].hostname, "turn.example.com");
  EXPECT_EQ(servers[1].port, 3479);
  EXPECT_EQ(servers[1].username, "user");
  EXPECT_EQ(servers[1].password, "pass");
}

TEST(IceServersTest, SkipsInvalidUrls)
{
  const auto servers = ParseIceServers({"http://example.com", "stun:", "stun:host:3478"});
  ASSERT_EQ(servers.size(), 1u);
  EXPECT_EQ(servers[0].hostname, "host");
}
