/**
 * server/src/hardware/camera/capture/localcameracapture.hpp
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP

#include "cameracapture.hpp"
#include <string>
#include <atomic>
#include <opencv2/videoio.hpp>

/**
 * @brief Captures frames from a local USB / V4L2 device via OpenCV.
 *
 * The device can be specified either as a numeric index ("0", "1", …) or as
 * a device path ("/dev/video0").  On Windows the index maps directly to the
 * DirectShow or Media Foundation device list.
 */
class LocalCameraCapture final : public CameraCapture
{
public:
  /**
   * @param device  Device index as string ("0") or path ("/dev/video0").
   * @param fps     Target frame rate; best-effort, hardware dependent.
   */
  LocalCameraCapture(const std::string& device, double fps);

  bool open()     override;
  uint32_t width()  const override { return m_width; }
  uint32_t height() const override { return m_height; }
  bool readJpeg(std::vector<uint8_t>& jpegOut) override;
  void interrupt() override { m_interrupted = true; }

private:
  std::string          m_device;
  double               m_fps;
  cv::VideoCapture     m_cap;
  uint32_t             m_width{0};
  uint32_t             m_height{0};
  std::atomic<bool>    m_interrupted{false};

  // JPEG encoding quality 0–100
  static constexpr int k_jpegQuality = 75;
};

#endif
