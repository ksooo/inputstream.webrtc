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
#include "whep/KodiHttpTransport.h"
#include "whep/WhepClient.h"

#include <chrono>
#include <memory>

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

  CKodiHttpTransport transport;
  std::unique_ptr<ISignaling> signaling;
  if (properties->signaling == SignalingType::HOME_ASSISTANT)
    signaling =
        std::make_unique<CHaSignaling>(props.GetURL(), properties->bearerToken,
                                       properties->entityId, FindCaBundle(), CONNECT_TIMEOUT);
  else
    signaling = std::make_unique<CWhepClient>(transport, props.GetURL(), properties->bearerToken);

  CSession session({ParseIceServers(properties->iceServers), properties->audio, CONNECT_TIMEOUT});
  if (session.Connect(*signaling))
    Log(LogLevel::LEVEL_ERROR, "Playback is not implemented yet");
  session.Close();
  signaling->Close();
  return false;
}

void CInputStream::Close()
{
}

} // namespace WEBRTC
