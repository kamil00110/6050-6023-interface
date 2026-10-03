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
#include <vector>
#include <stdexcept>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

namespace
{
  // A frame whose channel means are all ~0 is pure black: the backend opened the
  // device but isn't actually delivering video. This happens with virtual cameras
  // (e.g. NVIDIA Broadcast) and some capture cards on the DirectShow backend,
  // where Media Foundation (MSMF) works instead.
  bool frameHasContent(const cv::Mat& frame)
  {
    if(frame.empty())
      return false;
    const cv::Scalar m = cv::mean(frame);
    return (m[0] + m[1] + m[2] + m[3]) > 1.0;
  }
}

LocalCameraCapture::LocalCameraCapture(const std::string& device, double fps,
                                        uint32_t reqWidth, uint32_t reqHeight,
                                        int jpegQuality, bool flipVertical, bool flipHorizontal,
                                        int brightness, bool applyBrightness)
  : m_device(device)
  , m_reqWidth(reqWidth)
  , m_reqHeight(reqHeight)
  , m_initBrightness(brightness)
  , m_applyBrightness(applyBrightness)
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
  // The device is either a numeric index (webcams, capture cards, virtual
  // cameras) or a path (V4L2 on Linux).
  int idx = 0;
  bool numeric = true;
  try { idx = std::stoi(m_device); }
  catch(const std::invalid_argument&) { numeric = false; }
  catch(const std::exception&) { return false; }

  // Backends to try, in preference order.
  std::vector<int> backends;
#ifdef _WIN32
  backends = {cv::CAP_DSHOW, cv::CAP_MSMF};
#else
  backends = numeric ? std::vector<int>{cv::CAP_ANY} : std::vector<int>{cv::CAP_V4L2};
#endif

  const auto applyRequestedSettings = [this]()
  {
    // Request capture settings best-effort; the camera honours what it supports.
    if(m_reqWidth > 0 && m_reqHeight > 0)
    {
      m_cap->set(cv::CAP_PROP_FRAME_WIDTH,  static_cast<double>(m_reqWidth));
      m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(m_reqHeight));
    }
    m_cap->set(cv::CAP_PROP_FPS, m_fps.load());
    if(m_applyBrightness) // false = auto-brightness: leave the camera at its default
      m_cap->set(cv::CAP_PROP_BRIGHTNESS, static_cast<double>(m_initBrightness));
  };

  const auto openBackend = [&](int backend) -> bool
  {
    const bool ok = numeric ? m_cap->open(idx, backend)
                            : m_cap->open(m_device, backend);
    if(!ok || !m_cap->isOpened())
    {
      m_cap->release();
      return false;
    }
    applyRequestedSettings();
    return true;
  };

  const auto finalizeSize = [this]()
  {
    m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
    m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  };

  // Prefer a backend that actually delivers non-black frames (some virtual
  // cameras / capture cards open on DirectShow but only produce black). Warm up
  // a few reads before judging -- the first frames can be black during start-up.
  int fallbackBackend = -1;
  for(int backend : backends)
  {
    if(m_interrupted)
      return false;
    if(!openBackend(backend))
      continue;

    cv::Mat frame;
    bool hasContent = false;
    for(int i = 0; i < 20 && !m_interrupted; ++i)
    {
      if(m_cap->read(frame) && frameHasContent(frame))
      {
        hasContent = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if(hasContent)
    {
      finalizeSize();
      return true;
    }
    if(fallbackBackend < 0)
      fallbackBackend = backend; // opened but only black -- remember as a fallback
    m_cap->release();
  }

  // No backend produced real content; the scene may genuinely be dark, so accept
  // the first backend that at least opened.
  if(!m_interrupted && fallbackBackend >= 0 && openBackend(fallbackBackend))
  {
    finalizeSize();
    return true;
  }
  return false;
}

bool LocalCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  cv::Mat frame;
  while(!m_interrupted)
  {
    // Pick up any live brightness changes on the capture thread.
    applyLiveSettings(*m_cap);

    // grab() advances the source; retrieve() decodes. Frames that arrive
    // faster than the target rate are grabbed and dropped (not decoded),
    // which limits the frame rate instead of slowing the video down.
    if(!m_cap->grab())
    {
      if(m_interrupted)
        return false;
      // The device hiccuped -- e.g. another camera sharing the same physical
      // device was removed, tearing down the shared capture graph. Re-open and
      // keep going instead of dying (which left the stream frozen until it was
      // manually re-enabled). This mirrors IpCameraCapture's reconnect, and is
      // why IP cameras already recovered but local ones did not.
      std::this_thread::sleep_for(std::chrono::milliseconds(k_reconnectWaitMs));
      m_cap->release();
      if(m_interrupted || !open())
        return false;
      m_lastPublishTime = {}; // keep the first frame after a reconnect
      continue;
    }

    if(!framePeriodElapsed())
      continue;

    if(!m_cap->retrieve(frame) || frame.empty())
      continue;

    return encodeFrame(frame, jpegOut);
  }
  return false;
}
