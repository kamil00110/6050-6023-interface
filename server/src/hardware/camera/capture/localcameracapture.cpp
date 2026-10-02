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
                                        uint32_t reqWidth, uint32_t reqHeight,
                                        int jpegQuality, bool flipVertical, bool flipHorizontal,
                                        int brightness, int exposure)
  : m_device(device)
  , m_reqWidth(reqWidth)
  , m_reqHeight(reqHeight)
  , m_initBrightness(brightness)
  , m_initExposure(exposure)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
  m_fps.store(fps > 0.0 ? fps : 1.0);
  m_jpegQuality    = jpegQuality;
  m_flipVertical   = flipVertical;
  m_flipHorizontal = flipHorizontal;
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

  // Request capture settings best-effort; the camera honours what it supports.
  if(m_reqWidth > 0 && m_reqHeight > 0)
  {
    m_cap->set(cv::CAP_PROP_FRAME_WIDTH,  static_cast<double>(m_reqWidth));
    m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(m_reqHeight));
  }
  m_cap->set(cv::CAP_PROP_FPS, m_fps.load());
  if(m_initBrightness >= 0)
    m_cap->set(cv::CAP_PROP_BRIGHTNESS, static_cast<double>(m_initBrightness));
  if(m_initExposure >= 0)
  {
    m_cap->set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25); // 0.25 = manual (DirectShow/MSMF)
    m_cap->set(cv::CAP_PROP_EXPOSURE, static_cast<double>(m_initExposure));
  }

  m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  return true;
}

bool LocalCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  cv::Mat frame;
  while(!m_interrupted)
  {
    // Pick up any live brightness/exposure changes on the capture thread.
    applyLiveSettings(*m_cap);

    // grab() advances the source; retrieve() decodes. Frames that arrive
    // faster than the target rate are grabbed and dropped (not decoded),
    // which limits the frame rate instead of slowing the video down.
    if(!m_cap->grab())
      return false;

    if(!framePeriodElapsed())
      continue;

    if(!m_cap->retrieve(frame) || frame.empty())
      continue;

    return encodeFrame(frame, jpegOut);
  }
  return false;
}
