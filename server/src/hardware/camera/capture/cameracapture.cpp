/**
 * server/src/hardware/camera/capture/cameracapture.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#include "cameracapture.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

bool CameraCapture::framePeriodElapsed()
{
  using namespace std::chrono;
  const double fps = m_fps.load();
  if(fps <= 0.0)
    return true;

  const auto now = steady_clock::now();
  const auto period =
    duration_cast<steady_clock::duration>(duration<double>(1.0 / fps));

  if(m_lastPublishTime.time_since_epoch().count() != 0 &&
     (now - m_lastPublishTime) < period)
    return false; // too soon -- drop this frame to hold the target rate

  m_lastPublishTime = now;
  return true;
}

void CameraCapture::applyLiveSettings(cv::VideoCapture& cap)
{
  std::optional<int> brightness;
  {
    std::lock_guard<std::mutex> l(m_liveMutex);
    brightness = m_pendingBrightness; m_pendingBrightness.reset();
  }
  // Brightness: signed value applied directly, centred on 0 = neutral. Negative
  // darkens, positive brightens -- so a too-bright camera can be turned down
  // BELOW neutral (which a 0..100 range could not do). Only set when the caller
  // actually pushed a value (manual mode); auto-brightness leaves it untouched.
  if(brightness)
    cap.set(cv::CAP_PROP_BRIGHTNESS, static_cast<double>(*brightness));
}

bool CameraCapture::encodeFrame(const cv::Mat& frame,
                                 std::vector<uint8_t>& jpegOut) const
{
  cv::Mat out = frame;

  // flipCode: 0 = vertical (around x-axis), >0 = horizontal (around y-axis), <0 = both
  if((m_flipVertical || m_flipHorizontal) && !out.empty())
  {
    const int flipCode = (m_flipVertical && m_flipHorizontal) ? -1
                       : (m_flipVertical ? 0 : 1);
    cv::Mat flipped;
    cv::flip(out, flipped, flipCode);
    out = flipped;
  }

  const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, m_jpegQuality.load()};
  return cv::imencode(".jpg", out, jpegOut, params);
}
