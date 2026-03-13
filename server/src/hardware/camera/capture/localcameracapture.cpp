/**
 * server/src/hardware/camera/capture/localcameracapture.cpp
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

#include "localcameracapture.hpp"
#include <chrono>
#include <thread>
#include <opencv2/imgcodecs.hpp>

LocalCameraCapture::LocalCameraCapture(const std::string& device, double fps)
  : m_device(device)
  , m_fps(fps)
{
}

bool LocalCameraCapture::open()
{
  // Accept either a numeric index or a device path
  bool ok = false;
  try
  {
    const int idx = std::stoi(m_device);
    ok = m_cap.open(idx, cv::CAP_ANY);
  }
  catch(const std::invalid_argument&)
  {
    ok = m_cap.open(m_device, cv::CAP_V4L2);
  }

  if(!ok || !m_cap.isOpened())
    return false;

  // Request the desired frame-rate; cameras may ignore this
  m_cap.set(cv::CAP_PROP_FPS, m_fps);

  m_width  = static_cast<uint32_t>(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));

  return true;
}

bool LocalCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  // Period between frames based on requested fps
  using namespace std::chrono;
  const auto framePeriod = duration_cast<microseconds>(duration<double>(1.0 / m_fps));

  cv::Mat frame;
  while(!m_interrupted)
  {
    const auto t0 = steady_clock::now();

    if(!m_cap.read(frame) || frame.empty())
      return false;

    // Encode to JPEG in-memory
    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, k_jpegQuality};
    if(!cv::imencode(".jpg", frame, jpegOut, params))
      return false;

    // Sleep for the remainder of the frame period
    const auto elapsed = steady_clock::now() - t0;
    if(elapsed < framePeriod)
      std::this_thread::sleep_for(framePeriod - elapsed);

    return true; // caller loops
  }
  return false; // interrupted
}
