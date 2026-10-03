/**
 * server/src/hardware/camera/capture/ipcameracapture.cpp
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
#include "ipcameracapture.hpp"
#include <cctype>
#include <chrono>
#include <thread>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>
#include "../../../log/log.hpp"

namespace
{
  // ── URL classification ───────────────────────────────────────────────────

  bool isRtsp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtsp://"  ||
            url.substr(0, 8) == "rtsps://");
  }

  bool isRtmp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtmp://"  ||
            url.substr(0, 8) == "rtmps://" ||
            url.substr(0, 8) == "rtmpe://" ||
            url.substr(0, 8) == "rtmpt://");
  }

  bool isHls(const std::string& url)
  {
    if(url.size() < 5) return false;
    std::string ext = url.substr(url.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".m3u8";
  }

  bool isNetworkStream(const std::string& url)
  {
    return isRtsp(url) || isRtmp(url) || isHls(url) ||
           url.substr(0, 7) == "http://" ||
           url.substr(0, 8) == "https://";
  }

  // ── URL parsing ──────────────────────────────────────────────────────────

  bool parseHostPort(const std::string& url,
                     std::string& host,
                     int& port,
                     std::string& path)
  {
    const size_t schemeEnd = url.find("://");
    if(schemeEnd == std::string::npos) return false;
    std::string rest = url.substr(schemeEnd + 3);

    const size_t slash = rest.find('/');
    std::string authority = (slash != std::string::npos)
      ? rest.substr(0, slash) : rest;
    path = (slash != std::string::npos) ? rest.substr(slash) : "/";

    // Strip userinfo
    const size_t at = authority.rfind('@');
    if(at != std::string::npos)
      authority = authority.substr(at + 1);

    const size_t colon = authority.rfind(':');
    if(colon != std::string::npos)
    {
      host = authority.substr(0, colon);
      try { port = std::stoi(authority.substr(colon + 1)); }
      catch(...) { port = 554; }
    }
    else
    {
      host = authority;
      // Default ports per scheme
      if(url.substr(0, 7) == "rtsp://")        port = 554;
      else if(url.substr(0, 8) == "rtsps://")  port = 322;
      else if(url.substr(0, 7) == "rtmp://")   port = 1935;
      else if(url.substr(0, 7) == "http://")   port = 80;
      else if(url.substr(0, 8) == "https://")  port = 443;
      else                                      port = 554;
    }
    return !host.empty();
  }

  // ── GStreamer pipelines ───────────────────────────────────────────────────

  std::string gstreamerRtspPipeline(const std::string& url)
  {
    return "rtspsrc location=" + url +
           " protocols=4"
           " latency=200"
           " timeout=10000000000"
           " ! decodebin"
           " ! videoconvert"
           " ! video/x-raw,format=BGR"
           " ! appsink max-buffers=2 drop=true";
  }

  std::string gstreamerMjpegPipeline(const std::string& url)
  {
    return "souphttpsrc location=" + url +
           " ! multipartdemux"
           " ! image/jpeg"
           " ! jpegdec"
           " ! videoconvert"
           " ! video/x-raw,format=BGR"
           " ! appsink max-buffers=2 drop=true";
  }
}

// ─── IpCameraCapture ─────────────────────────────────────────────────────────

IpCameraCapture::IpCameraCapture(const std::string& url, double fps,
                                  uint32_t reqWidth, uint32_t reqHeight,
                                  int jpegQuality, bool flipVertical, bool flipHorizontal,
                                  bool appendSpecs,
                                  Object& logObject)
  : m_url(url)
  , m_cap(std::make_unique<cv::VideoCapture>())
  , m_logObject(logObject)
{
  m_fps.store(fps > 0.0 ? fps : 1.0);
  m_jpegQuality    = jpegQuality;
  m_flipVertical   = flipVertical;
  m_flipHorizontal = flipHorizontal;

  // Optionally ask the source for these specs via URL query parameters -- the
  // convention many MJPEG / IP cameras use. Only for HTTP(S) URLs.
  if(appendSpecs &&
     (m_url.rfind("http://", 0) == 0 || m_url.rfind("https://", 0) == 0))
  {
    std::string q = "fps=" + std::to_string(static_cast<int>(fps)) +
                    "&quality=" + std::to_string(jpegQuality);
    if(reqWidth > 0 && reqHeight > 0)
      q += "&res=" + std::to_string(reqWidth) + "x" + std::to_string(reqHeight);
    m_url += (m_url.find('?') == std::string::npos ? "?" : "&") + q;
  }
}

IpCameraCapture::~IpCameraCapture() = default;

bool IpCameraCapture::open()
{
  const auto finalize = [this]() -> bool
  {
    m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
    m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
    Log::log(m_logObject, LogMessage::I2010_CAMERA_CAPTURE_BACKEND_X,
      cv::videoio_registry::getBackendName(m_backend));
    return true;
  };

  // Attempt 1: GStreamer with a protocol-specific pipeline.
  if(cv::videoio_registry::hasBackend(cv::CAP_GSTREAMER) && isNetworkStream(m_url))
  {
    const std::string pipeline = isRtsp(m_url)
      ? gstreamerRtspPipeline(m_url)
      : gstreamerMjpegPipeline(m_url);
    if(m_cap->open(pipeline, cv::CAP_GSTREAMER) && m_cap->isOpened())
    {
      m_backend = cv::CAP_GSTREAMER;
      return finalize();
    }
    m_cap->release();
  }

  // Attempt 2: FFmpeg with open/read timeouts.
  {
    const std::vector<int> params{
      cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
      cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000,
    };
    if(m_cap->open(m_url, cv::CAP_FFMPEG, params) && m_cap->isOpened())
    {
      m_backend = cv::CAP_FFMPEG;
      return finalize();
    }
    m_cap->release();
  }

  // Attempt 3: FFmpeg again with the port made explicit in the URL -- some
  // FFmpeg builds need that for RTSP.
  if(isRtsp(m_url))
  {
    std::string host, path;
    int port = 554;
    if(parseHostPort(m_url, host, port, path))
    {
      const std::string explicitUrl = [&]() -> std::string
      {
        const size_t schemeEnd = m_url.find("://");
        if(schemeEnd == std::string::npos) return m_url;
        const std::string afterScheme = m_url.substr(schemeEnd + 3);
        const size_t slash = afterScheme.find('/');
        const std::string authority = (slash != std::string::npos)
          ? afterScheme.substr(0, slash) : afterScheme;
        if(authority.rfind(':') != std::string::npos)
          return m_url; // port already present
        return m_url.substr(0, schemeEnd + 3) + host + ":" +
               std::to_string(port) + path;
      }();

      if(explicitUrl != m_url)
      {
        const std::vector<int> params{
          cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
          cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000,
        };
        if(m_cap->open(explicitUrl, cv::CAP_FFMPEG, params) && m_cap->isOpened())
        {
          m_backend = cv::CAP_FFMPEG;
          return finalize();
        }
        m_cap->release();
      }
    }
  }

  // Attempt 4: let OpenCV pick any available backend.
  if(m_cap->open(m_url, cv::CAP_ANY) && m_cap->isOpened())
  {
    m_backend = cv::CAP_ANY;
    return finalize();
  }

  // All backends failed; Camera::captureLoop reports E2035 to the user.
  return false;
}

bool IpCameraCapture::readJpeg(std::vector<uint8_t>& jpegOut)
{
  using namespace std::chrono;

  cv::Mat frame;
  while(!m_interrupted)
  {
    // grab() pulls the next frame from the source; retrieve() decodes it.
    // Frames arriving faster than the target rate are grabbed and dropped
    // without decoding, so the frame rate is limited instead of the video
    // being slowed down / buffered into ever-growing latency.
    if(!m_cap->grab())
    {
      if(m_interrupted) return false;

      Log::log(m_logObject, LogMessage::W2029_CAMERA_STREAM_LOST_RECONNECTING);

      std::this_thread::sleep_for(milliseconds(k_reconnectWaitMs));
      m_cap->release();

      bool ok = false;
      if(m_backend == cv::CAP_GSTREAMER)
      {
        const std::string pipeline = isRtsp(m_url)
          ? gstreamerRtspPipeline(m_url)
          : gstreamerMjpegPipeline(m_url);
        ok = m_cap->open(pipeline, cv::CAP_GSTREAMER) && m_cap->isOpened();
      }
      else
      {
        const std::vector<int> params{
          cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
          cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000,
        };
        ok = m_cap->open(m_url, m_backend, params) && m_cap->isOpened();
      }

      if(!ok)
        return false; // Camera::captureLoopBody reports E2036 to the user

      Log::log(m_logObject, LogMessage::I2011_CAMERA_STREAM_RECONNECTED);
      // Reset the rate limiter so the first frame after a reconnect is kept.
      m_lastPublishTime = {};
      continue;
    }

    if(!framePeriodElapsed())
      continue; // too soon -- drop this frame to hold the target rate

    if(!m_cap->retrieve(frame) || frame.empty())
      continue;

    if(!encodeFrame(frame, jpegOut))
      return false;
    return true;
  }
  return false;
}
