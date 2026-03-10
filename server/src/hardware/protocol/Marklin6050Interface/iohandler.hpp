/**
 * server/src/hardware/protocol/Marklin6050/iohandler.hpp
 *
 * Serial IOHandler for the Märklin 6050/6051 binary kernel.
 * Owns the Boost.Asio serial_port, performs all async reads/writes,
 * and forwards received bytes to the kernel via its receive() entry point.
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
#include <cstdint>
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6050 {

class Kernel;

/**
 * @brief Serial IOHandler for the 6050 binary protocol.
 *
 * Architecture notes
 * ------------------
 * - Constructed by Kernel::start(); destroyed by Kernel::stop().
 * - All I/O runs on the io_context strand held by the Kernel.
 * - send() posts a write onto the strand; concurrent calls are safe.
 * - Incoming bytes are delivered one-at-a-time to Kernel::receive().
 */
class IOHandler final
{
public:
    /**
     * @param kernel    Owning kernel – must outlive this handler.
     * @param ioContext io_context shared with the kernel.
     * @param device    Serial device path (e.g. "/dev/ttyUSB0").
     * @param baudrate  Baud rate (typically 2400 for 6050).
     */
    IOHandler(Kernel& kernel,
              boost::asio::io_context& ioContext,
              const std::string& device,
              uint32_t baudrate);

    ~IOHandler();

    /** Send raw bytes asynchronously (thread-safe). */
    void send(std::initializer_list<uint8_t> bytes);
    void send(const uint8_t* data, std::size_t length);

private:
    static constexpr std::size_t kReadBufferSize = 256;

    Kernel&                                         m_kernel;
    boost::asio::io_context&                        m_ioContext;
    boost::asio::strand<boost::asio::io_context::executor_type> m_strand;
    boost::asio::serial_port                        m_serialPort;
    std::array<uint8_t, kReadBufferSize>            m_readBuffer;

    void startRead();
    void onRead(const boost::system::error_code& ec, std::size_t bytesRead);
};

} // namespace Marklin6050

#endif
