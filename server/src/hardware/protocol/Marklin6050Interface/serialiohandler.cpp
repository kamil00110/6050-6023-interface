/**
 * server/src/hardware/protocol/Marklin6050Interface/serialiohandler.cpp
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
#include <vector>

namespace Marklin6050 {

SerialIOHandler::SerialIOHandler(Kernel& kernel,
                                 boost::asio::io_context& ioContext,
                                 boost::asio::io_context::strand& strand,
                                 std::string device,
                                 uint32_t baudrate)
  : IOHandler{kernel}
  , m_strand{strand}
  , m_serialPort{ioContext}
  , m_device{std::move(device)}
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

void SerialIOHandler::send(std::initializer_list<uint8_t> bytes)
{
  auto buf = std::make_shared<std::vector<uint8_t>>(bytes);
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
    m_kernel.receive(m_readBuffer[i]);

  startRead();
}

} // namespace Marklin6050
