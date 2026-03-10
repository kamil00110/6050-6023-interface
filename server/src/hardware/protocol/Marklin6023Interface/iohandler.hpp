/**
 * server/src/hardware/protocol/Marklin6023/iohandler.hpp
 *
 * Serial IOHandler for the Märklin 6023/6223 ASCII kernel.
 * Owns the Boost.Asio serial_port and performs async line-oriented I/O.
 *
 * ASCII responses are CR or LF terminated; the handler accumulates bytes
 * until a terminator arrives and then delivers the complete line to the
 * kernel via Kernel::receiveLine().
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_IOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_IOHANDLER_HPP

#include <string>
#include <array>
#include <cstdint>
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6023 {

class Kernel;

class IOHandler final
{
public:
    IOHandler(Kernel& kernel,
              boost::asio::io_context& ioContext,
              const std::string& device,
              uint32_t baudrate);

    ~IOHandler();

    /** Send a raw string (including terminator) asynchronously. Thread-safe. */
    void sendString(std::string str);

private:
    static constexpr std::size_t kReadBufferSize = 256;

    Kernel&                                         m_kernel;
    boost::asio::io_context&                        m_ioContext;
    boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
    boost::asio::serial_port                        m_serialPort;

    std::array<uint8_t, kReadBufferSize>            m_readBuffer;
    std::string                                     m_lineBuffer;  ///< accumulates partial line

    void startRead();
    void onRead(const boost::system::error_code& ec, std::size_t bytesRead);
};

} // namespace Marklin6023

#endif
