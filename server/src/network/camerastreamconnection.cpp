/**
 * server/src/network/camerastreamconnection.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "camerastreamconnection.hpp"
#include "server.hpp"
#include "../hardware/camera/camera.hpp"
#include "../core/eventloop.hpp"
#include "../log/log.hpp"
#include <sstream>

// ─── MJPEG HTTP header sent once when the connection opens ───────────────────

static const std::string k_httpHeader =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
  "Cache-Control: no-cache, no-store, must-revalidate\r\n"
  "Pragma: no-cache\r\n"
  "Connection: close\r\n"
  "\r\n";

// ─── Constructor / Destructor ─────────────────────────────────────────────────

CameraStreamConnection::CameraStreamConnection(Server& server,
                                               boost::asio::ip::tcp::socket&& socket,
                                               std::shared_ptr<Camera> camera)
  : m_server(server)
  , m_stream(std::move(socket))
  , m_camera(std::move(camera))
{
}

CameraStreamConnection::~CameraStreamConnection()
{
  if(m_camera && m_subscriberId != 0)
    m_camera->removeFrameSubscriber(m_subscriberId);
}

// ─── Public API ───────────────────────────────────────────────────────────────

void CameraStreamConnection::start()
{
  // Subscribe to camera frames BEFORE sending the header so we never miss
  // the first frame that arrives while the header write is in flight.
  m_subscriberId = m_camera->addFrameSubscriber(
    [weak = weak_from_this()](std::vector<uint8_t> jpegData)
    {
      // Called from the camera capture thread — bounce to the IO thread.
      if(auto self = weak.lock())
      {
        self->m_stream.get_executor().post(
          [self, data = std::move(jpegData)]() mutable
          {
            self->enqueueFrame(std::move(data));
          },
          std::allocator<void>{});
      }
    });

  sendHttpHeader();
}

void CameraStreamConnection::close()
{
  if(m_camera && m_subscriberId != 0)
  {
    m_camera->removeFrameSubscriber(m_subscriberId);
    m_subscriberId = 0;
  }
  boost::system::error_code ec;
  m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void CameraStreamConnection::sendHttpHeader()
{
  // Ownership of the header string is captured in the lambda so it lives
  // until the async write completes.
  auto header = std::make_shared<std::string>(k_httpHeader);
  boost::asio::async_write(m_stream.socket(),
    boost::asio::buffer(*header),
    [self = shared_from_this(), header](boost::system::error_code ec, std::size_t)
    {
      if(ec)
      {
        self->close();
        return;
      }
      // Header sent — frame writes will now flow through enqueueFrame / doWrite
    });
}

void CameraStreamConnection::enqueueFrame(std::vector<uint8_t> jpegData)
{
  // Already on IO thread via the post() in the subscriber lambda.
  std::lock_guard<std::mutex> lock(m_writeMutex);
  m_writeQueue.push(buildMjpegChunk(jpegData));
  if(!m_writing)
  {
    m_writing = true;
    doWrite();
  }
}

void CameraStreamConnection::doWrite()
{
  // Must be called with m_writeMutex held, then immediately releases it.
  // Safe because doWrite() is always posted back to the same IO thread.

  if(m_writeQueue.empty())
  {
    m_writing = false;
    return;
  }

  // Take the front chunk by move to avoid holding the mutex across async_write
  std::vector<uint8_t> chunk;
  {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    chunk = std::move(m_writeQueue.front());
    m_writeQueue.pop();
  }

  auto buf = std::make_shared<std::vector<uint8_t>>(std::move(chunk));
  boost::asio::async_write(m_stream.socket(),
    boost::asio::buffer(*buf),
    [self = shared_from_this(), buf](boost::system::error_code ec, std::size_t)
    {
      if(ec)
      {
        self->close();
        return;
      }
      std::lock_guard<std::mutex> lock(self->m_writeMutex);
      if(!self->m_writeQueue.empty())
        self->doWrite();
      else
        self->m_writing = false;
    });
}

// ─── Static helpers ──────────────────────────────────────────────────────────

std::vector<uint8_t> CameraStreamConnection::buildMjpegChunk(const std::vector<uint8_t>& jpeg)
{
  std::ostringstream hdr;
  hdr << "--frame\r\n"
      << "Content-Type: image/jpeg\r\n"
      << "Content-Length: " << jpeg.size() << "\r\n"
      << "\r\n";
  const std::string h = hdr.str();

  std::vector<uint8_t> chunk;
  chunk.reserve(h.size() + jpeg.size() + 2);
  chunk.insert(chunk.end(), h.begin(), h.end());
  chunk.insert(chunk.end(), jpeg.begin(), jpeg.end());
  chunk.push_back('\r');
  chunk.push_back('\n');
  return chunk;
}
