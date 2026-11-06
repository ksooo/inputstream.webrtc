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

  time = clock.ToPresentationTime(123456 + 2 * 90000, 2600000);
  EXPECT_EQ(time.pts, 2500000);
  EXPECT_FALSE(time.catchingUp);

  // Arrives early, but the clock is not catching up any more after a second in time
  time = clock.ToPresentationTime(123456 + 3 * 90000, 3400000);
  EXPECT_EQ(time.pts, 3500000);
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

TEST(RtpClockTest, CatchesUpWithBurstsAfterAFrameInTime)
{
  CRtpClock clock(90000);
  EXPECT_EQ(clock.ToPresentationTime(0, 0).pts, 0);
  // Delayed by a large frame before it
  auto time = clock.ToPresentationTime(3600, 50000);
  EXPECT_EQ(time.pts, 40000);
  EXPECT_FALSE(time.catchingUp);
  // The next burst of buffered frames
  time = clock.ToPresentationTime(20 * 3600, 60000);
  EXPECT_EQ(time.pts, 60000);
  EXPECT_TRUE(time.catchingUp);
}

TEST(RtpClockTest, StartsAnewAfterJumps)
{
  CRtpClock clock(90000);
  // A second in time, so the clock is not catching up any more
  for (int i = 0; i <= 26; ++i)
    EXPECT_EQ(clock.ToPresentationTime(i * 3600, i * 40000).pts, i * 40000);
  // 22 s ahead
  auto time = clock.ToPresentationTime(27 * 3600 + 22 * 90000, 27 * 40000);
  EXPECT_EQ(time.pts, 27 * 40000);
  EXPECT_FALSE(time.catchingUp);
  EXPECT_EQ(clock.ToPresentationTime(28 * 3600 + 22 * 90000, 28 * 40000).pts, 28 * 40000);
  // 5 s back
  EXPECT_EQ(clock.ToPresentationTime(29 * 3600 + 17 * 90000, 29 * 40000).pts, 29 * 40000);
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
