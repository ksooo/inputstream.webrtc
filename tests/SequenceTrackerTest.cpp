/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/SequenceTracker.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(SequenceTrackerTest, CountsLostPackets)
{
  CSequenceTracker tracker;
  EXPECT_EQ(tracker.Update(10), 0u);
  EXPECT_EQ(tracker.Update(11), 0u);
  EXPECT_EQ(tracker.Update(14), 2u);
}

TEST(SequenceTrackerTest, IgnoresReorderedAndRepeated)
{
  CSequenceTracker tracker;
  EXPECT_EQ(tracker.Update(10), 0u);
  EXPECT_EQ(tracker.Update(12), 1u);
  EXPECT_EQ(tracker.Update(11), 0u);
  EXPECT_EQ(tracker.Update(12), 0u);
  EXPECT_EQ(tracker.Update(13), 0u);
}

TEST(SequenceTrackerTest, HandlesWrapAround)
{
  CSequenceTracker tracker;
  EXPECT_EQ(tracker.Update(65534), 0u);
  EXPECT_EQ(tracker.Update(65535), 0u);
  EXPECT_EQ(tracker.Update(0), 0u);
  EXPECT_EQ(tracker.Update(3), 2u);
}
