/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "CaBundle.h"

#include <cstdlib>
#include <fstream>

namespace WEBRTC
{

namespace
{

bool IsReadable(const std::string& path)
{
  return std::ifstream(path).good();
}

} // namespace

std::optional<std::string> FindCaBundle()
{
  return FindCaBundle(std::getenv("SSL_CERT_FILE"),
                      {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                       "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem"});
}

std::optional<std::string> FindCaBundle(const char* sslCertFile,
                                        const std::vector<std::string>& systemBundles)
{
  if (sslCertFile && IsReadable(sslCertFile))
    return sslCertFile;

  for (const auto& bundle : systemBundles)
  {
    if (IsReadable(bundle))
      return bundle;
  }
  return {};
}

} // namespace WEBRTC
