/**
 * server/src/network/httpconnection.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2024 Reinder Feenstra
 */

#include "httpconnection.hpp"
#include "server.hpp"
#include <boost/beast/http/read.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

HTTPConnection::HTTPConnection(std::shared_ptr<Server> server, boost::asio::ip::tcp::socket&& socket)
  : m_server{std::move(server)}
  , m_stream(std::move(socket))
{
}

void HTTPConnection::start()
{
  doRead();
}

void HTTPConnection::doRead()
{
  m_request = {};
  m_stream.expires_after(std::chrono::seconds(30));
  boost::beast::http::async_read(m_stream, m_buffer, m_request,
    [this, self = shared_from_this()](boost::beast::error_code readError, size_t /*bytesTransferred*/)
    {
      if(readError)
      {
        if(readError == boost::beast::http::error::end_of_stream)
          return doClose();
        return;
      }

      const bool keepAlive = m_request.keep_alive();

      if(boost::beast::websocket::is_upgrade(m_request))
      {
        if(!m_server->handleWebSocketUpgradeRequest(std::move(m_request), m_stream))
          self->doRead();
        return;
      }

      // Camera MJPEG stream — must be before handleHTTPRequest because it
      // takes ownership of the socket. Do not touch m_stream after this returns true.
      if(m_server->handleCameraStreamRequest(std::move(m_request), m_stream))
        return;

      auto response = m_server->handleHTTPRequest(std::move(m_request));
      boost::beast::async_write(m_stream, std::move(response),
        [self, keepAlive](boost::beast::error_code writeError, size_t /*bytesTransferred*/)
        {
          if(writeError)
            return;
          if(!keepAlive)
            return self->doClose();
          self->doRead();
        });
    });
}

void HTTPConnection::doClose()
{
  boost::beast::error_code ec;
  m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
}
