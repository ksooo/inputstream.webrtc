/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/addon-instance/Inputstream.h>

namespace WEBRTC
{

class ATTR_DLL_LOCAL CInputStream : public kodi::addon::CInstanceInputStream
{
public:
  explicit CInputStream(const kodi::addon::IInstanceInfo& instance);

  void GetCapabilities(kodi::addon::InputstreamCapabilities& capabilities) override;
  bool Open(const kodi::addon::InputstreamProperty& props) override;
  void Close() override;
};

} // namespace WEBRTC
