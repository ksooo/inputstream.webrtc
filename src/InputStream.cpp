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
#include "stream/StreamBuffer.h"
#include "utils/CaBundle.h"
#include "utils/Log.h"
#include "whep/KodiHttpTransport.h"
#include "whep/WhepClient.h"

#include <chrono>
#include <cstring>

namespace WEBRTC
{

namespace
{

constexpr std::chrono::milliseconds CONNECT_TIMEOUT = std::chrono::seconds(10);
constexpr std::chrono::milliseconds FIRST_KEYFRAME_TIMEOUT = std::chrono::seconds(5);
constexpr std::chrono::milliseconds READ_TIMEOUT = std::chrono::milliseconds(20);

} // namespace

CInputStream::CInputStream(const kodi::addon::IInstanceInfo& instance)
  : CInstanceInputStream(instance)
{
}

CInputStream::~CInputStream()
{
  Close();
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

  if (properties->signaling == SignalingType::HOME_ASSISTANT)
  {
    m_signaling =
        std::make_unique<CHaSignaling>(props.GetURL(), properties->bearerToken,
                                       properties->entityId, FindCaBundle(), CONNECT_TIMEOUT);
  }
  else
  {
    m_transport = std::make_unique<CKodiHttpTransport>();
    m_signaling =
        std::make_unique<CWhepClient>(*m_transport, props.GetURL(), properties->bearerToken);
  }

  m_buffer = std::make_shared<CStreamBuffer>();
  m_session = std::make_unique<CSession>(
      SessionConfig{ParseIceServers(properties->iceServers), properties->audio, CONNECT_TIMEOUT},
      m_buffer);
  if (!m_session->Connect(*m_signaling))
  {
    Close();
    return false;
  }

  if (!m_session->WaitForVideo(FIRST_KEYFRAME_TIMEOUT))
  {
    Log(LogLevel::LEVEL_ERROR, "Received no video");
    Close();
    return false;
  }
  return true;
}

void CInputStream::Close()
{
  if (m_session)
    m_session->Close();
  if (m_signaling)
    m_signaling->Close();
  m_session.reset();
  m_signaling.reset();
  m_transport.reset();
  m_buffer.reset();
}

bool CInputStream::GetStreamIds(std::vector<unsigned int>& ids)
{
  if (!m_buffer)
    return false;

  for (const auto& stream : m_buffer->GetStreams())
    ids.emplace_back(static_cast<unsigned int>(stream.id));
  return true;
}

bool CInputStream::GetStream(int streamid, kodi::addon::InputstreamInfo& stream)
{
  if (!m_buffer)
    return false;

  const auto info = m_buffer->GetStream(streamid);
  if (!info)
    return false;

  if (IsVideo(info->codec))
  {
    stream.SetStreamType(INPUTSTREAM_TYPE_VIDEO);
    // Android's MediaCodec decoders need the size up front and take the aspect ratio only from here
    if (const auto size = GetPictureSize(info->codec, info->extraData))
    {
      stream.SetWidth(size->width);
      stream.SetHeight(size->height);
      stream.SetAspect(static_cast<float>(size->width) / static_cast<float>(size->height));
    }
  }
  else
  {
    stream.SetStreamType(INPUTSTREAM_TYPE_AUDIO);
    stream.SetSampleRate(info->sampleRate);
    stream.SetChannels(info->channels);
  }
  stream.SetCodecName(GetKodiCodecName(info->codec));
  stream.SetPhysicalIndex(static_cast<unsigned int>(info->id));
  if (!info->extraData.empty())
    stream.SetExtraData(info->extraData);
  return true;
}

void CInputStream::EnableStream(int streamid, bool enable)
{
}

bool CInputStream::OpenStream(int streamid)
{
  return m_buffer && m_buffer->GetStream(streamid);
}

DEMUX_PACKET* CInputStream::DemuxRead()
{
  if (!m_buffer)
    return nullptr;

  MediaPacket packet;
  switch (m_buffer->Pop(READ_TIMEOUT, packet))
  {
    case CStreamBuffer::Result::PACKET:
    {
      DEMUX_PACKET* demuxPacket = AllocateDemuxPacket(static_cast<int>(packet.data.size()));
      if (!demuxPacket)
        return AllocateDemuxPacket(0);
      std::memcpy(demuxPacket->pData, packet.data.data(), packet.data.size());
      demuxPacket->iSize = static_cast<int>(packet.data.size());
      demuxPacket->iStreamId = packet.streamId;
      demuxPacket->pts = static_cast<double>(packet.pts);
      demuxPacket->dts = demuxPacket->pts;
      return demuxPacket;
    }
    case CStreamBuffer::Result::STREAMS_CHANGED:
    {
      DEMUX_PACKET* demuxPacket = AllocateDemuxPacket(0);
      if (demuxPacket)
        demuxPacket->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
      return demuxPacket;
    }
    case CStreamBuffer::Result::NONE:
      return AllocateDemuxPacket(0);
    case CStreamBuffer::Result::ENDED:
      Log(LogLevel::LEVEL_INFO, "The connection ended");
      return nullptr;
  }
  return nullptr;
}

void CInputStream::DemuxAbort()
{
  if (m_buffer)
    m_buffer->Abort();
}

void CInputStream::DemuxFlush()
{
  if (m_buffer)
    m_buffer->Flush();
}

void CInputStream::DemuxReset()
{
  DemuxFlush();
}

} // namespace WEBRTC
