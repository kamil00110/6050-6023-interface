/**
 * server/src/hardware/camera/capture/ipcameracapture.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "ipcameracapture.hpp"
#include <chrono>
#include <thread>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

IpCameraCapture::IpCameraCapture(const std::string& url, double fps)
  : m_url(url)
  , m_fps(fps)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
}

IpCameraCapture::~IpCameraCapture() = default;  // cv::VideoCapture complete here

bool IpCameraCapture::open()
{
  if(!m_cap->open(m_url, cv::CAP_FFMPEG) || !m_cap->isOpened())
    return false;

  m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  return true;
}

bool IpCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  using namespace std::chrono;
  const auto framePeriod = duration_cast<microseconds>(duration<double>(1.0 / m_fps));

  cv::Mat frame;
  while(!m_interrupted)
  {
    const auto t0 = steady_clock::now();

    if(!m_cap->read(frame) || frame.empty())
    {
      if(m_interrupted) return false;
      std::this_thread::sleep_for(milliseconds(k_reconnectWaitMs));
      m_cap->release();
      if(!m_cap->open(m_url, cv::CAP_FFMPEG) || !m_cap->isOpened())
        return false;
      continue;
    }

    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, k_jpegQuality};
    if(!cv::imencode(".jpg", frame, jpegOut, params))
      return false;

    const auto elapsed = steady_clock::now() - t0;
    if(elapsed < framePeriod)
      std::this_thread::sleep_for(framePeriod - elapsed);

    return true;
  }
  return false;
}
