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
#include <iomanip>
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
  #include <errno.h>
  #include <cstdlib>
  #include <cstring>
#endif

namespace
{
  // ── URL helpers ──────────────────────────────────────────────────────────

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
    const std::string ext = url.substr(url.size() - 5);
    return (ext == ".m3u8" || ext == ".M3U8");
  }

  bool isNetworkStream(const std::string& url)
  {
    return isRtsp(url) || isRtmp(url) || isHls(url) ||
           url.substr(0, 7) == "http://" ||
           url.substr(0, 8) == "https://";
  }

bool isRtsp(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "rtsp://" ||
            url.substr(0, 8) == "rtsps://");
  }

  bool isMjpeg(const std::string& url)
  {
    return url.size() >= 7 &&
           (url.substr(0, 7) == "http://" ||
            url.substr(0, 8) == "https://");
  }

  bool parseRtspHostPort(const std::string& url,
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
      port = 554;
    }
    return !host.empty();
  }

  // ── Backend list ─────────────────────────────────────────────────────────

  std::string availableBackends()
  {
    std::ostringstream oss;
    oss << "available backends:";
    for(auto b : cv::videoio_registry::getBackends())
      oss << " " << cv::videoio_registry::getBackendName(b);
    return oss.str();
  }

  // ── Socket helpers ───────────────────────────────────────────────────────

#ifdef _WIN32
  using SocketType = SOCKET;
  static constexpr SocketType kInvalidSocket = INVALID_SOCKET;
  void closeSocket(SocketType s) { closesocket(s); }
#else
  using SocketType = int;
  static constexpr SocketType kInvalidSocket = -1;
  void closeSocket(SocketType s) { close(s); }
#endif

  // Connect to host:port with a 3-second timeout.
  // Returns kInvalidSocket on failure, sets errOut to a description.
  SocketType connectTcp(const std::string& host, int port, std::string& errOut)
  {
#ifdef _WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
      errOut = "WSAStartup failed";
      return kInvalidSocket;
    }
#endif

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    const int gaErr = getaddrinfo(
      host.c_str(), std::to_string(port).c_str(), &hints, &res);
    if(gaErr != 0 || !res)
    {
      errOut = std::string("DNS resolution failed: ") + gai_strerror(gaErr);
#ifdef _WIN32
      WSACleanup();
#endif
      return kInvalidSocket;
    }

    // Log all resolved addresses
    {
      std::ostringstream oss;
      oss << "DNS resolved " << host << " -> ";
      for(struct addrinfo* ai = res; ai; ai = ai->ai_next)
      {
        char buf[INET6_ADDRSTRLEN]{};
        if(ai->ai_family == AF_INET)
          inet_ntop(AF_INET,
            &reinterpret_cast<sockaddr_in*>(ai->ai_addr)->sin_addr,
            buf, sizeof(buf));
        else if(ai->ai_family == AF_INET6)
          inet_ntop(AF_INET6,
            &reinterpret_cast<sockaddr_in6*>(ai->ai_addr)->sin6_addr,
            buf, sizeof(buf));
        oss << buf;
        if(ai->ai_next) oss << ", ";
      }
      errOut = oss.str(); // temporarily used as info output
    }

    const SocketType sock = socket(
      res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock == kInvalidSocket)
    {
      freeaddrinfo(res);
#ifdef _WIN32
      WSACleanup();
#endif
      errOut = "socket() failed";
      return kInvalidSocket;
    }

    // Non-blocking connect with 3s timeout
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
    connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    fd_set wset, eset;
    FD_ZERO(&wset); FD_SET(sock, &wset);
    FD_ZERO(&eset); FD_SET(sock, &eset);
    struct timeval tv{3, 0};
#ifdef _WIN32
    const int sel = select(0, nullptr, &wset, &eset, &tv);
#else
    const int sel = select(sock + 1, nullptr, &wset, &eset, &tv);
