/**
 * server/src/hardware/protocol/Marklin6050/iohandler.hpp
 *
 * Serial IOHandler for the Märklin 6050/6051 binary kernel.
 * Owns the serial_port and delivers bytes asynchronously via Kernel::receive().
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_IOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_IOHANDLER_HPP

#include <string>
#include <array>
#include <vector>
#include <cstdint>
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6050 {

class Kernel;

class IOHandler final
{
public:
    IOHandler(Kernel& kernel,
              boost::asio::io_context& ioContext,
              boost::asio::io_context::strand& strand,
              const std::string& device,
              uint32_t baudrate);

    ~IOHandler();

    /** Send raw bytes asynchronously. Must be called on the strand. */
    void send(std::initializer_list<uint8_t> bytes);

private:
    static constexpr std::size_t kReadBufferSize = 256;

    Kernel&                          m_kernel;
    boost::asio::io_context::strand& m_strand;
    boost::asio::serial_port         m_serialPort;
    std::array<uint8_t, kReadBufferSize> m_readBuffer;

    void startRead();
    void onRead(const boost::system::error_code& ec, std::size_t bytesRead);
};

} // namespace Marklin6050

#endif
