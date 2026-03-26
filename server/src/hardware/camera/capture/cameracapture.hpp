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
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

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

  // Call this from subclass readJpeg() implementations after grabbing a frame,
  // before calling imencode. Scales down if the frame exceeds maxWidth/maxHeight
  // and re-encodes at the configured quality.
  bool encodeFrame(const cv::Mat& frame, std::vector<uint8_t>& jpegOut) const
  {
    cv::Mat out = frame;

    // Scale down if source exceeds requested maximum dimensions
    if((m_maxWidth > 0 || m_maxHeight > 0) && !frame.empty())
    {
      const uint32_t srcW = static_cast<uint32_t>(frame.cols);
      const uint32_t srcH = static_cast<uint32_t>(frame.rows);

      uint32_t dstW = srcW;
      uint32_t dstH = srcH;

      if(m_maxWidth > 0 && dstW > m_maxWidth)
      {
        dstW = m_maxWidth;
        dstH = static_cast<uint32_t>(
          static_cast<double>(srcH) * m_maxWidth / srcW);
      }
      if(m_maxHeight > 0 && dstH > m_maxHeight)
      {
        dstH = m_maxHeight;
        dstW = static_cast<uint32_t>(
          static_cast<double>(dstW) * m_maxHeight / dstH);
      }

      if(dstW != srcW || dstH != srcH)
        cv::resize(frame, out,
          cv::Size(static_cast<int>(dstW), static_cast<int>(dstH)),
          0, 0, cv::INTER_AREA);
    }

    const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, m_jpegQuality};
    return cv::imencode(".jpg", out, jpegOut, params);
  }
};
#endif
