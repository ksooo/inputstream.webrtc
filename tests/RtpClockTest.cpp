/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/RtpClock.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(RtpClockTest, StartsAtStartTime)
{
  CRtpClock clock(90000, 500000);
  EXPECT_EQ(clock.ToPresentationTime(123456), 500000);
  EXPECT_EQ(clock.ToPresentationTime(123456 + 90000), 1500000);
}

TEST(RtpClockTest, HandlesWrapAround)
{
  CRtpClock clock(90000, 0);
  EXPECT_EQ(clock.ToPresentationTime(0xffffffff - 2999), 0);
  EXPECT_EQ(clock.ToPresentationTime(3000), 66666);
}

TEST(RtpClockTest, TimestampInThePast)
{
  CRtpClock clock(48000, 0);
  EXPECT_EQ(clock.ToPresentationTime(48000), 0);
  EXPECT_EQ(clock.ToPresentationTime(47040), -20000);
}
