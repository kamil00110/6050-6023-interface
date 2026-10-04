/**
 * client/src/widget/camera/capture/ipcamerasource.cpp
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
#include "ipcamerasource.hpp"
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h> // __try / __except / EXCEPTION_EXECUTE_HANDLER
#endif

namespace
{
  // URL classification (ported from the server's IpCameraCapture)

  bool isRtsp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtsp://" || url.substr(0, 8) == "rtsps://");
  }

  bool isNetworkStream(const std::string& url)
  {
    return isRtsp(url) ||
           url.substr(0, 7) == "rtmp://"  || url.substr(0, 8) == "rtmps://" ||
           url.substr(0, 7) == "http://"  || url.substr(0, 8) == "https://";
  }

  bool parseHostPort(const std::string& url, std::string& host, int& port, std::string& path)
  {
    const size_t schemeEnd = url.find("://");
    if(schemeEnd == std::string::npos)
      return false;
    const std::string rest = url.substr(schemeEnd + 3);

    const size_t slash = rest.find('/');
    std::string authority = (slash != std::string::npos) ? rest.substr(0, slash) : rest;
    path = (slash != std::string::npos) ? rest.substr(slash) : "/";

    const size_t at = authority.rfind('@'); // strip userinfo
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
      if(url.substr(0, 7) == "rtsp://")        port = 554;
      else if(url.substr(0, 8) == "rtsps://")  port = 322;
      else if(url.substr(0, 7) == "rtmp://")   port = 1935;
      else if(url.substr(0, 7) == "http://")   port = 80;
      else if(url.substr(0, 8) == "https://")  port = 443;
      else                                     port = 554;
    }
    return !host.empty();
  }

  std::string gstreamerRtspPipeline(const std::string& url)
  {
    return "rtspsrc location=" + url +
           " protocols=4 latency=200 timeout=10000000000"
           " ! decodebin ! videoconvert ! video/x-raw,format=BGR"
           " ! appsink max-buffers=2 drop=true";
  }

  std::string gstreamerMjpegPipeline(const std::string& url)
  {
    return "souphttpsrc location=" + url +
           " ! multipartdemux ! image/jpeg ! jpegdec ! videoconvert"
           " ! video/x-raw,format=BGR ! appsink max-buffers=2 drop=true";
  }

  // cv::Mat (BGR / grayscale, 8-bit) -> an OWNED QImage. The cv::Mat buffer is
  // reused on the next read, so the result must not alias it: rgbSwapped() and
  // copy() both return deep copies.
  QImage matToQImage(const cv::Mat& frame)
  {
    if(frame.empty())
      return {};
    if(frame.type() == CV_8UC3) // BGR (the usual VideoCapture output)
    {
      // Treat the BGR bytes as RGB, then swap R<->B; rgbSwapped() deep-copies.
      // (Format_RGB888 exists on every supported Qt, unlike Format_BGR888.)
      const QImage view(frame.data, frame.cols, frame.rows,
                        static_cast<int>(frame.step), QImage::Format_RGB888);
      return view.rgbSwapped();
    }
    if(frame.type() == CV_8UC1) // grayscale
    {
      const QImage view(frame.data, frame.cols, frame.rows,
                        static_cast<int>(frame.step), QImage::Format_Grayscale8);
      return view.copy();
    }
    return {}; // unsupported pixel format -- drop the frame
  }
}

IpCameraSource::IpCameraSource(const QString& url, double fps,
                               uint32_t reqWidth, uint32_t reqHeight, int jpegQuality,
                               bool flipVertical, bool flipHorizontal, bool appendSpecs,
                               QObject* parent)
  : QThread(parent)
  , m_url(url.toStdString())
  , m_fps(fps > 0.0 ? fps : 1.0)
  , m_flipVertical(flipVertical)
  , m_flipHorizontal(flipHorizontal)
  , m_cap(std::make_unique<cv::VideoCapture>())
{
  // Self-delete once the capture loop exits (set up before the thread can run).
  connect(this, &QThread::finished, this, &QObject::deleteLater);

  // Optionally ask an HTTP(S) source for these specs via query parameters -- the
  // convention many MJPEG / IP cameras use (mirrors the server's appendSpecs).
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

IpCameraSource::~IpCameraSource()
{
  // Safety net: never let the QThread be destroyed while still running.
  m_interrupted = true;
  if(isRunning())
    wait();
}

void IpCameraSource::stop()
{
  m_interrupted = true;
  // run() returns within the read timeout and then self-deletes (finished ->
  // deleteLater, wired in the ctor). Nothing to wait on here.
}

bool IpCameraSource::openCapture()
{
  const auto finalize = [this](int backend) -> bool
  {
    m_backend = backend;
    return true;
  };

  // Attempt 1: GStreamer with a protocol-specific pipeline.
  if(cv::videoio_registry::hasBackend(cv::CAP_GSTREAMER) && isNetworkStream(m_url))
  {
    const std::string pipeline = isRtsp(m_url)
      ? gstreamerRtspPipeline(m_url) : gstreamerMjpegPipeline(m_url);
    if(m_cap->open(pipeline, cv::CAP_GSTREAMER) && m_cap->isOpened())
      return finalize(cv::CAP_GSTREAMER);
    m_cap->release();
  }
  if(m_interrupted)
    return false;

  // Attempt 2: FFmpeg with open/read timeouts.
  {
    const std::vector<int> params{
      cv::CAP_PROP_OPEN_TIMEOUT_MSEC, openTimeoutMs,
      cv::CAP_PROP_READ_TIMEOUT_MSEC, readTimeoutMs,
    };
    if(m_cap->open(m_url, cv::CAP_FFMPEG, params) && m_cap->isOpened())
      return finalize(cv::CAP_FFMPEG);
    m_cap->release();
  }
  if(m_interrupted)
    return false;

  // Attempt 3: FFmpeg again with the port made explicit -- some FFmpeg builds
  // need that for RTSP.
  if(isRtsp(m_url))
  {
    std::string host, path;
    int port = 554;
    if(parseHostPort(m_url, host, port, path))
    {
      const std::string schemePrefix = m_url.substr(0, m_url.find("://") + 3);
      const std::string afterScheme = m_url.substr(schemePrefix.size());
      const size_t slash = afterScheme.find('/');
      const std::string authority = (slash != std::string::npos)
        ? afterScheme.substr(0, slash) : afterScheme;
      if(authority.rfind(':') == std::string::npos) // no port present
      {
        const std::string explicitUrl =
          schemePrefix + host + ":" + std::to_string(port) + path;
        const std::vector<int> params{
          cv::CAP_PROP_OPEN_TIMEOUT_MSEC, openTimeoutMs,
          cv::CAP_PROP_READ_TIMEOUT_MSEC, readTimeoutMs,
        };
        if(m_cap->open(explicitUrl, cv::CAP_FFMPEG, params) && m_cap->isOpened())
          return finalize(cv::CAP_FFMPEG);
        m_cap->release();
      }
    }
  }
  if(m_interrupted)
    return false;

  // Attempt 4: let OpenCV pick any available backend.
  if(m_cap->open(m_url, cv::CAP_ANY) && m_cap->isOpened())
    return finalize(cv::CAP_ANY);
  m_cap->release();
  return false;
}

bool IpCameraSource::framePeriodElapsed()
{
  const double fps = m_fps.load();
  if(fps <= 0.0)
    return true;
  const auto now = std::chrono::steady_clock::now();
  if(m_lastFrameTime.time_since_epoch().count() != 0)
  {
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / fps));
    if(now - m_lastFrameTime < period)
      return false;
  }
  m_lastFrameTime = now;
  return true;
}

void IpCameraSource::reconnectWait()
{
  // Bounded backoff before a reconnect (mirrors the server's pre-reopen sleep),
  // chunked so it stays responsive to stop().
  for(int i = 0; i < reconnectWaitMs / 100 && !m_interrupted; ++i)
    msleep(100);
}

// grab()/retrieve() run native FFmpeg/GStreamer decoder code that can raise a
// Windows structured exception (access violation) on a corrupt/truncated stream.
// Contain it here -- a trivial wrapper with no C++ objects needing unwinding, so
// MSVC does not reject the __try (C2712) -- and report it as a stream loss, just
// as the server's captureLoopBody did with __except.
int IpCameraSource::readFrameGuarded(cv::Mat& frame)
{
#ifdef _WIN32
  __try
  {
    return readFrameRaw(frame);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    return -1;
  }
#else
  return readFrameRaw(frame);
#endif
}

// -1 = stream lost (reconnect), 0 = no frame this tick (drop/again), 1 = frame.
int IpCameraSource::readFrameRaw(cv::Mat& frame)
{
  if(!m_cap->grab())
    return -1;
  if(!framePeriodElapsed())
    return 0; // too soon -- hold the target rate
  if(!m_cap->retrieve(frame) || frame.empty())
    return 0;
  return 1;
}

void IpCameraSource::run()
{
  // End-of-session cleanup: release the capture and, unless we're stopping, tell
  // the view and back off before the outer loop reconnects.
  const auto endSession = [this]()
  {
    if(m_cap)
      m_cap->release();
    if(!m_interrupted)
    {
      emit streamLost();
      reconnectWait();
    }
  };

  cv::Mat frame;
  while(!m_interrupted)
  {
    // Contain C++ exceptions (cv::Exception derives from std::exception) from the
    // OpenCV/FFmpeg/GStreamer backends. Crucially the try/catch is INSIDE the
    // loop: on a fault we reconnect rather than let run() return -- run() must
    // return only when m_interrupted, otherwise finished->deleteLater would
    // self-delete this object while the owner still holds its pointer (UAF). The
    // decoder's grab()/retrieve() are additionally SEH-guarded in
    // readFrameGuarded(). Mirrors the server's try/catch + __except.
    try
    {
      if(!openCapture())
      {
        if(m_interrupted)
          break;
        emit streamLost();
        reconnectWait();
        continue;
      }

      m_lastFrameTime = {};

      // grab() advances the source; retrieve() decodes. Frames arriving faster
      // than the target rate are grabbed and dropped without decoding, capping
      // the rate instead of growing latency.
      while(!m_interrupted)
      {
        const int r = readFrameGuarded(frame);
        if(r < 0)
          break;      // stream dropped -> reconnect via the outer loop
        if(r == 0)
          continue;   // no frame this tick

        const bool fv = m_flipVertical.load();
        const bool fh = m_flipHorizontal.load();
        QImage image;
        if(fv || fh)
        {
          const int flipCode = (fv && fh) ? -1 : (fv ? 0 : 1);
          cv::Mat flipped;
          cv::flip(frame, flipped, flipCode);
          image = matToQImage(flipped);
        }
        else
        {
          image = matToQImage(frame);
        }

        if(!image.isNull())
          emit frameReady(image);
      }

      endSession();
    }
    catch(...)
    {
      // Contained: release, report, back off, and reconnect on the next
      // iteration -- never return from run() on a fault.
      endSession();
    }
  }

  if(m_cap)
    m_cap->release();
}
