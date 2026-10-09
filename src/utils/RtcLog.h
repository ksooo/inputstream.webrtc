/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "utils/Log.h"

#include <rtc/global.hpp>

namespace WEBRTC
{

LogLevel ToLogLevel(rtc::LogLevel level);

/*!
 * \brief Forwards the messages of libdatachannel to the Kodi log.
 */
void InitRtcLog();

} // namespace WEBRTC
