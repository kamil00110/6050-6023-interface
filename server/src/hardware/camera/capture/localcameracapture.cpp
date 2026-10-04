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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "localcameracapture.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <stdexcept>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>
#include <opencv2/imgcodecs.hpp>
#include "../../../log/log.hpp"

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
                                        int brightness, bool applyBrightness,
                                        Object& logObject)
  : m_device(device)
  , m_reqWidth(reqWidth)
  , m_reqHeight(reqHeight)
  , m_initBrightness(brightness)
  , m_applyBrightness(applyBrightness)
  , m_logObject(logObject)
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

  // Backends to try, in preference order. DirectShow first -- it is the backend
  // that actually delivers here, including the NVIDIA Broadcast virtual camera
  // (Media Foundation fails to grab that device). Media Foundation stays as a
  // fallback, chosen by the frame-content check below.
  std::vector<int> backends;
#ifdef _WIN32
  backends = {cv::CAP_DSHOW, cv::CAP_MSMF};
#else
  backends = numeric ? std::vector<int>{cv::CAP_ANY} : std::vector<int>{cv::CAP_V4L2};
#endif

  const auto backendName = [](int backend)
  {
    return cv::videoio_registry::getBackendName(static_cast<cv::VideoCaptureAPIs>(backend));
  };

  // Open the device on one backend and request the chosen size. The size is
  // decided up front by Camera (Auto resolves to a single safe size, 1280x720):
  // we open ONCE and never reopen to probe other sizes -- reopening in a tight
  // loop destabilises some virtual cameras (NVIDIA Broadcast goes permanently
  // black). A size of 0 means "leave the camera default".
  const auto openBackend = [&](int backend) -> bool
  {
    const bool ok = numeric ? m_cap->open(idx, backend)
                            : m_cap->open(m_device, backend);
    if(!ok || !m_cap->isOpened())
    {
      m_cap->release();
      return false;
    }
    if(m_reqWidth > 0 && m_reqHeight > 0)
    {
      m_cap->set(cv::CAP_PROP_FRAME_WIDTH,  static_cast<double>(m_reqWidth));
      m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(m_reqHeight));
    }
    m_cap->set(cv::CAP_PROP_FPS, m_fps.load());
    // Manual mode applies the requested brightness; auto mode forces neutral (0)
    // so a previously set value doesn't linger on the device.
    m_cap->set(cv::CAP_PROP_BRIGHTNESS,
               m_applyBrightness ? static_cast<double>(m_initBrightness) : 0.0);
    return true;
  };

  const auto finalizeSize = [this]()
  {
    m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
    m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  };

  // Open the first backend that delivers non-black frames. One open per backend
  // (no per-size reopening). A device may hand back a few black frames at start-up,
  // so warm up a few reads before judging a backend black.
  int fallbackBackend = -1;
  for(int backend : backends)
  {
    if(m_interrupted)
      return false;
    if(!openBackend(backend))
      continue;

    cv::Mat frame;
    bool hasContent = false;
    for(int i = 0; i < 15 && !m_interrupted; ++i)
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
      Log::log(m_logObject, LogMessage::I2010_CAMERA_CAPTURE_BACKEND_X, backendName(backend));
      return true;
    }
    if(fallbackBackend < 0) // opened but only black -- remember as a last resort
      fallbackBackend = backend;
    m_cap->release();
  }

  // Nothing produced real content; open the first backend that at least opened
  // and warn -- the usual "connected but black" case for virtual cameras.
  if(!m_interrupted && fallbackBackend >= 0 && openBackend(fallbackBackend))
  {
    finalizeSize();
    Log::log(m_logObject, LogMessage::I2010_CAMERA_CAPTURE_BACKEND_X, backendName(fallbackBackend));
    Log::log(m_logObject, LogMessage::W2030_CAMERA_OPENED_NO_IMAGE_X, backendName(fallbackBackend));
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
      // manually re-enabled), mirroring the reconnect the client-side IP capture
      // (IpCameraSource) does, so local cameras recover from a transient loss too.
      std::this_thread::sleep_for(std::chrono::milliseconds(reconnectWaitMs));
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
