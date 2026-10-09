/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Addon.h"

#include "InputStream.h"
#include "utils/RtcLog.h"

#include <rtc/global.hpp>

namespace WEBRTC
{

CAddon::CAddon()
{
  InitRtcLog();
}

CAddon::~CAddon()
{
  // The threads of libdatachannel must not outlive the add-on library
  rtc::Cleanup().wait();
}

ADDON_STATUS CAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                    KODI_ADDON_INSTANCE_HDL& hdl)
{
  if (!instance.IsType(ADDON_INSTANCE_INPUTSTREAM))
    return ADDON_STATUS_UNKNOWN;

  hdl = new CInputStream(instance);
  return ADDON_STATUS_OK;
}

} // namespace WEBRTC

ADDONCREATOR(WEBRTC::CAddon)
