/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "InputStream.h"

#include "utils/Log.h"

namespace WEBRTC
{

CInputStream::CInputStream(const kodi::addon::IInstanceInfo& instance)
  : CInstanceInputStream(instance)
{
}

void CInputStream::GetCapabilities(kodi::addon::InputstreamCapabilities& capabilities)
{
  capabilities.SetMask(INPUTSTREAM_SUPPORTS_IDEMUX);
}

bool CInputStream::Open(const kodi::addon::InputstreamProperty& props)
{
  Log(LogLevel::LEVEL_ERROR, "Playback is not implemented yet");
  return false;
}

void CInputStream::Close()
{
}

} // namespace WEBRTC
