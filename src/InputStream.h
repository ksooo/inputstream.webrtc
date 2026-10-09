/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <memory>

#include <kodi/addon-instance/Inputstream.h>

namespace WEBRTC
{

class CSession;
class CStreamBuffer;
class IHttpTransport;
class ISignaling;

class ATTR_DLL_LOCAL CInputStream : public kodi::addon::CInstanceInputStream
{
public:
  explicit CInputStream(const kodi::addon::IInstanceInfo& instance);
  ~CInputStream() override;

  void GetCapabilities(kodi::addon::InputstreamCapabilities& capabilities) override;
  bool Open(const kodi::addon::InputstreamProperty& props) override;
  void Close() override;

  bool GetStreamIds(std::vector<unsigned int>& ids) override;
  bool GetStream(int streamid, kodi::addon::InputstreamInfo& stream) override;
  void EnableStream(int streamid, bool enable) override;
  bool OpenStream(int streamid) override;

  DEMUX_PACKET* DemuxRead() override;
  void DemuxAbort() override;
  void DemuxFlush() override;
  void DemuxReset() override;

private:
  std::shared_ptr<CStreamBuffer> m_buffer;
  std::unique_ptr<IHttpTransport> m_transport;
  std::unique_ptr<ISignaling> m_signaling;
  std::unique_ptr<CSession> m_session;
};

} // namespace WEBRTC
