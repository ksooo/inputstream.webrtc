/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/Url.h"

#include <gtest/gtest.h>

using namespace WEBRTC;

TEST(UrlTest, SplitsOptions)
{
  EXPECT_EQ(SplitOptions("http://h/whep|User-Agent=Kodi"),
            std::make_pair(std::string("http://h/whep"), std::string("|User-Agent=Kodi")));
  EXPECT_EQ(SplitOptions("http://h/whep"),
            std::make_pair(std::string("http://h/whep"), std::string()));
}

TEST(UrlTest, ResolvesReferences)
{
  const std::string base = "http://u:p@h:11984/api/webrtc?src=cam";
  EXPECT_EQ(ResolveUrl(base, "webrtc?id=1"), "http://u:p@h:11984/api/webrtc?id=1");
  EXPECT_EQ(ResolveUrl(base, "/whep/session/1"), "http://u:p@h:11984/whep/session/1");
  EXPECT_EQ(ResolveUrl(base, "?id=1"), "http://u:p@h:11984/api/webrtc?id=1");
  EXPECT_EQ(ResolveUrl(base, "../session/1"), "http://u:p@h:11984/session/1");
  EXPECT_EQ(ResolveUrl(base, "./session/1"), "http://u:p@h:11984/api/session/1");
  EXPECT_EQ(ResolveUrl(base, "//other/x"), "http://other/x");
  EXPECT_EQ(ResolveUrl(base, "https://other/session/1"), "https://other/session/1");
  EXPECT_EQ(ResolveUrl("http://h", "session"), "http://h/session");
}

TEST(UrlTest, RemovesUserInfo)
{
  EXPECT_EQ(RemoveUserInfo("http://u:p@h:11984/api/webrtc?src=cam"),
            "http://h:11984/api/webrtc?src=cam");
  EXPECT_EQ(RemoveUserInfo("http://h/whep"), "http://h/whep");
}
