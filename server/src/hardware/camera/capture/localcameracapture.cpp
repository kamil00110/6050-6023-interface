/**
 * server/src/hardware/camera/capture/localcameracapture.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#include "localcameracapture.hpp"
#include <chrono>
#include <thread>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

LocalCameraCapture::LocalCameraCapture(const std::string& device, double fps,
                                        uint32_t maxWidth, uint32_t maxHeight,
                                        int jpegQuality)
  : m_device(device)
  , m_fps(fps)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
  m_maxWidth    = maxWidth;
  m_maxHeight   = maxHeight;
  m_jpegQuality = jpegQuality;
}

LocalCameraCapture::~LocalCameraCapture() = default;

bool LocalCameraCapture::open()
{
  bool ok = false;
  try
  {
    const int idx = std::stoi(m_device);
#ifdef _WIN32
    ok = m_cap->open(idx, cv::CAP_DSHOW);
    if(!ok)
      ok = m_cap->open(idx, cv::CAP_MSMF);
#else
    ok = m_cap->open(idx, cv::CAP_ANY);
#endif
  }
  catch(const std::invalid_argument&)
  {
    ok = m_cap->open(m_device, cv::CAP_V4L2);
  }
  catch(const std::exception&)
  {
    return false;
  }

  if(!ok || !m_cap->isOpened())
    return false;

  m_cap->set(cv::CAP_PROP_FPS, m_fps);

  // Request source resolution matching our max constraints if set —
  // some cameras will honour this and save bandwidth on the USB bus.
  if(m_maxWidth > 0)
    m_cap->set(cv::CAP_PROP_FRAME_WIDTH,  static_cast<double>(m_maxWidth));
  if(m_maxHeight > 0)
    m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(m_maxHeight));

  m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  return true;
}

bool LocalCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  using namespace std::chrono;
  const auto framePeriod = duration_cast<microseconds>(duration<double>(1.0 / m_fps));
  cv::Mat frame;
  while(!m_interrupted)
  {
    const auto t0 = steady_clock::now();
    if(!m_cap->read(frame) || frame.empty())
      return false;
    if(!encodeFrame(frame, jpegOut))
      return false;
    const auto elapsed = steady_clock::now() - t0;
    if(elapsed < framePeriod)
      std::this_thread::sleep_for(framePeriod - elapsed);
    return true;
  }
  return false;
}
