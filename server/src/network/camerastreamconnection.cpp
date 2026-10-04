/**
 * server/src/network/camerastreamconnection.cpp
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

#include "camerastreamconnection.hpp"
#include "server.hpp"
#include "../hardware/camera/camera.hpp"
#include "../core/eventloop.hpp"
#include <sstream>
#include <boost/asio/post.hpp>

static const std::string httpHeader =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
  "Cache-Control: no-cache, no-store, must-revalidate\r\n"
  "Pragma: no-cache\r\n"
  "Connection: close\r\n"
  "\r\n";

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
  if(m_subscriberId != 0)
    if(auto camera = m_camera.lock())
      camera->removeFrameSubscriber(m_subscriberId);
}

void CameraStreamConnection::start()
{
  // Hop onto the stream's own executor before touching the socket, so this is
  // safe to call from any thread.
  boost::asio::post(m_stream.get_executor(),
    [self = shared_from_this()]()
    {
      self->sendHttpHeader();
    });
}

void CameraStreamConnection::close()
{
  if(m_closed)
    return;
  m_closed = true;

  if(m_subscriberId != 0)
  {
    if(auto camera = m_camera.lock())
      camera->removeFrameSubscriber(m_subscriberId);
    m_subscriberId = 0;
  }

  boost::system::error_code ec;
  m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  m_stream.socket().close(ec);

  // Drop ourselves from the server registry on the event loop, where the map
  // lives. The raw pointer is only used as a lookup key, so it is safe even if
  // this is our last reference.
  Server* server = &m_server;
  CameraStreamConnection* key = this;
  EventLoop::call([server, key]() { server->cameraStreamGone(key); });
}

void CameraStreamConnection::sendHttpHeader()
{
  auto header = std::make_shared<std::string>(httpHeader);
  boost::asio::async_write(m_stream.socket(),
    boost::asio::buffer(*header),
    [self = shared_from_this(), header](boost::system::error_code ec, std::size_t)
    {
      if(ec)
      {
        self->close();
        return;
      }

      // Only subscribe once the header is on the wire, so a frame write can
      // never overlap the header write on the socket.
      auto camera = self->m_camera.lock();
      if(!camera)
      {
        self->close();
        return;
      }

      self->m_subscriberId = camera->addFrameSubscriber(
        [weak = self->weak_from_this()](std::vector<uint8_t> jpegData)
        {
          if(auto s = weak.lock())
          {
            boost::asio::post(s->m_stream.get_executor(),
              [s, data = std::move(jpegData)]() mutable
              {
                s->enqueueFrame(std::move(data));
              });
          }
        });
    });
}

void CameraStreamConnection::enqueueFrame(std::vector<uint8_t> jpegData)
{
  {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    auto chunk = buildMjpegChunk(std::move(jpegData));
    if(m_writing)
    {
      // A send is already in flight. MJPEG frames are independent, so instead of
      // letting the queue grow when the client can't keep up (which only adds
      // latency), keep ONLY the most recent frame queued -- drop any stale ones.
      while(!m_writeQueue.empty())
        m_writeQueue.pop();
      m_writeQueue.push(std::move(chunk));
      return;   // doWrite() will pick it up after the current send completes
    }
    m_writeQueue.push(std::move(chunk));
    m_writing = true;
  }
  doWrite();    // called WITHOUT the lock held
}

void CameraStreamConnection::doWrite()
{
  // Must NOT be called while holding m_writeMutex.
  std::vector<uint8_t> chunk;
  {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if(m_writeQueue.empty())
    {
      m_writing = false;
      return;
    }
    chunk = std::move(m_writeQueue.front());
    m_writeQueue.pop();
  }  // lock released before async_write

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
      self->doWrite();  // recurse -- no lock held here either
    });
}

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
