/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Codecs.h"

#include "utils/Base64.h"
#include "utils/StringUtils.h"

#include <variant>

#include <rtc/description.hpp>

namespace WEBRTC
{

namespace
{

struct NalUnit
{
  const uint8_t* data; // after the start code
  size_t size;
};

std::vector<NalUnit> GetNalUnits(const uint8_t* data, size_t size)
{
  std::vector<NalUnit> units;
  for (size_t i = 0; i + 2 < size; ++i)
  {
    if (data[i] != 0 || data[i + 1] != 0 || data[i + 2] != 1)
      continue;

    // The zero byte in front belongs to a 4 byte start code
    const size_t startCode = i > 0 && data[i - 1] == 0 ? i - 1 : i;
    if (!units.empty())
      units.back().size = static_cast<size_t>(data + startCode - units.back().data);
    units.push_back({data + i + 3, size - (i + 3)});
    i += 2;
  }
  return units;
}

int GetNalUnitType(Codec codec, const NalUnit& unit)
{
  if (unit.size == 0)
    return -1;
  return codec == Codec::H264 ? unit.data[0] & 0x1f : (unit.data[0] >> 1) & 0x3f;
}

std::optional<Codec> ParseCodec(const std::string& format)
{
  const std::string name = ToLower(format);
  if (name == "h264")
    return Codec::H264;
  if (name == "h265")
    return Codec::H265;
  return {};
}

} // namespace

const char* GetKodiCodecName(Codec codec)
{
  switch (codec)
  {
    case Codec::H264:
      return "h264";
    case Codec::H265:
      return "hevc";
  }
  return "";
}

bool IsKeyframe(Codec codec, const uint8_t* data, size_t size)
{
  for (const auto& unit : GetNalUnits(data, size))
  {
    const int type = GetNalUnitType(codec, unit);
    if (codec == Codec::H264 && (type == 5 || type == 7)) // IDR slice, SPS
      return true;
    if (codec == Codec::H265 &&
        ((type >= 16 && type <= 21) || type == 32 || type == 33)) // IRAP slices, VPS, SPS
      return true;
  }
  return false;
}

std::vector<uint8_t> ExtractParameterSets(Codec codec, const uint8_t* data, size_t size)
{
  std::vector<uint8_t> result;
  for (const auto& unit : GetNalUnits(data, size))
  {
    const int type = GetNalUnitType(codec, unit);
    const bool parameterSet = codec == Codec::H264 ? (type == 7 || type == 8) // SPS, PPS
                                                   : (type >= 32 && type <= 34); // VPS, SPS, PPS
    if (!parameterSet)
      continue;
    result.insert(result.end(), {0, 0, 0, 1});
    result.insert(result.end(), unit.data, unit.data + unit.size);
  }
  return result;
}

std::map<int, CodecInfo> GetCodecs(const rtc::Description& description, const std::string& mid)
{
  std::map<int, CodecInfo> codecs;
  for (int i = 0; i < description.mediaCount(); ++i)
  {
    const auto entry = description.media(i);
    const auto* media = std::get_if<const rtc::Description::Media*>(&entry);
    if (!media || (*media)->mid() != mid)
      continue;

    for (const int payloadType : (*media)->payloadTypes())
    {
      const auto* rtpMap = (*media)->rtpMap(payloadType);
      const auto codec = ParseCodec(rtpMap->format);
      if (!codec)
        continue;
      codecs.emplace(payloadType, CodecInfo{*codec, static_cast<uint32_t>(rtpMap->clockRate),
                                            GetParameterSets(*codec, rtpMap->fmtps)});
    }
  }
  return codecs;
}

std::vector<uint8_t> GetParameterSets(Codec codec, const std::vector<std::string>& fmtps)
{
  std::map<std::string, std::string> parameters;
  for (const auto& fmtp : fmtps)
  {
    for (const auto& parameter : Split(fmtp, ';'))
    {
      const size_t equals = parameter.find('=');
      if (equals != std::string::npos)
        parameters[ToLower(parameter.substr(0, equals))] = parameter.substr(equals + 1);
    }
  }

  std::vector<std::string> names;
  if (codec == Codec::H264)
    names = {"sprop-parameter-sets"};
  else
    names = {"sprop-vps", "sprop-sps", "sprop-pps"};

  std::vector<uint8_t> result;
  for (const auto& name : names)
  {
    const auto it = parameters.find(name);
    if (it == parameters.end())
      continue;
    for (const auto& encoded : Split(it->second, ','))
    {
      const auto decoded = Base64Decode(encoded);
      if (!decoded || decoded->empty())
        continue;
      result.insert(result.end(), {0, 0, 0, 1});
      result.insert(result.end(), decoded->begin(), decoded->end());
    }
  }
  return result;
}

} // namespace WEBRTC
