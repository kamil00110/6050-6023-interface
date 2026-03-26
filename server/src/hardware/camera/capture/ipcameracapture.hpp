/**
 * server/src/hardware/camera/capture/ipcameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_IPCAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_IPCAMERACAPTURE_HPP

#include "cameracapture.hpp"
#include <string>
#include <memory>
#include <atomic>

// cv::VideoCaptureAPIs is an enum so we need the real header, but only
// in this header to type m_backend. Keep it isolated here.
#include <opencv2/videoio.hpp>

class Object;
namespace cv { class VideoCapture; }

class IpCameraCapture final : public CameraCapture
{
public:
  IpCameraCapture(const std::string& url, double fps,
                  uint32_t maxWidth, uint32_t maxHeight,
                  int jpegQuality, Object& logObject);
  ~IpCameraCapture() override;

  bool     open()    override;
  uint32_t width()   const override { return m_width;  }
  uint32_t height()  const override { return m_height; }
  bool     readJpeg(std::vector<uint8_t>& jpegOut) override;
  void     interrupt() override { m_interrupted = true; }

private:
  std::string                       m_url;
  double                            m_fps;
  std::unique_ptr<cv::VideoCapture> m_cap;
  Object&                           m_logObject;
  cv::VideoCaptureAPIs              m_backend{cv::CAP_ANY};
  uint32_t                          m_width{0};
  uint32_t                          m_height{0};
  std::atomic<bool>                 m_interrupted{false};

  static constexpr int k_reconnectWaitMs = 2000;
};
#endif
