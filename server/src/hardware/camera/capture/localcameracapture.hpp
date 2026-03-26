/**
 * server/src/hardware/camera/capture/localcameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP

#include "cameracapture.hpp"
#include <string>
#include <memory>
#include <atomic>

namespace cv { class VideoCapture; }

class LocalCameraCapture final : public CameraCapture
{
public:
  LocalCameraCapture(const std::string& device, double fps,
                     uint32_t maxWidth, uint32_t maxHeight,
                     int jpegQuality);
  ~LocalCameraCapture() override;

  bool     open()    override;
  uint32_t width()   const override { return m_width;  }
  uint32_t height()  const override { return m_height; }
  bool     readJpeg(std::vector<uint8_t>& jpegOut) override;
  void     interrupt() override { m_interrupted = true; }

private:
  std::string                       m_device;
  double                            m_fps;
  std::unique_ptr<cv::VideoCapture> m_cap;
  uint32_t                          m_width{0};
  uint32_t                          m_height{0};
  std::atomic<bool>                 m_interrupted{false};
};
#endif
