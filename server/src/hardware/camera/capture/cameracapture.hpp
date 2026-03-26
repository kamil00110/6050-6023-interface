/**
 * server/src/hardware/camera/capture/cameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP

#include <cstdint>
#include <vector>

namespace cv { class Mat; }

class CameraCapture
{
public:
  virtual ~CameraCapture() = default;

  [[nodiscard]] virtual bool open() = 0;
  virtual uint32_t width()  const = 0;
  virtual uint32_t height() const = 0;
  [[nodiscard]] virtual bool readJpeg(std::vector<uint8_t>& jpegOut) = 0;
  virtual void interrupt() = 0;

protected:
  uint32_t m_maxWidth{0};
  uint32_t m_maxHeight{0};
  int      m_jpegQuality{75};

  // Implemented in cameracapture.cpp to keep OpenCV out of this header
  bool encodeFrame(const cv::Mat& frame, std::vector<uint8_t>& jpegOut) const;
};
#endif
