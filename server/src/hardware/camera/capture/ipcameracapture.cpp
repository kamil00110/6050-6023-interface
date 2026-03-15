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

#ifdef _WIN32
  #include <stdlib.h>  // _putenv_s
#else
  #include <cstdlib>   // setenv
#endif

namespace
{
  // Set OPENCV_FFMPEG_CAPTURE_OPTIONS to force TCP transport and a 10 s timeout.
  // FFmpeg uses UDP for RTSP by default; UDP is unreliable on many local networks
  // and is often blocked entirely. TCP is slower to set up but always works if
  // the server is reachable.
  // Format: key1;value1|key2;value2
  void setFfmpegRtspOptions()
  {
#ifdef _WIN32
    _putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS",
              "rtsp_transport;tcp|timeout;10000000");
#else
    setenv("OPENCV_FFMPEG_CAPTURE_OPTIONS",
           "rtsp_transport;tcp|timeout;10000000", 1);
#endif
  }

  void clearFfmpegOptions()
  {
#ifdef _WIN32
    _putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS", "");
#else
    unsetenv("OPENCV_FFMPEG_CAPTURE_OPTIONS");
#endif
  }

  bool isRtsp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtsp://" ||
            url.substr(0, 8) == "rtsps://");
  }
}

IpCameraCapture::IpCameraCapture(const std::string& url, double fps)
  : m_url(url)
  , m_fps(fps)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
}

IpCameraCapture::~IpCameraCapture() = default;

bool IpCameraCapture::open()
{
  // For RTSP streams force TCP transport + timeout via FFmpeg env var.
  // Must be set before cv::VideoCapture::open() is called.
  if(isRtsp(m_url))
    setFfmpegRtspOptions();

  bool ok = m_cap->open(m_url, cv::CAP_FFMPEG) && m_cap->isOpened();

  // Always clear the env var so it does not affect other captures
  clearFfmpegOptions();

  if(!ok)
  {
    // FFmpeg failed — try any available backend (e.g. GStreamer on Linux)
    m_cap->release();
    if(!m_cap->open(m_url, cv::CAP_ANY) || !m_cap->isOpened())
      return false;
  }

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

      // Reconnect with same TCP-first logic
      if(isRtsp(m_url))
        setFfmpegRtspOptions();

      bool ok = m_cap->open(m_url, cv::CAP_FFMPEG) && m_cap->isOpened();
      clearFfmpegOptions();

      if(!ok)
      {
        m_cap->release();
        if(!m_cap->open(m_url, cv::CAP_ANY) || !m_cap->isOpened())
          return false;
      }
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
