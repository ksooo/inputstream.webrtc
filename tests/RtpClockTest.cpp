/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/RtpClock.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(RtpClockTest, FollowsTimestampsWhenInTime)
{
  CRtpClock clock(90000);
  auto time = clock.ToPresentationTime(123456, 500000);
  EXPECT_EQ(time.pts, 500000);
  EXPECT_FALSE(time.catchingUp);

  // Arrives late, the timestamp rules
  time = clock.ToPresentationTime(123456 + 90000, 1600000);
  EXPECT_EQ(time.pts, 1500000);
  EXPECT_FALSE(time.catchingUp);

  // Arrives early, but the clock is not catching up any more
  time = clock.ToPresentationTime(123456 + 2 * 90000, 2400000);
  EXPECT_EQ(time.pts, 2500000);
  EXPECT_FALSE(time.catchingUp);
}

TEST(RtpClockTest, CatchesUpWithBufferedFrames)
{
  CRtpClock clock(90000);
  // 30 frames of 33 ms, buffered by the source, arrive within 10 ms
  for (int i = 0; i < 30; ++i)
  {
    const auto time = clock.ToPresentationTime(i * 3000, i * 333);
    EXPECT_EQ(time.pts, i * 333);
    EXPECT_EQ(time.catchingUp, i > 0);
  }
  // Then the frames arrive in real time, 1 ms later than their timestamps suggest
  for (int i = 30; i < 40; ++i)
  {
    const int64_t arrival = 29 * 333 + (i - 29) * 33333 + 1000;
    const auto time = clock.ToPresentationTime(i * 3000, arrival);
    EXPECT_NEAR(time.pts, arrival - 1000, 100);
    EXPECT_FALSE(time.catchingUp);
  }
}

TEST(RtpClockTest, HandlesWrapAround)
{
  CRtpClock clock(90000);
  EXPECT_EQ(clock.ToPresentationTime(0xffffffff - 2999, 0).pts, 0);
  EXPECT_EQ(clock.ToPresentationTime(3000, 70000).pts, 66666);
}

TEST(RtpClockTest, TimestampInThePast)
{
  CRtpClock clock(48000);
  EXPECT_EQ(clock.ToPresentationTime(48000, 0).pts, 0);
  EXPECT_EQ(clock.ToPresentationTime(47040, 10000).pts, -20000);
}
