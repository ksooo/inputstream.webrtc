/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace WEBRTC
{

/*!
 * \brief Evens out the presentation times of video frames. Cameras send their frames at an
 *        irregular pace, with timestamps tens or hundreds of milliseconds off, and Kodi detects
 *        the frame rate only from evenly spaced presentation times. The frame duration is measured
 *        over the first seconds; from then on, the presentation times advance by it and slowly
 *        follow the source. When the source stays off by whole frames, frames were lost and are
 *        skipped; when its pace changes or its timestamps jump, a new measurement starts. Sources
 *        that reorder frames, for B-frames, keep their presentation times.
 */
class CPtsSmoother
{
public:
  /*!
   * \param pts The presentation time from the RTP timestamp, in microseconds.
   * \return The even presentation time.
   */
  int64_t Smooth(int64_t pts);

private:
  int64_t Measure(int64_t pts);
  int64_t Advance(int64_t pts);
  double GetReferenceError() const;

  bool m_reordered{false};
  std::optional<int64_t> m_previous;
  std::optional<int64_t> m_last;
  std::vector<int64_t> m_measured; // presentation times of the source while measuring
  double m_duration{0}; // frame duration, 0 while measuring
  double m_pts{0};
  double m_error{0}; // difference to the source, low-pass filtered
  struct Error
  {
    int64_t pts;
    double error;
  };
  std::deque<Error> m_recent; // differences to the source in the latest seconds
  std::vector<double> m_offGrid; // differences of the latest frames that are off the others
  int64_t m_offGridStart{0};
};

} // namespace WEBRTC
