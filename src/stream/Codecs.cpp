/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Codecs.h"

#include "utils/Base64.h"
#include "utils/StringUtils.h"

#include <cstdlib>
#include <stdexcept>
#include <utility>
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

std::vector<uint8_t> RemoveEmulationPrevention(const NalUnit& unit)
{
  std::vector<uint8_t> data;
  data.reserve(unit.size);
  for (size_t i = 0; i < unit.size; ++i)
  {
    if (i >= 2 && unit.data[i] == 3 && unit.data[i - 1] == 0 && unit.data[i - 2] == 0)
      continue;
    data.push_back(unit.data[i]);
  }
  return data;
}

class CBitReader
{
public:
  explicit CBitReader(std::vector<uint8_t> data) : m_data(std::move(data)) {}

  uint32_t Read(unsigned int bits)
  {
    uint32_t value = 0;
    for (unsigned int i = 0; i < bits; ++i)
    {
      if (m_position >= m_data.size() * 8)
        throw std::out_of_range("Truncated parameter set");
      value = (value << 1) | ((m_data[m_position / 8] >> (7 - m_position % 8)) & 1);
      ++m_position;
    }
    return value;
  }

  void Skip(unsigned int bits) { m_position += bits; }

  uint32_t ReadUe()
  {
    unsigned int zeros = 0;
    while (Read(1) == 0)
    {
      if (++zeros > 31)
        throw std::out_of_range("Invalid Exp-Golomb code");
    }
    return (1u << zeros) - 1 + Read(zeros);
  }

  int32_t ReadSe()
  {
    const uint32_t value = ReadUe();
    return value & 1 ? static_cast<int32_t>((value + 1) / 2) : -static_cast<int32_t>(value / 2);
  }

private:
  std::vector<uint8_t> m_data;
  size_t m_position{0};
};

struct Crop
{
  uint32_t left{0};
  uint32_t right{0};
  uint32_t top{0};
  uint32_t bottom{0};
};

Crop ReadCrop(CBitReader& reader)
{
  Crop crop;
  if (reader.Read(1))
  {
    crop.left = reader.ReadUe();
    crop.right = reader.ReadUe();
    crop.top = reader.ReadUe();
    crop.bottom = reader.ReadUe();
  }
  return crop;
}

void SkipScalingLists(CBitReader& reader, unsigned int count)
{
  for (unsigned int i = 0; i < count; ++i)
  {
    if (!reader.Read(1))
      continue;
    int last = 8;
    for (int j = 0; j < (i < 6 ? 16 : 64); ++j)
    {
      const int next = (last + reader.ReadSe() + 256) % 256;
      if (next == 0)
        break;
      last = next;
    }
  }
}

// H.264 7.3.2.1.1
PictureSize ReadH264PictureSize(CBitReader& reader)
{
  const uint32_t profile = reader.Read(8);
  reader.Skip(16); // constraint flags, level_idc
  reader.ReadUe(); // seq_parameter_set_id
  uint32_t chromaArrayType = 1;
  if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 ||
      profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 ||
      profile == 139 || profile == 134 || profile == 135)
  {
    const uint32_t chromaFormat = reader.ReadUe();
    chromaArrayType = chromaFormat;
    if (chromaFormat == 3 && reader.Read(1)) // separate_colour_plane_flag
      chromaArrayType = 0;
    reader.ReadUe(); // bit_depth_luma_minus8
    reader.ReadUe(); // bit_depth_chroma_minus8
    reader.Skip(1); // qpprime_y_zero_transform_bypass_flag
    if (reader.Read(1)) // seq_scaling_matrix_present_flag
      SkipScalingLists(reader, chromaFormat == 3 ? 12 : 8);
  }
  reader.ReadUe(); // log2_max_frame_num_minus4
  const uint32_t pocType = reader.ReadUe();
  if (pocType == 0)
  {
    reader.ReadUe(); // log2_max_pic_order_cnt_lsb_minus4
  }
  else if (pocType == 1)
  {
    reader.Skip(1); // delta_pic_order_always_zero_flag
    reader.ReadSe(); // offset_for_non_ref_pic
    reader.ReadSe(); // offset_for_top_to_bottom_field
    for (uint32_t i = reader.ReadUe(); i > 0; --i)
      reader.ReadSe(); // offset_for_ref_frame
  }
  reader.ReadUe(); // max_num_ref_frames
  reader.Skip(1); // gaps_in_frame_num_value_allowed_flag
  const uint32_t widthInMbs = reader.ReadUe() + 1;
  const uint32_t heightInMapUnits = reader.ReadUe() + 1;
  const uint32_t frameMbsOnly = reader.Read(1);
  if (!frameMbsOnly)
    reader.Skip(1); // mb_adaptive_frame_field_flag
  reader.Skip(1); // direct_8x8_inference_flag
  const Crop crop = ReadCrop(reader);

  const uint32_t cropUnitX = chromaArrayType == 1 || chromaArrayType == 2 ? 2 : 1;
  const uint32_t cropUnitY = (chromaArrayType == 1 ? 2 : 1) * (2 - frameMbsOnly);
  return {widthInMbs * 16 - cropUnitX * (crop.left + crop.right),
          (2 - frameMbsOnly) * heightInMapUnits * 16 - cropUnitY * (crop.top + crop.bottom)};
}

