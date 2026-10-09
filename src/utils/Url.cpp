/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Url.h"

#include <cctype>
#include <optional>

namespace WEBRTC
{

namespace
{

struct ParsedUrl
{
  std::string scheme;
  std::optional<std::string> authority;
  std::string path;
  std::optional<std::string> query;
  std::optional<std::string> fragment;
};

bool IsScheme(const std::string& value)
{
  if (value.empty() || !std::isalpha(static_cast<unsigned char>(value[0])))
    return false;
  for (const char c : value)
  {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.')
      return false;
  }
  return true;
}

ParsedUrl Parse(std::string url)
{
  ParsedUrl result;

  if (const size_t fragment = url.find('#'); fragment != std::string::npos)
  {
    result.fragment = url.substr(fragment + 1);
    url.erase(fragment);
  }
  if (const size_t query = url.find('?'); query != std::string::npos)
  {
    result.query = url.substr(query + 1);
    url.erase(query);
  }
  if (const size_t colon = url.find(':'); colon != std::string::npos)
  {
    const std::string scheme = url.substr(0, colon);
    if (IsScheme(scheme))
    {
      result.scheme = scheme;
      url.erase(0, colon + 1);
    }
  }
  if (url.starts_with("//"))
  {
    const size_t end = url.find('/', 2);
    result.authority = url.substr(2, end == std::string::npos ? std::string::npos : end - 2);
    url.erase(0, end == std::string::npos ? url.size() : end);
  }
  result.path = url;
  return result;
}

std::string Compose(const ParsedUrl& url)
{
  std::string result;
  if (!url.scheme.empty())
    result += url.scheme + ":";
  if (url.authority)
    result += "//" + *url.authority;
  result += url.path;
  if (url.query)
    result += "?" + *url.query;
  if (url.fragment)
    result += "#" + *url.fragment;
  return result;
}

void RemoveLastSegment(std::string& output)
{
  const size_t slash = output.rfind('/');
  output.erase(slash == std::string::npos ? 0 : slash);
}

// RFC 3986 section 5.2.4
std::string RemoveDotSegments(std::string input)
{
  std::string output;
  while (!input.empty())
  {
    if (input.starts_with("../"))
      input.erase(0, 3);
    else if (input.starts_with("./"))
      input.erase(0, 2);
    else if (input.starts_with("/./"))
      input.replace(0, 3, "/");
    else if (input == "/.")
      input = "/";
    else if (input.starts_with("/../"))
    {
      input.replace(0, 4, "/");
      RemoveLastSegment(output);
    }
    else if (input == "/..")
    {
      input = "/";
      RemoveLastSegment(output);
    }
    else if (input == "." || input == "..")
      input.clear();
    else
    {
      size_t end = input.find('/', input[0] == '/' ? 1 : 0);
      if (end == std::string::npos)
        end = input.size();
      output += input.substr(0, end);
      input.erase(0, end);
    }
  }
  return output;
}

std::string MergePaths(const ParsedUrl& base, const std::string& path)
{
  if (base.authority && base.path.empty())
    return "/" + path;
  const size_t slash = base.path.rfind('/');
  if (slash == std::string::npos)
    return path;
  return base.path.substr(0, slash + 1) + path;
}

} // namespace

std::pair<std::string, std::string> SplitOptions(const std::string& url)
{
  const size_t separator = url.find('|');
  if (separator == std::string::npos)
    return {url, {}};
  return {url.substr(0, separator), url.substr(separator)};
}

// RFC 3986 section 5.2.2
std::string ResolveUrl(const std::string& base, const std::string& reference)
{
  const ParsedUrl baseUrl = Parse(base);
  const ParsedUrl referenceUrl = Parse(reference);

  ParsedUrl target;
  if (!referenceUrl.scheme.empty())
  {
    target = referenceUrl;
    target.path = RemoveDotSegments(referenceUrl.path);
    return Compose(target);
  }

  target.scheme = baseUrl.scheme;
  if (referenceUrl.authority)
  {
    target.authority = referenceUrl.authority;
    target.path = RemoveDotSegments(referenceUrl.path);
    target.query = referenceUrl.query;
  }
  else
  {
    target.authority = baseUrl.authority;
    if (referenceUrl.path.empty())
    {
      target.path = baseUrl.path;
      target.query = referenceUrl.query ? referenceUrl.query : baseUrl.query;
    }
    else
    {
      target.path = RemoveDotSegments(referenceUrl.path.starts_with('/')
                                          ? referenceUrl.path
                                          : MergePaths(baseUrl, referenceUrl.path));
      target.query = referenceUrl.query;
    }
  }
  target.fragment = referenceUrl.fragment;
  return Compose(target);
}

std::string RemoveUserInfo(const std::string& url)
{
  ParsedUrl parsed = Parse(url);
  if (parsed.authority)
  {
    if (const size_t at = parsed.authority->rfind('@'); at != std::string::npos)
      parsed.authority->erase(0, at + 1);
  }
  return Compose(parsed);
}

} // namespace WEBRTC
