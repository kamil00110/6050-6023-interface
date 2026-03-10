/**
 * server/src/hardware/protocol/Marklin6023/iohandler.cpp
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "iohandler.hpp"
#include "kernel.hpp"
#include "../../../utils/serialport.hpp"

#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/buffer.hpp>
#include <memory>

namespace Marklin6023 {

IOHandler::IOHandler(Kernel& kernel,
                     boost::asio::io_context& ioContext,
                     const std::string& device,
                     uint32_t baudrate)
    : m_kernel{kernel}
    , m_ioContext{ioContext}
    , m_strand{boost::asio::make_strand(ioContext)}
    , m_serialPort{ioContext}
{
    // Throws LogMessageException on failure – propagated to Kernel::start()
    SerialPort::open(m_serialPort, device, baudrate,
                     8, SerialParity::None, SerialStopBits::One, SerialFlowControl::None);

    startRead();
}

IOHandler::~IOHandler()
{
    boost::system::error_code ec;
    m_serialPort.cancel(ec);
    m_serialPort.close(ec);
}

// ---------------------------------------------------------------------------

void IOHandler::sendString(std::string str)
{
    auto buf = std::make_shared<std::string>(std::move(str));

    boost::asio::post(m_strand,
        [this, buf]()
        {
            if(!m_serialPort.is_open())
                return;

            boost::asio::async_write(
                m_serialPort,
                boost::asio::buffer(*buf),
                boost::asio::bind_executor(m_strand,
                    [this, buf](const boost::system::error_code& ec, std::size_t)
                    {
                        if(ec && ec != boost::asio::error::operation_aborted)
                            m_kernel.onWriteError(ec);
                    }));
        });
}

// ---------------------------------------------------------------------------

void IOHandler::startRead()
{
    if(!m_serialPort.is_open())
        return;

    m_serialPort.async_read_some(
        boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_strand,
            [this](const boost::system::error_code& ec, std::size_t bytesRead)
            {
                onRead(ec, bytesRead);
            }));
}

void IOHandler::onRead(const boost::system::error_code& ec, std::size_t bytesRead)
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
