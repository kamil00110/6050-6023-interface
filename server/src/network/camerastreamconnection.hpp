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
 */

#ifndef TRAINTASTIC_SERVER_NETWORK_CAMERASTREAMCONNECTION_HPP
#define TRAINTASTIC_SERVER_NETWORK_CAMERASTREAMCONNECTION_HPP

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
 * When the HTTP server detects a request for /camera/{id}/stream it creates
 * a CameraStreamConnection, moves the TCP socket into it, and registers it as
 * a frame subscriber on the matching Camera object.
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
  // camerastreamconnection.hpp
CameraStreamConnection(Server& server,
                       boost::asio::ip::tcp::socket&& socket,
                       std::string cameraId);   // ← id, not shared_ptr



  /** Send the HTTP 200 header and start reading frames. */
  void start();

  /** Called from the event loop when the camera is destroyed. */
  void close();

private:
  Server&                       m_server;
  boost::beast::tcp_stream      m_stream;
  std::shared_ptr<Camera>       m_camera;
  uint64_t                      m_subscriberId{0};

  std::mutex                    m_writeMutex;
  std::queue<std::vector<uint8_t>> m_writeQueue;
  bool                          m_writing{false};
  std::string             m_cameraId;         
  void sendHttpHeader();
  void enqueueFrame(std::vector<uint8_t> jpegData);
  void doWrite();

  static std::vector<uint8_t> buildMjpegChunk(const std::vector<uint8_t>& jpeg);
};

#endif
