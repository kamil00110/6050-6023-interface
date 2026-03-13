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
#include <memory>
#include <atomic>

// Forward-declare cv::VideoCapture so this header compiles without OpenCV.
// The actual #include is in localcameracapture.cpp only.
namespace cv { class VideoCapture; }

class LocalCameraCapture final : public CameraCapture
{
public:
  LocalCameraCapture(const std::string& device, double fps);
  ~LocalCameraCapture() override;  // needs to be in .cpp where cv:: is complete

  bool open()     override;
  uint32_t width()  const override { return m_width; }
  uint32_t height() const override { return m_height; }
  bool readJpeg(std::vector<uint8_t>& jpegOut) override;
  void interrupt() override { m_interrupted = true; }

private:
  std::string                      m_device;
  double                           m_fps;
  std::unique_ptr<cv::VideoCapture> m_cap;  // allocated in open()
  uint32_t                         m_width{0};
  uint32_t                         m_height{0};
  std::atomic<bool>                m_interrupted{false};

  static constexpr int k_jpegQuality = 75;
};

#endif
