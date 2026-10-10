/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "PtsSmoother.h"

#include <algorithm>
#include <cmath>

namespace WEBRTC
{

namespace
{

constexpr size_t MIN_MEASURED_FRAMES = 16;
constexpr int64_t MEASURE_TIME = 4000000;
constexpr double FILTER_FRAMES = 32;
constexpr double DURATION_GAIN = 1.0 / 16384;
// Per frame; Kodi accepts frame durations that differ by up to 2.5 ms
constexpr double MAX_CORRECTION = 500;
// Frames are off when they differ from the median of this time, which ignores the few frames
// that cameras delay around keyframes, by up to about 300 ms
constexpr int64_t REFERENCE_TIME = 4000000;
constexpr int64_t OFF_GRID_TIME = 1000000;
constexpr size_t MIN_OFF_GRID_FRAMES = 3;
constexpr double MAX_ERROR = 1000000;

double Median(std::vector<double> values)
{
  const auto middle = values.begin() + values.size() / 2;
  std::nth_element(values.begin(), middle, values.end());
  return *middle;
}

} // namespace

int64_t CPtsSmoother::Smooth(int64_t pts)
{
  if (!m_reordered && m_previous && pts < *m_previous && *m_previous - pts < MAX_ERROR)
    m_reordered = true;
  m_previous = pts;
  if (m_reordered)
    return pts;

  if (m_duration > 0 && std::abs(pts - (m_pts + m_duration)) > MAX_ERROR)
    m_duration = 0;

  int64_t result = m_duration == 0 ? Measure(pts) : Advance(pts);
  if (m_last && result <= *m_last)
    result = *m_last + 1;
  m_last = result;
  return result;
}

int64_t CPtsSmoother::Measure(int64_t pts)
{
  m_measured.push_back(pts);
  if (m_measured.size() < MIN_MEASURED_FRAMES || pts - m_measured.front() < MEASURE_TIME)
    return pts;

  // The least squares line through the measured presentation times
  const double count = static_cast<double>(m_measured.size());
  double mean = 0;
  for (const int64_t measured : m_measured)
    mean += static_cast<double>(measured) / count;
  const double meanIndex = (count - 1) / 2;
  double covariance = 0;
  double variance = 0;
  for (size_t i = 0; i < m_measured.size(); ++i)
  {
    covariance += (i - meanIndex) * (m_measured[i] - mean);
    variance += (i - meanIndex) * (i - meanIndex);
  }
  m_measured.clear();
  if (covariance <= 0)
    return pts;

  m_duration = covariance / variance;
  m_pts = mean + m_duration * meanIndex;
  m_error = 0;
  m_recent.clear();
  m_offGrid.clear();
  return std::llround(m_pts);
}

int64_t CPtsSmoother::Advance(int64_t pts)
{
  m_pts += m_duration;
  const double error = pts - m_pts;
  m_recent.push_back({pts, error});
  while (m_recent.front().pts < pts - REFERENCE_TIME)
    m_recent.pop_front();

  const double off = error - GetReferenceError();
  if (std::abs(off) > m_duration / 2 && (m_offGrid.empty() || (off > 0) == (m_offGrid.back() > 0)))
  {
    if (m_offGrid.empty())
      m_offGridStart = pts;
    m_offGrid.push_back(off);
  }
  else
  {
    m_offGrid.clear();
  }

  if (m_offGrid.size() >= MIN_OFF_GRID_FRAMES && pts - m_offGridStart >= OFF_GRID_TIME)
  {
    const double pace = static_cast<double>(pts - m_offGridStart) / (m_offGrid.size() - 1);
    const double frames = std::round(Median(m_offGrid) / m_duration);
    m_offGrid.clear();
    if (std::abs(pace - m_duration) > m_duration / 4)
    {
      // The frame rate changed
      m_duration = 0;
      return Measure(pts);
    }

    // Lost frames
    m_pts += frames * m_duration;
    m_error = 0;
    m_recent.clear();
    return std::llround(m_pts);
  }

  // Frames that are off do not tell about the pace of the source
  if (!m_offGrid.empty())
    return std::llround(m_pts);

  m_error += (error - m_error) / FILTER_FRAMES;
  m_duration += m_error * DURATION_GAIN;
  m_pts += std::clamp(m_error / FILTER_FRAMES, -MAX_CORRECTION, MAX_CORRECTION);
  return std::llround(m_pts);
}

double CPtsSmoother::GetReferenceError() const
{
  std::vector<double> errors;
  errors.reserve(m_recent.size());
  for (const auto& recent : m_recent)
    errors.push_back(recent.error);
  return Median(std::move(errors));
}

} // namespace WEBRTC
