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
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include "../../../log/log.hpp"
#include "../../../log/logmessage.hpp"

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
    if(sock == INVALID_SOCKET)
    {
      freeaddrinfo(res);
      WSACleanup();
      return "socket() failed";
    }
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen));
    fd_set wset; FD_ZERO(&wset); FD_SET(sock, &wset);
    struct timeval tv{3, 0};
    int sel = select(0, nullptr, &wset, nullptr, &tv);
    closesocket(sock);
    freeaddrinfo(res);
    WSACleanup();
#else
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock < 0) { freeaddrinfo(res); return "socket() failed"; }
    fcntl(sock, F_SETFL, O_NONBLOCK);
    connect(sock, res->ai_addr, res->ai_addrlen);
    fd_set wset; FD_ZERO(&wset); FD_SET(sock, &wset);
    struct timeval tv{3, 0};
    int sel = select(sock + 1, nullptr, &wset, nullptr, &tv);
    close(sock);
    freeaddrinfo(res);
#endif
    if(sel <= 0)
      return "TCP connection to " + host + ":" + std::to_string(port) +
             " timed out or refused — try adding the correct port to the URL";
    return "";
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
    std::string host;
    int port = 554;
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
          std::string("TCP probe OK: ") + host + ":" + std::to_string(port) + " is reachable");
    }
    else
    {
      Log::log(m_logObject, LogMessage::W9999_X,
        std::string("could not parse host/port from URL: ") + m_url);
    }
  }

  // ── CAP_FFMPEG with TCP transport ─────────────────────────────────────
  if(isRtsp(m_url))
  {
    setFfmpegRtspOptions();
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("trying CAP_FFMPEG with rtsp_transport=tcp timeout=10s"));
  }
  else
  {
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("trying CAP_FFMPEG"));
  }

  bool ok = m_cap->open(m_url, cv::CAP_FFMPEG) && m_cap->isOpened();
  clearFfmpegOptions();

  if(ok)
  {
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("CAP_FFMPEG open() succeeded"));
  }
  else
  {
    Log::log(m_logObject, LogMessage::W9999_X,
      std::string("CAP_FFMPEG open() failed, trying CAP_ANY fallback"));
    m_cap->release();
    ok = m_cap->open(m_url, cv::CAP_ANY) && m_cap->isOpened();
    if(ok)
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("CAP_ANY open() succeeded"));
    else
    {
      Log::log(m_logObject, LogMessage::E9999_X,
        std::string("CAP_ANY open() failed — all backends exhausted for url=[") + m_url + "]");
      return false;
    }
  }

  m_width  = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_WIDTH));
  m_height = static_cast<uint32_t>(m_cap->get(cv::CAP_PROP_FRAME_HEIGHT));

  Log::log(m_logObject, LogMessage::I9999_X,
    std::string("open() success: ") +
    std::to_string(m_width) + "x" + std::to_string(m_height));

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

      if(isRtsp(m_url))
        setFfmpegRtspOptions();

      bool ok = m_cap->open(m_url, cv::CAP_FFMPEG) && m_cap->isOpened();
      clearFfmpegOptions();

      if(!ok)
      {
        Log::log(m_logObject, LogMessage::W9999_X,
          std::string("readJpeg: CAP_FFMPEG reconnect failed, trying CAP_ANY"));
        m_cap->release();
        if(!m_cap->open(m_url, cv::CAP_ANY) || !m_cap->isOpened())
        {
          Log::log(m_logObject, LogMessage::E9999_X,
            std::string("readJpeg: CAP_ANY reconnect failed — stopping capture"));
          return false;
        }
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("readJpeg: CAP_ANY reconnect succeeded"));
      }
      else
      {
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("readJpeg: CAP_FFMPEG reconnect succeeded"));
      }
      continue;
    }

    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, k_jpegQuality};
    if(!cv::imencode(".jpg", frame, jpegOut, params))
    {
      Log::log(m_logObject, LogMessage::E9999_X,
        std::string("readJpeg: imencode failed"));
      return false;
    }

    const auto elapsed = steady_clock::now() - t0;
    if(elapsed < framePeriod)
      std::this_thread::sleep_for(framePeriod - elapsed);
    return true;
  }
  return false;
}
