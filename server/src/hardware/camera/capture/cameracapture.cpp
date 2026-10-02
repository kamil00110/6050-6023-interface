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

bool CameraCapture::encodeFrame(const cv::Mat& frame,
                                 std::vector<uint8_t>& jpegOut) const
{
  cv::Mat out = frame;

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
      const uint32_t prevH = dstH;
      dstH = m_maxHeight;
      dstW = static_cast<uint32_t>(
        static_cast<double>(dstW) * m_maxHeight / prevH);
    }

    if(dstW != srcW || dstH != srcH)
      cv::resize(frame, out,
        cv::Size(static_cast<int>(dstW), static_cast<int>(dstH)),
        0, 0, cv::INTER_AREA);
  }

  // flipCode: 0 = vertical (around x-axis), >0 = horizontal (around y-axis), <0 = both
  if((m_flipVertical || m_flipHorizontal) && !out.empty())
  {
    const int flipCode = (m_flipVertical && m_flipHorizontal) ? -1
                       : (m_flipVertical ? 0 : 1);
    cv::Mat flipped;
    cv::flip(out, flipped, flipCode);
    out = flipped;
  }

  const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, m_jpegQuality};
  return cv::imencode(".jpg", out, jpegOut, params);
}
