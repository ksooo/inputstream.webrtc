/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rtc
{
class Description;
}

namespace WEBRTC
{

enum class Codec
{
  H264,
  H265
};

struct CodecInfo
{
  Codec codec;
  uint32_t clockRate;
  std::vector<uint8_t> extraData; // parameter sets from the SDP, Annex B
};

/*!
 * \brief The codec name Kodi uses for a codec.
 */
const char* GetKodiCodecName(Codec codec);

/*!
 * \brief The supported codecs of a media section of a session description, by payload type.
 */
std::map<int, CodecInfo> GetCodecs(const rtc::Description& description, const std::string& mid);

/*!
 * \brief Whether an Annex B frame starts a sequence the decoder can begin with: it contains an
 *        IDR/IRAP picture or parameter sets.
 */
bool IsKeyframe(Codec codec, const uint8_t* data, size_t size);

/*!
 * \brief The parameter sets (H.264 SPS/PPS, H.265 VPS/SPS/PPS) of an Annex B frame, each
 *        preceded by a start code.
 */
std::vector<uint8_t> ExtractParameterSets(Codec codec, const uint8_t* data, size_t size);

/*!
 * \brief The parameter sets of a H.264 or H.265 format description (sprop-parameter-sets or
 *        sprop-vps/sps/pps), each preceded by an Annex B start code.
 */
std::vector<uint8_t> GetParameterSets(Codec codec, const std::vector<std::string>& fmtps);

} // namespace WEBRTC
