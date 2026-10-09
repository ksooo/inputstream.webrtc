/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "InputStream.h"

#include "StreamProperties.h"
#include "ha/HaSignaling.h"
#include "session/IceServers.h"
#include "session/Session.h"
#include "utils/CaBundle.h"
#include "utils/Log.h"

#include <chrono>

namespace WEBRTC
{

namespace
{

constexpr std::chrono::milliseconds CONNECT_TIMEOUT = std::chrono::seconds(10);

} // namespace

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
  const auto properties = ParseStreamProperties(props.GetProperties());
  if (!properties)
    return false;

  if (properties->signaling != SignalingType::HOME_ASSISTANT)
  {
    Log(LogLevel::LEVEL_ERROR, "WHEP is not implemented yet");
    return false;
  }

  CHaSignaling signaling(props.GetURL(), properties->bearerToken, properties->entityId,
                         FindCaBundle(), CONNECT_TIMEOUT);
  CSession session({ParseIceServers(properties->iceServers), properties->audio, CONNECT_TIMEOUT});
  if (session.Connect(signaling))
    Log(LogLevel::LEVEL_ERROR, "Playback is not implemented yet");
  session.Close();
  signaling.Close();
  return false;
}

void CInputStream::Close()
{
}

} // namespace WEBRTC