#endif
    if(sel <= 0 || FD_ISSET(sock, &eset))
    {
      closeSocket(sock);
#ifdef _WIN32
      WSACleanup();
#endif
      errOut = "TCP connect to " + host + ":" + std::to_string(port) +
               " timed out or refused";
      return kInvalidSocket;
    }

    // Restore blocking mode
#ifdef _WIN32
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sock, F_SETFL, 0);
#endif
    return sock;
  }

  // Send bytes on socket, return false on error
  bool sendAll(SocketType sock, const std::string& data)
  {
    size_t sent = 0;
    while(sent < data.size())
    {
#ifdef _WIN32
      const int n = send(sock, data.c_str() + sent,
                         static_cast<int>(data.size() - sent), 0);
      if(n == SOCKET_ERROR) return false;
#else
      const ssize_t n = send(sock, data.c_str() + sent,
                              data.size() - sent, 0);
      if(n <= 0) return false;
#endif
      sent += static_cast<size_t>(n);
    }
    return true;
  }

  // Read until CRLFCRLF or timeout (5s), return raw response
  std::string recvHeaders(SocketType sock)
  {
    std::string buf;
    buf.reserve(2048);
    char tmp[256];

    fd_set rset;
    while(true)
    {
      FD_ZERO(&rset);
      FD_SET(sock, &rset);
      struct timeval tv{5, 0};
#ifdef _WIN32
      if(select(0, &rset, nullptr, nullptr, &tv) <= 0) break;
      const int n = recv(sock, tmp, sizeof(tmp), 0);
      if(n <= 0) break;
#else
      if(select(sock + 1, &rset, nullptr, nullptr, &tv) <= 0) break;
      const ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
      if(n <= 0) break;
#endif
      buf.append(tmp, static_cast<size_t>(n));
      if(buf.find("\r\n\r\n") != std::string::npos)
        break;
      if(buf.size() > 8192)
        break;
    }
    return buf;
  }

  // ── Raw RTSP DESCRIBE probe ───────────────────────────────────────────────
  //
  // Sends a minimal RTSP DESCRIBE request and returns the full server
  // response. This tells us the exact status code and SDP body before
  // OpenCV tries anything — auth failures (401), codec info, redirect
  // targets (301/302), and server identification are all visible here.

  struct RtspProbeResult
  {
    bool     connected{false};
    bool     respondedToDescribe{false};
    int      statusCode{0};
    std::string statusLine;
    std::string headers;  // all response headers
    std::string sdp;      // SDP body if present
    std::string error;
    std::string dnsInfo;
  };

  RtspProbeResult rtspDescribeProbe(const std::string& url,
                                     const std::string& host,
                                     int port)
  {
    RtspProbeResult result;

    SocketType sock = connectTcp(host, port, result.error);
    result.dnsInfo = result.error; // connectTcp stores DNS info in errOut on success
    if(sock == kInvalidSocket)
      return result;

    result.connected = true;
    result.error.clear();

    // Send RTSP DESCRIBE
    // CSeq 1, no auth, Accept SDP
    const std::string req =
      "DESCRIBE " + url + " RTSP/1.0\r\n"
      "CSeq: 1\r\n"
      "User-Agent: Traintastic/1.0\r\n"
      "Accept: application/sdp\r\n"
      "\r\n";

    if(!sendAll(sock, req))
    {
      result.error = "send() failed for DESCRIBE request";
      closeSocket(sock);
#ifdef _WIN32
      WSACleanup();
#endif
      return result;
    }

    const std::string response = recvHeaders(sock);
    closeSocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif

    if(response.empty())
    {
      result.error = "no response to DESCRIBE (server closed connection immediately)";
      return result;
    }

    result.respondedToDescribe = true;
    result.headers = response;

    // Parse status line: RTSP/1.0 200 OK
    const size_t crlf = response.find("\r\n");
    result.statusLine = (crlf != std::string::npos)
      ? response.substr(0, crlf) : response;

    // Extract status code
    const size_t sp1 = result.statusLine.find(' ');
    if(sp1 != std::string::npos)
    {
      try { result.statusCode = std::stoi(result.statusLine.substr(sp1 + 1)); }
      catch(...) {}
    }

    // Extract SDP body (after CRLFCRLF)
    const size_t bodyStart = response.find("\r\n\r\n");
    if(bodyStart != std::string::npos)
      result.sdp = response.substr(bodyStart + 4);

    return result;
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

  // ── OpenCV version and build info ─────────────────────────────────────
  Log::log(m_logObject, LogMessage::I9999_X,
    std::string("OpenCV version: ") + cv::getVersionString());

  Log::log(m_logObject, LogMessage::I9999_X, availableBackends());

  // Log the Video I/O section of the build info only — the full string is
  // very long, so we skip lines until we hit "Video I/O" then log until
  // the next section header.
  {
    const std::string buildInfo = cv::getBuildInformation();
    std::istringstream ss(buildInfo);
    std::string line;
    bool inVideoIO = false;
    while(std::getline(ss, line))
    {
      if(line.find("Video I/O") != std::string::npos)
      {
        inVideoIO = true;
      }
      else if(inVideoIO && !line.empty() && line[0] != ' ' && line[0] != '\t')
      {
        // Hit the next top-level section — stop
        break;
      }

      if(inVideoIO && !line.empty())
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("buildinfo: ") + line);
    }
  }

  // ── Enable FFmpeg verbose stderr output ───────────────────────────────
