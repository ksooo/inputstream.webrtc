/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/PtsSmoother.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

using namespace WEBRTC;

namespace
{

constexpr int64_t FRAME = 64000;
constexpr int64_t MEASURE_TIME = 4000000;

// Frame intervals of a camera in ms, with the hiccups around its keyframes
const std::vector<int64_t> CAMERA_INTERVALS{
    95,  33, 64, 95, 38, 72, 99, 32, 72, 107, 280, 75, 3,  7,  3,   1,  65, 63, 98, 34,
    70,  65, 63, 64, 94, 33, 92, 65, 62, 63,  93,  38, 62, 93, 35,  68, 98, 34, 61, 128,
    309, 91, 2,  3,  1,  2,  33, 62, 96, 32,  65,  92, 35, 63, 102, 35, 66, 93, 32, 65};

} // namespace

TEST(PtsSmootherTest, PassesTimesThroughWhileMeasuring)
{
  CPtsSmoother smoother;
  const int64_t start = 1000000;
  int64_t pts = start;
  for (size_t i = 0; pts - start < MEASURE_TIME; ++i)
  {
    EXPECT_EQ(smoother.Smooth(pts), pts);
    pts += CAMERA_INTERVALS[i % CAMERA_INTERVALS.size()] * 1000;
  }
}

TEST(PtsSmootherTest, EvensOutIrregularTimes)
{
  CPtsSmoother smoother;
  int64_t sum = 0;
  for (const int64_t interval : CAMERA_INTERVALS)
    sum += interval * 1000;
  const double duration = static_cast<double>(sum) / CAMERA_INTERVALS.size();

  const int64_t start = 1000000;
  int64_t pts = start;
  int64_t previous = 0;
  for (size_t i = 0; i < 20 * CAMERA_INTERVALS.size(); ++i)
  {
    const int64_t smoothed = smoother.Smooth(pts);
    if (pts - start > MEASURE_TIME + 200000)
    {
      EXPECT_NEAR(smoothed - previous, duration, 1000) << "frame " << i;
      // The camera delays frames around its keyframes by up to 300 ms
      EXPECT_LT(std::abs(smoothed - pts), 350000) << "frame " << i;
    }
    previous = smoothed;
    pts += CAMERA_INTERVALS[i % CAMERA_INTERVALS.size()] * 1000;
  }
}

TEST(PtsSmootherTest, IgnoresKeyframeHiccupsAtHigherFrameRates)
{
  // 30 fps; around each keyframe, every 2 s, frames are delayed by up to 300 ms
  constexpr int64_t duration = 33333;
  CPtsSmoother smoother;
  int64_t previousPts = -1;
  int64_t previous = 0;
  for (int64_t i = 0; i < 1800; ++i)
  {
    const int64_t delay = std::max<int64_t>(0, 300000 - (i % 60) * duration);
    const int64_t pts = std::max(i * duration + delay, previousPts + 1000);
    const int64_t smoothed = smoother.Smooth(pts);
    // Once measured, a little later because of the hiccup at the start
    if (i > 150)
      EXPECT_NEAR(smoothed - previous, duration, 1000) << "frame " << i;
    previousPts = pts;
    previous = smoothed;
  }
}

TEST(PtsSmootherTest, SkipsLostFrames)
{
  CPtsSmoother smoother;
  int64_t pts = 0;
  for (int i = 0; i < 100; ++i, pts += FRAME)
    EXPECT_EQ(smoother.Smooth(pts), pts);

  // Three frames lost
  pts += 3 * FRAME;
  int64_t previous = pts - 4 * FRAME;
  for (int i = 0; i < 100; ++i, pts += FRAME)
  {
    const int64_t smoothed = smoother.Smooth(pts);
    EXPECT_GE(smoothed - previous, FRAME - 1000);
    if (i >= 40)
      EXPECT_NEAR(smoothed, pts, 5000) << "frame " << i;
    previous = smoothed;
  }
}

TEST(PtsSmootherTest, MeasuresAgainAfterJump)
{
  CPtsSmoother smoother;
  int64_t pts = 0;
  for (int i = 0; i < 100; ++i, pts += FRAME)
    smoother.Smooth(pts);

  pts += 5000000;
  for (int i = 0; i < 100; ++i, pts += FRAME)
    EXPECT_EQ(smoother.Smooth(pts), pts);
}

TEST(PtsSmootherTest, MeasuresAgainAfterFrameRateChange)
{
  CPtsSmoother smoother;
  int64_t pts = 0;
  for (int i = 0; i < 100; ++i, pts += FRAME)
    smoother.Smooth(pts);

  for (int i = 0; i < 200; ++i, pts += 2 * FRAME)
  {
    const int64_t smoothed = smoother.Smooth(pts);
    if (i >= 100)
      EXPECT_EQ(smoothed, pts) << "frame " << i;
  }
}

TEST(PtsSmootherTest, KeepsTimesOfReorderedFrames)
{
  // B-frames: I0 P3 B1 B2 P6 B4 B5 ...
  CPtsSmoother smoother;
  EXPECT_EQ(smoother.Smooth(0), 0);
  for (int64_t group = 0; group < 100; ++group)
  {
    for (const int64_t frame : {3, 1, 2})
    {
      const int64_t pts = (group * 3 + frame) * 40000;
      EXPECT_EQ(smoother.Smooth(pts), pts);
    }
  }
}

TEST(PtsSmootherTest, NeverGoesBackwards)
{
  CPtsSmoother smoother;
  int64_t pts = 10000000;
  int64_t previous = 0;
  for (int i = 0; i < 200; ++i, pts += FRAME)
  {
    if (i == 100)
      pts -= 5000000;
    const int64_t smoothed = smoother.Smooth(pts);
    EXPECT_GT(smoothed, previous) << "frame " << i;
    previous = smoothed;
  }
}
