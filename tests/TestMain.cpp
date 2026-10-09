/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/RtcLog.h"

#include <cstdlib>

#include <gtest/gtest.h>
#include <rtc/global.hpp>

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  if (std::getenv("WEBRTC_TEST_LOG"))
    WEBRTC::InitRtcLog();
  const int result = RUN_ALL_TESTS();
  rtc::Cleanup().wait();
  return result;
}
