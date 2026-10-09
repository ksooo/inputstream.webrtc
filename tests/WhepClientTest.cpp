/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "whep/HttpTransport.h"
#include "whep/WhepClient.h"

#include <deque>
#include <vector>

#include <gtest/gtest.h>

using namespace WEBRTC;

namespace
{

class CFakeTransport : public IHttpTransport
{
public:
  std::optional<HttpResponse> Send(const HttpRequest& request) override
  {
    m_requests.emplace_back(request);
    if (m_responses.empty())
      return {};
    auto response = std::move(m_responses.front());
    m_responses.pop_front();
    return response;
  }

  std::deque<std::optional<HttpResponse>> m_responses;
  std::vector<HttpRequest> m_requests;
};

HttpResponse Created(std::string location, std::string effectiveUrl = {})
{
  HttpResponse response;
  response.status = 201;
  response.effectiveUrl = std::move(effectiveUrl);
  response.location = std::move(location);
  response.body = "answer";
  return response;
}

std::string GetHeader(const HttpRequest& request, const std::string& name)
{
  for (const auto& [headerName, value] : request.headers)
  {
    if (headerName == name)
      return value;
  }
  return {};
}

} // namespace

TEST(WhepClientTest, PostsOffer)
{
  CFakeTransport transport;
  transport.m_responses.emplace_back(Created("http://h/whep/session/1"));
  CWhepClient client(transport, "http://h/whep", "token");

  EXPECT_EQ(client.Offer("offer", nullptr), "answer");
  ASSERT_EQ(transport.m_requests.size(), 1u);
  const auto& request = transport.m_requests[0];
  EXPECT_EQ(request.method, "POST");
  EXPECT_EQ(request.url, "http://h/whep");
  EXPECT_EQ(request.body, "offer");
  EXPECT_EQ(GetHeader(request, "Content-Type"), "application/sdp");
  EXPECT_EQ(GetHeader(request, "Authorization"), "Bearer token");
}

TEST(WhepClientTest, NoAuthorizationWithoutToken)
{
  CFakeTransport transport;
  transport.m_responses.emplace_back(Created({}));
  CWhepClient client(transport, "http://h/whep", "");

  EXPECT_TRUE(client.Offer("offer", nullptr));
  EXPECT_EQ(GetHeader(transport.m_requests[0], "Authorization"), "");
}

TEST(WhepClientTest, DeletesSessionOnce)
{
  CFakeTransport transport;
  transport.m_responses.emplace_back(Created("http://h/whep/session/1"));
  CWhepClient client(transport, "http://h/whep", "token");

  ASSERT_TRUE(client.Offer("offer", nullptr));
  client.Close();
  client.Close();
  ASSERT_EQ(transport.m_requests.size(), 2u);
  EXPECT_EQ(transport.m_requests[1].method, "DELETE");
  EXPECT_EQ(transport.m_requests[1].url, "http://h/whep/session/1");
  EXPECT_EQ(GetHeader(transport.m_requests[1], "Authorization"), "Bearer token");
}

TEST(WhepClientTest, KeepsCredentialsAndOptionsOfRelativeSession)
{
  CFakeTransport transport;
  // go2rtc answers like this; Kodi reports the URL without the credentials
  transport.m_responses.emplace_back(Created("webrtc?id=1", "http://h:11984/api/webrtc?src=cam"));
  CWhepClient client(transport, "http://u:p@h:11984/api/webrtc?src=cam|X-Test=1", "");

  ASSERT_TRUE(client.Offer("offer", nullptr));
  client.Close();
  ASSERT_EQ(transport.m_requests.size(), 2u);
  EXPECT_EQ(transport.m_requests[1].url, "http://u:p@h:11984/api/webrtc?id=1|X-Test=1");
}

TEST(WhepClientTest, ResolvesSessionAgainstRedirect)
{
  CFakeTransport transport;
  transport.m_responses.emplace_back(Created("session/1", "http://other/whep/cam"));
  CWhepClient client(transport, "http://h/whep/cam", "");

  ASSERT_TRUE(client.Offer("offer", nullptr));
  client.Close();
  ASSERT_EQ(transport.m_requests.size(), 2u);
  EXPECT_EQ(transport.m_requests[1].url, "http://other/whep/session/1");
}

TEST(WhepClientTest, NoSessionNoDelete)
{
  CFakeTransport transport;
  transport.m_responses.emplace_back(Created({}));
  CWhepClient client(transport, "http://h/whep", "");

  ASSERT_TRUE(client.Offer("offer", nullptr));
  client.Close();
  EXPECT_EQ(transport.m_requests.size(), 1u);
}

TEST(WhepClientTest, Failures)
{
  HttpResponse counterOffer = Created("http://h/whep/session/1");
  counterOffer.status = 406;
  HttpResponse serverError;
  serverError.status = 500;
  HttpResponse noAnswer = Created("http://h/whep/session/1");
  noAnswer.body.clear();

  for (const auto& response :
       {std::optional<HttpResponse>(counterOffer), std::optional<HttpResponse>(serverError),
        std::optional<HttpResponse>(noAnswer), std::optional<HttpResponse>()})
  {
    CFakeTransport transport;
    transport.m_responses.emplace_back(response);
    CWhepClient client(transport, "http://h/whep", "");

    EXPECT_FALSE(client.Offer("offer", nullptr));
    client.Close();
    EXPECT_EQ(transport.m_requests.size(), 1u);
  }
}
