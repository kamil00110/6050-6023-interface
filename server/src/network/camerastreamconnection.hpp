/**
 * server/src/network/camerastreamconnection.hpp
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

#ifndef TRAINTASTIC_SERVER_NETWORK_CAMERASTREAMCONNECTION_HPP
#define TRAINTASTIC_SERVER_NETWORK_CAMERASTREAMCONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast/core/tcp_stream.hpp>

class Server;
class Camera;

/**
 * @brief Persistent HTTP connection that streams MJPEG frames.
 *
 * When the HTTP server gets a request for /camera/{id}/stream it moves the TCP
 * socket into a CameraStreamConnection and (on the event loop) registers it as
 * a frame subscriber on the matching Camera object. The camera is held by
 * weak_ptr: a camera removed from the world is freed immediately, and this
 * connection simply stops receiving frames.
 *
 * Wire protocol:
 *   HTTP/1.1 200 OK
 *   Content-Type: multipart/x-mixed-replace; boundary=frame
 *
 *   --frame\r\n
 *   Content-Type: image/jpeg\r\n
 *   Content-Length: N\r\n
 *   \r\n
 *   <N JPEG bytes>
 *   \r\n
 *   (repeat)
 */
class CameraStreamConnection : public std::enable_shared_from_this<CameraStreamConnection>
{
  public:
    CameraStreamConnection(Server& server,
                           boost::asio::ip::tcp::socket&& socket,
                           std::shared_ptr<Camera> camera);
    ~CameraStreamConnection();

    /**
     * @brief Begin streaming.
     *
     * Hops onto the stream's executor, sends the HTTP header and only then
     * subscribes to frames, so a frame write can never race the header on the
     * socket. Safe to call from any thread.
     */
    void start();

    /**
     * @brief Stop streaming and tear the connection down (idempotent).
     *
     * Unsubscribes from the camera, shuts the socket down and removes this
     * connection from the server registry. Called on a socket error.
     */
    void close();

  private:
    Server&                          m_server;
    boost::beast::tcp_stream         m_stream;
    std::weak_ptr<Camera>            m_camera;
    uint64_t                         m_subscriberId{0};
    bool                             m_closed{false};

    std::mutex                       m_writeMutex;
    std::queue<std::vector<uint8_t>> m_writeQueue;
    bool                             m_writing{false};

    void sendHttpHeader();
    void enqueueFrame(std::vector<uint8_t> jpegData);
    void doWrite();

    static std::vector<uint8_t> buildMjpegChunk(const std::vector<uint8_t>& jpeg);
};

#endif
