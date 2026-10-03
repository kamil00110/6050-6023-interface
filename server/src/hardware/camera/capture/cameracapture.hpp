/**
 * server/src/hardware/camera/capture/cameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP

#include <cstdint>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <optional>
#include <utility>

namespace cv { class Mat; class VideoCapture; }

class CameraCapture
{
public:
  virtual ~CameraCapture() = default;

  [[nodiscard]] virtual bool open() = 0;
  virtual uint32_t width()  const = 0;
  virtual uint32_t height() const = 0;
  [[nodiscard]] virtual bool readJpeg(std::vector<uint8_t>& jpegOut) = 0;
  virtual void interrupt() = 0;

  /// Resolutions that were verified to deliver non-black frames during open(),
  /// largest-first. Empty when not applicable (network cameras) or not probed.
  /// Used to populate the camera's resolution list with only working sizes.
  virtual std::vector<std::pair<uint32_t, uint32_t>> usableResolutions() const { return {}; }

  // Live-adjustable settings. These may be called from another thread while the
  // capture loop is running; the new value is picked up without a reconnect.
  // fps always takes effect (server-side rate limiting). brightness is applied
  // best-effort by LocalCameraCapture only (ignored by network captures).
  void setFps(double fps) { m_fps.store(fps > 0.0 ? fps : 1.0); }
  void setJpegQuality(int value) { m_jpegQuality.store(value); }
  void setBrightness(int value) { std::lock_guard<std::mutex> l(m_liveMutex); m_pendingBrightness = value; }

protected:
  std::atomic<double> m_fps{25.0};
  std::atomic<int>    m_jpegQuality{75};
  bool     m_flipVertical{false};
  bool     m_flipHorizontal{false};

  // Frame-rate limiting: timestamp of the last frame we actually published.
  std::chrono::steady_clock::time_point m_lastPublishTime{};

  // Pending live settings (applied on the capture thread between frames).
  std::mutex m_liveMutex;
  std::optional<int> m_pendingBrightness;

  // Returns true once at least one frame period (1/fps) has elapsed since the
  // last published frame; false means this frame should be dropped.
  bool framePeriodElapsed();

  // Applies any queued brightness changes to the given capture.
  void applyLiveSettings(cv::VideoCapture& cap);

  // Implemented in cameracapture.cpp to keep OpenCV out of this header
  bool encodeFrame(const cv::Mat& frame, std::vector<uint8_t>& jpegOut) const;
};
#endif
