/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/RtcLog.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(RtcLogTest, MapsLevels)
{
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Fatal), LogLevel::LEVEL_ERROR);
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Error), LogLevel::LEVEL_ERROR);
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Warning), LogLevel::LEVEL_WARNING);
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Info), LogLevel::LEVEL_DEBUG);
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Debug), LogLevel::LEVEL_DEBUG);
  EXPECT_EQ(ToLogLevel(rtc::LogLevel::Verbose), LogLevel::LEVEL_DEBUG);
}
