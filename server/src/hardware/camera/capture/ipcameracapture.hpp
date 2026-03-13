/**
 * server/src/hardware/camera/capture/ipcameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_IPCAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_IPCAMERACAPTURE_HPP

#include "cameracapture.hpp"
#include <string>
#include <atomic>
#include <opencv2/videoio.hpp>

/**
 * @brief Captures from an IP camera URL using OpenCV.
 *
 * Supports:
 *   - RTSP streams     rtsp://user:pass@host/stream
 *   - MJPEG over HTTP  http://host/video.mjpg
 *
 * OpenCV's FFmpeg backend handles both transparently.
 * Frames are re-encoded as JPEG so the downstream MJPEGStreamer always
 * receives a consistent byte format regardless of the camera's native codec.
 */
class IpCameraCapture final : public CameraCapture
{
public:
  IpCameraCapture(const std::string& url, double fps);

  bool     open()    override;
  uint32_t width()   const override { return m_width;  }
  uint32_t height()  const override { return m_height; }
  bool     readJpeg(std::vector<uint8_t>& jpegOut) override;
  void     interrupt() override { m_interrupted = true; }

private:
  std::string       m_url;
  double            m_fps;
  cv::VideoCapture  m_cap;
  uint32_t          m_width{0};
  uint32_t          m_height{0};
  std::atomic<bool> m_interrupted{false};

  static constexpr int k_jpegQuality   = 75;
  static constexpr int k_reconnectWaitMs = 2000; ///< Wait before reconnect attempt
};

#endif
