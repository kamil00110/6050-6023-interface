/**
 * server/src/hardware/protocol/Marklin6023Interface/serialiohandler.hpp
 *
 * Serial port IOHandler for the Märklin 6023/6223 kernel.
 * Accumulates incoming bytes into lines and delivers them via
 * Kernel::receiveLine().
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_SERIALIOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_SERIALIOHANDLER_HPP

#include "iohandler.hpp"

#include <array>
#include <cstdint>
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6023 {

class SerialIOHandler final : public IOHandler
{
public:
  SerialIOHandler(Kernel& kernel,
                  boost::asio::io_context& ioContext,
                  boost::asio::io_context::strand& strand,
                  const std::string& device,
                  uint32_t baudrate);

  ~SerialIOHandler() final;

  void start() final;
  void stop() final;
  void sendString(std::string str) final;

private:
  static constexpr std::size_t kReadBufferSize = 256;

  boost::asio::io_context::strand&     m_strand;
  boost::asio::serial_port             m_serialPort;
  const std::string                    m_device;
  const uint32_t                       m_baudrate;
  std::array<uint8_t, kReadBufferSize> m_readBuffer;
  std::string                          m_lineBuffer;

  void startRead();
  void onRead(const boost::system::error_code& ec, std::size_t bytesRead);
};

} // namespace Marklin6023

#endif
