/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/CaBundle.h"

#include <cstdio>
#include <fstream>

#include <gtest/gtest.h>

using namespace WEBRTC;

namespace
{

class CaBundleTest : public testing::Test
{
protected:
  void SetUp() override { std::ofstream(m_file) << "certificates"; }
  void TearDown() override { std::remove(m_file.c_str()); }

  const std::string m_file{"CaBundleTest.pem"};
};

} // namespace

TEST_F(CaBundleTest, PrefersSslCertFile)
{
  EXPECT_EQ(FindCaBundle(m_file.c_str(), {"missing.pem"}), m_file);
}

TEST_F(CaBundleTest, FallsBackToSystemBundle)
{
  EXPECT_EQ(FindCaBundle(nullptr, {"missing.pem", m_file}), m_file);
  EXPECT_EQ(FindCaBundle("missing.pem", {m_file}), m_file);
}

TEST_F(CaBundleTest, NothingFound)
{
  EXPECT_FALSE(FindCaBundle(nullptr, {"missing.pem"}));
}
