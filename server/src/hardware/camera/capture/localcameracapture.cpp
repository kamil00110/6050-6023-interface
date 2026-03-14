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

LocalCameraCapture::LocalCameraCapture(const std::string& device, double fps)
  : m_device(device)
  , m_fps(fps)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
}

LocalCameraCapture::~LocalCameraCapture() = default;

bool LocalCameraCapture::open()
{
  bool ok = false;
  try
  {
    const int idx = std::stoi(m_device);

#ifdef _WIN32
    // Use DirectShow on Windows — MSMF (the default CAP_ANY backend on Windows)
    // causes stack corruption (BEX64 / 0xc0000409) with some drivers.
    ok = m_cap->open(idx, cv::CAP_DSHOW);
    if(!ok)
      ok = m_cap->open(idx, cv::CAP_MSMF); // fallback to MSMF if DSHOW fails
#else
    ok = m_cap->open(idx, cv::CAP_ANY);
#endif
  }
  catch(const std::invalid_argument&)
  {
    // device is a path string rather than an index
    ok = m_cap->open(m_device, cv::CAP_V4L2);
  }
  catch(const std::exception&)
  {
    return false;
  }

  if(!ok || !m_cap->isOpened())
    return false;

  m_cap->set(cv::CAP_PROP_FPS, m_fps);
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
