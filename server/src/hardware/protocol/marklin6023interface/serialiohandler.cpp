/**
 * server/src/hardware/protocol/Marklin6023Interface/serialiohandler.cpp
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "serialiohandler.hpp"
#include "kernel.hpp"
#include "../../../utils/serialport.hpp"

#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <memory>

namespace Marklin6023 {

SerialIOHandler::SerialIOHandler(Kernel& kernel,
                                 boost::asio::io_context& ioContext,
                                 boost::asio::io_context::strand& strand,
                                 const std::string& device,
                                 uint32_t baudrate)
  : IOHandler{kernel}
  , m_strand{strand}
  , m_serialPort{ioContext}
  , m_device{device}
  , m_baudrate{baudrate}
{
}

SerialIOHandler::~SerialIOHandler()
{
  boost::system::error_code ec;
  m_serialPort.cancel(ec);
  m_serialPort.close(ec);
}

void SerialIOHandler::start()
{
  SerialPort::open(m_serialPort, m_device, m_baudrate,
                   8, SerialParity::None, SerialStopBits::One,
                   SerialFlowControl::None);
  startRead();
  m_kernel.started(); // port open — kernel is ready
}

void SerialIOHandler::stop()
{
  boost::system::error_code ec;
  m_serialPort.cancel(ec);
  m_serialPort.close(ec);
}

void SerialIOHandler::sendString(std::string str)
{
  auto buf = std::make_shared<std::string>(std::move(str));

  boost::asio::async_write(
    m_serialPort,
    boost::asio::buffer(*buf),
    m_strand.wrap(
      [this, buf](const boost::system::error_code& ec, std::size_t)
      {
        if(ec && ec != boost::asio::error::operation_aborted)
          m_kernel.onWriteError(ec);
      }));
}

void SerialIOHandler::startRead()
{
  if(!m_serialPort.is_open())
    return;

  m_serialPort.async_read_some(
    boost::asio::buffer(m_readBuffer),
    m_strand.wrap(
      [this](const boost::system::error_code& ec, std::size_t bytesRead)
      {
        onRead(ec, bytesRead);
      }));
}

void SerialIOHandler::onRead(const boost::system::error_code& ec, std::size_t bytesRead)
{
  if(ec)
  {
    if(ec != boost::asio::error::operation_aborted)
      m_kernel.onReadError(ec);
    return;
  }

  for(std::size_t i = 0; i < bytesRead; ++i)
  {
    const char c = static_cast<char>(m_readBuffer[i]);
    if(c == '\r' || c == '\n')
    {
      if(!m_lineBuffer.empty())
      {
        m_kernel.receiveLine(std::move(m_lineBuffer));
        m_lineBuffer.clear();
      }
    }
    else
    {
      m_lineBuffer += c;
    }
  }

  startRead();
}

} // namespace Marklin6023
