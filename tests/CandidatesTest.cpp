/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "session/Candidates.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

namespace
{

rtc::Candidate MakeCandidate(int port, const std::string& type)
{
  return rtc::Candidate("a=candidate:1 1 udp 1 192.0.2.1 " + std::to_string(port) + " typ " + type);
}

} // namespace

TEST(CandidatesTest, RemovesTcpCandidates)
{
  const std::string sdp =
      "v=0\r\n"
      "a=candidate:1 1 udp 2130706431 192.168.1.2 50000 typ host\r\n"
      "a=candidate:2 1 TCP 1671430143 192.168.1.2 18555 typ host tcptype "
      "passive\r\n"
      "a=candidate:3 1 tcp 1671430143 fd00::1 18555 typ host tcptype passive\r\n"
      "a=end-of-candidates\r\n";
  EXPECT_EQ(RemoveTcpCandidates(sdp),
            "v=0\r\n"
            "a=candidate:1 1 udp 2130706431 192.168.1.2 50000 typ host\r\n"
            "a=end-of-candidates\r\n");
}

TEST(CandidatesTest, KeepsDescriptionWithoutFinalLineBreak)
{
  EXPECT_EQ(RemoveTcpCandidates("v=0\na=mid:video"), "v=0\na=mid:video");
}

TEST(CandidatesTest, LimitsRelayCandidates)
{
  CRelayCandidateLimit limit;
  for (int i = 0; i < CRelayCandidateLimit::MAX_RELAY_CANDIDATES; ++i)
    EXPECT_TRUE(limit.Accept(MakeCandidate(10000 + i, "relay")));
  EXPECT_FALSE(limit.Accept(MakeCandidate(20000, "relay")));
  EXPECT_TRUE(limit.Accept(MakeCandidate(30000, "host")));
  EXPECT_TRUE(limit.Accept(MakeCandidate(30001, "srflx")));
}
