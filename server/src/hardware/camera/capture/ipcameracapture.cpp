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
#include <sstream>
#include <opencv2/videoio.hpp>
#include <opencv2/videoio/registry.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include "../../../log/log.hpp"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <stdlib.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <cstdlib>
#endif

namespace
{
  bool isRtsp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtsp://" ||
            url.substr(0, 8) == "rtsps://");
  }

  bool isMjpeg(const std::string& url)
  {
    // Simple heuristic — MJPEG streams are typically HTTP
    return url.size() >= 7 &&
           (url.substr(0, 7) == "http://" ||
            url.substr(0, 8) == "https://");
  }

  std::string availableBackends()
  {
    std::ostringstream oss;
    oss << "available backends:";
    for(auto b : cv::videoio_registry::getBackends())
      oss << " " << cv::videoio_registry::getBackendName(b);
    return oss.str();
  }

  bool parseRtspHostPort(const std::string& url, std::string& host, int& port)
  {
    size_t schemeEnd = url.find("://");
    if(schemeEnd == std::string::npos) return false;
    std::string rest = url.substr(schemeEnd + 3);

    size_t slash = rest.find('/');
    std::string authority = (slash != std::string::npos) ? rest.substr(0, slash) : rest;

    size_t at = authority.rfind('@');
    if(at != std::string::npos)
      authority = authority.substr(at + 1);

    size_t colon = authority.rfind(':');
    if(colon != std::string::npos)
    {
      host = authority.substr(0, colon);
      try { port = std::stoi(authority.substr(colon + 1)); }
      catch(...) { port = 554; }
    }
    else
    {
      host = authority;
      port = 554;
    }
    return !host.empty();
  }

  std::string tcpProbe(const std::string& host, int port)
  {
#ifdef _WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
      return "WSAStartup failed";
#endif
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
    {
#ifdef _WIN32
      WSACleanup();
#endif
      return "DNS resolution failed for " + host;
    }
#ifdef _WIN32
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock == INVALID_SOCKET) { freeaddrinfo(res); WSACleanup(); return "socket() failed"; }
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
    connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen));
    fd_set wset; FD_ZERO(&wset); FD_SET(sock, &wset);
    struct timeval tv{3, 0};
    int sel = select(0, nullptr, &wset, nullptr, &tv);
    closesocket(sock); freeaddrinfo(res); WSACleanup();
#else
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock < 0) { freeaddrinfo(res); return "socket() failed"; }
    fcntl(sock, F_SETFL, O_NONBLOCK);
    connect(sock, res->ai_addr, res->ai_addrlen);
    fd_set wset; FD_ZERO(&wset); FD_SET(sock, &wset);
    struct timeval tv{3, 0};
    int sel = select(sock + 1, nullptr, &wset, nullptr, &tv);
    close(sock); freeaddrinfo(res);
#endif
    if(sel <= 0)
      return "TCP connection to " + host + ":" + std::to_string(port) +
             " timed out or refused";
    return "";
  }

  // Build a GStreamer pipeline string for an RTSP URL.
  // Forces TCP transport and a 10-second connection timeout.
  std::string gstreamerRtspPipeline(const std::string& url)
  {
    // rtspsrc handles RTSP natively via GStreamer, no FFmpeg needed.
    // protocols=4 forces TCP (GST_RTSP_LOWER_TRANS_TCP = 4).
    // timeout is in nanoseconds (10s = 10000000000).
    return "rtspsrc location=" + url +
           " protocols=4"
           " latency=200"
           " timeout=10000000000"
           " ! decodebin"
           " ! videoconvert"
           " ! video/x-raw,format=BGR"
           " ! appsink max-buffers=2 drop=true";
  }

  // Build a GStreamer pipeline for an MJPEG HTTP stream.
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

IpCameraCapture::IpCameraCapture(const std::string& url, double fps, Object& logObject)
  : m_url(url)
  , m_fps(fps)
  , m_cap(std::make_unique<cv::VideoCapture>())
  , m_logObject(logObject)
{
}

IpCameraCapture::~IpCameraCapture() = default;