// H.265 7.3.2.2.1
PictureSize ReadH265PictureSize(CBitReader& reader)
{
  reader.Skip(4); // sps_video_parameter_set_id
  const uint32_t maxSubLayersMinus1 = reader.Read(3);
  reader.Skip(1); // sps_temporal_id_nesting_flag
  reader.Skip(96); // general profile, tier and level
  bool profilePresent[7]{};
  bool levelPresent[7]{};
  for (uint32_t i = 0; i < maxSubLayersMinus1; ++i)
  {
    profilePresent[i] = reader.Read(1);
    levelPresent[i] = reader.Read(1);
  }
  if (maxSubLayersMinus1 > 0)
    reader.Skip(2 * (8 - maxSubLayersMinus1)); // reserved_zero_2bits
  for (uint32_t i = 0; i < maxSubLayersMinus1; ++i)
  {
    if (profilePresent[i])
      reader.Skip(88);
    if (levelPresent[i])
      reader.Skip(8);
  }
  reader.ReadUe(); // sps_seq_parameter_set_id
  const uint32_t chromaFormat = reader.ReadUe();
  uint32_t chromaArrayType = chromaFormat;
  if (chromaFormat == 3 && reader.Read(1)) // separate_colour_plane_flag
    chromaArrayType = 0;
  const uint32_t width = reader.ReadUe();
  const uint32_t height = reader.ReadUe();
  const Crop crop = ReadCrop(reader);

  const uint32_t subWidth = chromaArrayType == 1 || chromaArrayType == 2 ? 2 : 1;
  const uint32_t subHeight = chromaArrayType == 1 ? 2 : 1;
  return {width - subWidth * (crop.left + crop.right),
          height - subHeight * (crop.top + crop.bottom)};
}

std::optional<Codec> ParseCodec(const std::string& format)
{
  const std::string name = ToLower(format);
  if (name == "h264")
    return Codec::H264;
  if (name == "h265")
    return Codec::H265;
  if (name == "opus")
    return Codec::OPUS;
  if (name == "pcmu")
    return Codec::PCMU;
  if (name == "pcma")
    return Codec::PCMA;
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
    case Codec::OPUS:
      return "opus";
    case Codec::PCMU:
      return "pcm_mulaw";
    case Codec::PCMA:
      return "pcm_alaw";
  }
  return "";
}

bool IsVideo(Codec codec)
{
  return codec == Codec::H264 || codec == Codec::H265;
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
      CodecInfo info{*codec, static_cast<uint32_t>(rtpMap->clockRate)};
      if (IsVideo(*codec))
        info.extraData = GetParameterSets(*codec, rtpMap->fmtps);
      else
        info.channels = rtpMap->encParams.empty()
                            ? 1
                            : static_cast<unsigned int>(std::atoi(rtpMap->encParams.c_str()));
      codecs.emplace(payloadType, std::move(info));
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

std::optional<PictureSize> GetPictureSize(Codec codec, const std::vector<uint8_t>& parameterSets)
{
  for (const auto& unit : GetNalUnits(parameterSets.data(), parameterSets.size()))
  {
    if (GetNalUnitType(codec, unit) != (codec == Codec::H264 ? 7 : 33)) // SPS
      continue;

    CBitReader reader(RemoveEmulationPrevention(unit));
    reader.Skip(codec == Codec::H264 ? 8 : 16); // NAL unit header
    try
    {
      return codec == Codec::H264 ? ReadH264PictureSize(reader) : ReadH265PictureSize(reader);
    }
    catch (const std::out_of_range&)
    {
      return {};
    }
  }
  return {};
}

} // namespace WEBRTC