#ifdef _WIN32
  _putenv_s("OPENCV_FFMPEG_DEBUG",    "1");
  _putenv_s("OPENCV_FFMPEG_LOGLEVEL", "48");
#else
  setenv("OPENCV_FFMPEG_DEBUG",    "1", 1);
  setenv("OPENCV_FFMPEG_LOGLEVEL", "48", 1);
#endif

  // ── URL parse diagnostics ─────────────────────────────────────────────
  if(isRtsp(m_url))
  {
    std::string host, path;
    int port = 554;
    if(parseRtspHostPort(m_url, host, port, path))
    {
      Log::log(m_logObject, LogMessage::I9999_X,
        std::string("URL parsed: host=[") + host +
        "] port=" + std::to_string(port) +
        " path=[" + path + "]");

      // ── DNS + TCP connectivity ────────────────────────────────────────
      std::string tcpErr;
      const SocketType probe = connectTcp(host, port, tcpErr);
      if(probe == kInvalidSocket)
      {
        Log::log(m_logObject, LogMessage::W9999_X,
          std::string("TCP connect FAILED: ") + tcpErr);
      }
      else
      {
        // tcpErr contains DNS info on success
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("TCP connect OK — ") + tcpErr);
        closeSocket(probe);
#ifdef _WIN32
        WSACleanup();
#endif

        // ── Raw RTSP DESCRIBE handshake ───────────────────────────────
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("sending raw RTSP DESCRIBE to check server response ..."));

        const RtspProbeResult rp = rtspDescribeProbe(m_url, host, port);

        if(!rp.dnsInfo.empty())
          Log::log(m_logObject, LogMessage::I9999_X, rp.dnsInfo);

        if(!rp.connected)
        {
          Log::log(m_logObject, LogMessage::W9999_X,
            std::string("RTSP probe: could not connect: ") + rp.error);
        }
        else if(!rp.respondedToDescribe)
        {
          Log::log(m_logObject, LogMessage::W9999_X,
            std::string("RTSP probe: connected but no response to DESCRIBE: ") + rp.error);
        }
        else
        {
          Log::log(m_logObject, LogMessage::I9999_X,
            std::string("RTSP DESCRIBE response status: [") + rp.statusLine + "]");

          // Log each response header line individually
          std::istringstream hss(rp.headers);
          std::string hline;
          while(std::getline(hss, hline))
          {
            // Strip trailing \r
            if(!hline.empty() && hline.back() == '\r')
              hline.pop_back();
            if(!hline.empty())
              Log::log(m_logObject, LogMessage::I9999_X,
                std::string("RTSP header: ") + hline);
          }

          // Log SDP if present
          if(!rp.sdp.empty())
          {
            std::istringstream sss(rp.sdp);
            std::string sline;
            while(std::getline(sss, sline))
            {
              if(!sline.empty() && sline.back() == '\r')
                sline.pop_back();
              if(!sline.empty())
                Log::log(m_logObject, LogMessage::I9999_X,
                  std::string("SDP: ") + sline);
            }
          }

          // Give a human-readable interpretation of the status code
          switch(rp.statusCode)
          {
            case 200:
              Log::log(m_logObject, LogMessage::I9999_X,
                std::string("RTSP server accepted DESCRIBE (200 OK) — "
                            "stream exists, codec details in SDP above"));
              break;
            case 401:
            case 403:
              Log::log(m_logObject, LogMessage::W9999_X,
                std::string("RTSP server requires authentication (")
                + std::to_string(rp.statusCode)
                + ") — add credentials to URL: "
                  "rtsp://user:password@host/path");
              break;
            case 404:
              Log::log(m_logObject, LogMessage::W9999_X,
                std::string("RTSP server says stream not found (404) — "
                            "check the path in the URL"));
              break;
            case 301:
            case 302:
              Log::log(m_logObject, LogMessage::W9999_X,
                std::string("RTSP server redirected (")
                + std::to_string(rp.statusCode)
                + ") — check Location header above");
              break;
            default:
              if(rp.statusCode > 0)
                Log::log(m_logObject, LogMessage::W9999_X,
                  std::string("RTSP server returned unexpected status: ")
                  + std::to_string(rp.statusCode));
              break;
          }
        }
      }
    }
    else
    {
      Log::log(m_logObject, LogMessage::W9999_X,
        std::string("could not parse host/port/path from URL: ") + m_url);
    }
  }

  // ── Attempt 1: GStreamer ──────────────────────────────────────────────
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
  else if(isRtsp(m_url) || isMjpeg(m_url))
  {
    Log::log(m_logObject, LogMessage::W9999_X,
      std::string("GStreamer backend not available — skipping"));
  }

  // ── Attempt 2: FFmpeg with timeout properties ─────────────────────────
  {
    Log::log(m_logObject, LogMessage::I9999_X,
      std::string("trying CAP_FFMPEG with open/read timeout 10s"));

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

  // ── Attempt 3: FFmpeg with explicit port in URL ───────────────────────
  if(isRtsp(m_url))
  {
    std::string host, path;
    int port = 554;
    if(parseRtspHostPort(m_url, host, port, path))
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
        Log::log(m_logObject, LogMessage::I9999_X,
          std::string("trying CAP_FFMPEG with explicit port url=[")
          + explicitUrl + "]");
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

  // ── All attempts failed ───────────────────────────────────────────────
  Log::log(m_logObject, LogMessage::E9999_X,
    std::string("all backends exhausted for url=[") + m_url + "]");
#ifdef _WIN32
  _putenv_s("OPENCV_FFMPEG_DEBUG",    "");
  _putenv_s("OPENCV_FFMPEG_LOGLEVEL", "");
#else
  unsetenv("OPENCV_FFMPEG_DEBUG");
  unsetenv("OPENCV_FFMPEG_LOGLEVEL");
#endif
  return false;

success:
#ifdef _WIN32
  _putenv_s("OPENCV_FFMPEG_DEBUG",    "");
  _putenv_s("OPENCV_FFMPEG_LOGLEVEL", "");
#else
  unsetenv("OPENCV_FFMPEG_DEBUG");
  unsetenv("OPENCV_FFMPEG_LOGLEVEL");
#endif
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

    const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, k_jpegQuality};
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