bool IpCameraCapture::open()
{
  Log::log(m_logObject, LogMessage::I9999_X,
    std::string("IpCameraCapture::open() url=[") + m_url +
    "] fps=" + std::to_string(m_fps));

  Log::log(m_logObject, LogMessage::I9999_X, availableBackends());

  // ── TCP reachability probe ────────────────────────────────────────────
  if(isRtsp(m_url))
  {
    std::string host; int port = 554;
    if(parseRtspHostPort(m_url, host, port))
    {
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("TCP probe -> ") + host + ":" + std::to_string(port));
      const std::string probeErr = tcpProbe(host, port);
      if(!probeErr.empty())
        Log::log(m_logObject, LogMessage::W9999_X,
          std::string("TCP probe FAILED: ") + probeErr);
      else
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("TCP probe OK: ") + host + ":" + std::to_string(port) + " reachable");
    }
  }

  // ── Attempt 1: GStreamer (most reliable for RTSP on Linux/Windows) ────
  const bool gstreamerAvailable = cv::videoio_registry::hasBackend(cv::CAP_GSTREAMER);
  if(gstreamerAvailable && (isRtsp(m_url) || isMjpeg(m_url)))
  {
    const std::string pipeline = isRtsp(m_url)
      ? gstreamerRtspPipeline(m_url)
      : gstreamerMjpegPipeline(m_url);

    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("trying CAP_GSTREAMER pipeline: ") + pipeline);

    if(m_cap->open(pipeline, cv::CAP_GSTREAMER) && m_cap->isOpened())
    {
      m_backend = cv::CAP_GSTREAMER;
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("CAP_GSTREAMER open() succeeded"));
      goto success;
    }
    Log::log(m_logObject, LogMessage::W9999_X,
      std::string("CAP_GSTREAMER open() failed"));
    m_cap->release();
  }
  else if(isRtsp(m_url))
  {
    Log::log(m_logObject, LogMessage::W9999_X,
      std::string("GStreamer backend not available — skipping"));
  }

  // ── Attempt 2: FFmpeg with explicit timeout property ──────────────────
  {
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("trying CAP_FFMPEG with open timeout 10s"));

    // Use the params-vector overload available in OpenCV 4.x.
    // CAP_PROP_OPEN_TIMEOUT_MSEC sets the FFmpeg connection timeout.
    const std::vector<int> params{
      cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
      cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000,
    };
    if(m_cap->open(m_url, cv::CAP_FFMPEG, params) && m_cap->isOpened())
    {
      m_backend = cv::CAP_FFMPEG;
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("CAP_FFMPEG open() succeeded"));
      goto success;
    }
    Log::log(m_logObject, LogMessage::W9999_X,
      std::string("CAP_FFMPEG open() failed"));
    m_cap->release();
  }

  // ── Attempt 3: FFmpeg with URL containing explicit port ───────────────
  if(isRtsp(m_url))
  {
    std::string host; int port = 554;
    if(parseRtspHostPort(m_url, host, port))
    {
      // Rebuild URL with explicit port if not already present
      const std::string explicitUrl = [&]() -> std::string
      {
        // Check if port already in URL
        const size_t schemeEnd = m_url.find("://");
        if(schemeEnd == std::string::npos) return m_url;
        const std::string afterScheme = m_url.substr(schemeEnd + 3);
        // Find host portion end
        const size_t slash = afterScheme.find('/');
        const std::string authority = (slash != std::string::npos)
          ? afterScheme.substr(0, slash) : afterScheme;
        // If authority already has a colon (port), don't add again
        if(authority.rfind(':') != std::string::npos)
          return m_url;
        // Insert :port
        const std::string path = (slash != std::string::npos)
          ? afterScheme.substr(slash) : "/";
        return m_url.substr(0, schemeEnd + 3) + host + ":" +
               std::to_string(port) + path;
      }();

      if(explicitUrl != m_url)
      {
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("trying CAP_FFMPEG with explicit port url=[") + explicitUrl + "]");
        const std::vector<int> params{
          cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 10000,
          cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000,
        };
        if(m_cap->open(explicitUrl, cv::CAP_FFMPEG, params) && m_cap->isOpened())
        {
          m_backend = cv::CAP_FFMPEG;
          Log::log(m_logObject, LogMessage::I9999_X,
            std::string("CAP_FFMPEG with explicit port succeeded"));
          goto success;
        }
        Log::log(m_logObject, LogMessage::W9999_X,
          std::string("CAP_FFMPEG with explicit port failed"));
        m_cap->release();
      }
    }
  }

  // ── Attempt 4: CAP_ANY last resort ───────────────────────────────────
  Log::log(m_logObject, LogMessage::I9999_X,
    std::string("trying CAP_ANY last resort"));
  if(m_cap->open(m_url, cv::CAP_ANY) && m_cap->isOpened())
  {
    m_backend = cv::CAP_ANY;
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("CAP_ANY open() succeeded"));
    goto success;
  }

  Log::log(m_logObject, LogMessage::E9999_X,
    std::string("all backends exhausted for url=[") + m_url + "]");
  return false;

success:
  m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));
  Log::log(m_logObject, LogMessage::I9999_X,
    std::string("open() success: ") +
    std::to_string(m_width) + "x" + std::to_string(m_height) +
    " backend=" + cv::videoio_registry::getBackendName(m_backend));
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

      Log::log(m_logObject, LogMessage::W9999_X,
        std::string("readJpeg: frame read failed, reconnecting in ") +
        std::to_string(k_reconnectWaitMs) + " ms");

      std::this_thread::sleep_for(milliseconds(k_reconnectWaitMs));
      m_cap->release();

      // Reconnect using whichever backend succeeded at open()
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
      {
        Log::log(m_logObject, LogMessage::E9999_X,
          std::string("readJpeg: reconnect failed — stopping capture"));
        return false;
      }
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("readJpeg: reconnect succeeded"));
      continue;
    }

    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, k_jpegQuality};
    if(!cv::imencode(".jpg", frame, jpegOut, params))
    {
      Log::log(m_logObject, LogMessage::E9999_X, std::string("readJpeg: imencode failed"));
      return false;
    }
    const auto elapsed = steady_clock::now() - t0;
    if(elapsed < framePeriod)
      std::this_thread::sleep_for(framePeriod - elapsed);
    return true;
  }
  return false;
}
