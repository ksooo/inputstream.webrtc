/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/Base64.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(Base64Test, Encodes)
{
  EXPECT_EQ(Base64Encode(""), "");
  EXPECT_EQ(Base64Encode("f"), "Zg==");
  EXPECT_EQ(Base64Encode("fo"), "Zm8=");
  EXPECT_EQ(Base64Encode("foo"), "Zm9v");
  EXPECT_EQ(Base64Encode("foobar"), "Zm9vYmFy");
  EXPECT_EQ(Base64Encode(std::string("\xff\xfe\x00", 3)), "//4A");
}
