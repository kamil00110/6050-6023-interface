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
#include <algorithm>
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
                                        std::vector<std::pair<uint32_t, uint32_t>> resolutions,
                                        int jpegQuality, bool flipVertical, bool flipHorizontal,
                                        int brightness, bool applyBrightness,
                                        Object& logObject)
  : m_device(device)
  , m_resolutions(std::move(resolutions))
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

  // Candidate sizes to try, largest-first. A {0,0} entry means "don't request a
  // size" (let the camera keep its own default).
  std::vector<std::pair<uint32_t, uint32_t>> candidates = m_resolutions;
  if(candidates.empty())
    candidates.push_back({0, 0});

  const auto backendName = [](int backend)
  {
    return cv::videoio_registry::getBackendName(static_cast<cv::VideoCaptureAPIs>(backend));
  };

  const auto openAt = [&](int backend, uint32_t cw, uint32_t ch) -> bool
  {
    const bool ok = numeric ? m_cap->open(idx, backend)
                            : m_cap->open(m_device, backend);
    if(!ok || !m_cap->isOpened())
    {
      m_cap->release();
      return false;
    }
    if(cw > 0 && ch > 0)
    {
      m_cap->set(cv::CAP_PROP_FRAME_WIDTH,  static_cast<double>(cw));
      m_cap->set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(ch));
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

  const auto deliversContent = [this]() -> bool
  {
    // A device may hand back a few black frames at start-up, so warm up a few
    // reads. Returns true as soon as a non-black frame arrives.
    cv::Mat frame;
    for(int i = 0; i < 15 && !m_interrupted; ++i)
    {
      if(m_cap->read(frame) && frameHasContent(frame))
        return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return false;
  };

  // For each backend (DirectShow first), probe every candidate size and record
  // the ones that actually deliver non-black frames (by their real, read-back
  // size). The resolution list the UI shows is built from exactly these verified
  // sizes -- so a camera like NVIDIA Broadcast, which accepts every size but only
  // delivers video at 1280x720, lists only 1280x720, not the sizes it renders
  // black. Trying all of a backend's sizes before moving on means the working
  // backend is found without probing the other (Media Foundation can't grab the
  // Broadcast camera at all). The largest usable size is used for capture.
  m_usableResolutions.clear();
  int fallbackBackend = -1;
  uint32_t fallbackW = 0, fallbackH = 0;
  for(int backend : backends)
  {
    std::vector<std::pair<uint32_t, uint32_t>> usable;
    for(const auto& [cw, ch] : candidates)
    {
      if(m_interrupted)
        return false;
      if(!openAt(backend, cw, ch))
        continue;
      if(deliversContent())
        usable.emplace_back(static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH)),
                            static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT)));
      else if(fallbackBackend < 0) // opened but only black -- remember as a last resort
      {
        fallbackBackend = backend;
        fallbackW = cw;
        fallbackH = ch;
      }
      m_cap->release();
    }

    if(!usable.empty())
    {
      // A camera may round several requests to the same actual size: deduplicate,
      // then sort largest-first and capture at the largest usable size.
      std::sort(usable.begin(), usable.end(),
        [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b)
        {
          const uint64_t pa = static_cast<uint64_t>(a.first) * a.second;
          const uint64_t pb = static_cast<uint64_t>(b.first) * b.second;
          return (pa != pb) ? (pa > pb) : (a > b); // area desc, then a total-order tiebreak
        });
      usable.erase(std::unique(usable.begin(), usable.end()), usable.end());
      m_usableResolutions = usable;

      const auto [uw, uh] = usable.front();
      if(openAt(backend, uw, uh))
      {
        finalizeSize();
        Log::log(m_logObject, LogMessage::I2010_CAMERA_CAPTURE_BACKEND_X, backendName(backend));
        return true;
      }
    }
  }

  // Nothing produced real content; open the first combination that at least
  // opened and warn -- the usual "connected but black" case for virtual cameras.
  if(!m_interrupted && fallbackBackend >= 0 && openAt(fallbackBackend, fallbackW, fallbackH))
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
