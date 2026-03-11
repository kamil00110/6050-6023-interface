/**
 * server/src/hardware/protocol/Marklin6023/kernel.cpp
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "kernel.hpp"
#include "iohandler.hpp"
#include "protocol.hpp"
#include "../../../core/eventloop.hpp"
#include "../../../log/log.hpp"
#include "../../../log/logmessageexception.hpp"

#include <chrono>

using namespace std::chrono_literals;

namespace Marklin6023 {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Kernel::Kernel(std::string logId_, const Config& config,
               std::string device, uint32_t baudrate)
    : KernelBase{std::move(logId_)}
    , m_config{config}
    , m_device{std::move(device)}
    , m_baudrate{baudrate}
    , m_strand{m_ioContext}
    , m_s88Timer{m_ioContext}
{
}

Kernel::~Kernel() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Kernel::start()
{
    m_ioHandler = std::make_unique<IOHandler>(*this, m_ioContext, m_strand,
                                              m_device, m_baudrate);

    if(m_config.s88amount > 0)
        startS88Cycle();

    m_ioThread = std::thread([this](){ m_ioContext.run(); });
}

void Kernel::stop()
{
    m_strand.post(
        [this]()
        {
            m_s88Timer.cancel();
            m_redundancyTimers.clear();
            m_ioHandler.reset();
            m_ioContext.stop();
        });

    if(m_ioThread.joinable())
        m_ioThread.join();
}

// ---------------------------------------------------------------------------
// Public command API
// ---------------------------------------------------------------------------

void Kernel::sendGlobalGo()
{
    m_strand.post([this](){ sendCmdWithRedundancy("G"); });
}

void Kernel::sendGlobalStop()
{
    m_strand.post([this](){ sendCmdWithRedundancy("S"); });
}

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    m_strand.post(
        [this, address, speed, f0]()
        {
            sendCmdWithRedundancy(
                "L " + std::to_string(address) +
                " S " + std::to_string(speed & 0x0Fu) +
                " F " + (f0 ? "1" : "0"));
        });
}

void Kernel::setLocoDirection(uint8_t address, bool /*f0*/)
{
    // No redundancy – toggling twice cancels out.
    m_strand.post(
        [this, address]()
        {
            sendCmd("L " + std::to_string(address) + " D");
        });
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool /*f0*/)
{
    // Double direction-toggle: first stops, second restores direction.
    m_strand.post(
        [this, address]()
        {
            const std::string cmd = "L " + std::to_string(address) + " D";
            sendCmd(cmd);

            auto& t = m_redundancyTimers.emplace_back(m_ioContext);
            t.expires_after(50ms);
            t.async_wait(
                m_strand.wrap(
                    [this, cmd](const boost::system::error_code& ec)
                    {
                        if(!ec && m_ioHandler)
                            sendCmd(cmd);
                    }));
        });
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    setLocoSpeed(address, currentSpeed, f0);
}

bool Kernel::setAccessory(uint32_t address, OutputValue value)
{
    if(address < 1 || address > 256)
        return false;

    char dir = 'G';
    std::visit(
        [&](auto&& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr(std::is_same_v<T, OutputPairValue>)
                dir = (v == OutputPairValue::First) ? 'R' : 'G';
            else if constexpr(std::is_same_v<T, TriState>)
                dir = (v == TriState::True) ? 'R' : 'G';
        }, value);

    m_strand.post(
        [this, address, dir]()
        {
            sendCmdWithRedundancy("M " + std::to_string(address) + " " + dir);
        });

    return true;
}

// ---------------------------------------------------------------------------
// IOHandler callbacks  (arrive on m_strand)
// ---------------------------------------------------------------------------

void Kernel::receiveLine(std::string line)
{
    if(m_config.debugLogRXTX)
        EventLoop::call([this, msg = line](){ Log::log(logId, LogMessage::D2002_RX_X, msg); });

    if(m_s88WaitingReply)
        onS88Response(line);
}

void Kernel::onReadError(const boost::system::error_code& ec)
{
    EventLoop::call(
        [this, ec]()
        {
            Log::log(logId, LogMessage::E2002_SERIAL_READ_FAILED_X, ec);
        });
}

void Kernel::onWriteError(const boost::system::error_code& ec)
{
    EventLoop::call(
        [this, ec]()
        {
            Log::log(logId, LogMessage::E2001_SERIAL_WRITE_FAILED_X, ec);
        });
}

// ---------------------------------------------------------------------------
// Internal send helpers  (must be on m_strand)
// ---------------------------------------------------------------------------

void Kernel::sendCmd(std::string cmd)
{
    if(m_ioHandler)
    {
        if(m_config.debugLogRXTX)
            EventLoop::call([this, msg = cmd](){ Log::log(logId, LogMessage::D2001_TX_X, msg); });
        m_ioHandler->sendString(std::move(cmd) + CR);
    }
}

void Kernel::sendCmdWithRedundancy(std::string cmd)
{
    sendCmd(cmd);
    for(unsigned int i = 0; i < m_config.redundancy; ++i)
    {
        auto& t = m_redundancyTimers.emplace_back(m_ioContext);
        t.expires_after(std::chrono::milliseconds(50u * (i + 1)));
        t.async_wait(
            m_strand.wrap(
                [this, cmd](const boost::system::error_code& ec)
                {
                    if(!ec && m_ioHandler)
                        sendCmd(cmd);
                }));
    }
}

// ---------------------------------------------------------------------------
// S88 polling
// ---------------------------------------------------------------------------

void Kernel::startS88Cycle()
{
    m_s88NextContact  = 1;
    m_s88WaitingReply = false;

    m_s88Timer.expires_after(std::chrono::milliseconds(m_config.s88interval));
    m_s88Timer.async_wait(
        m_strand.wrap(
            [this](const boost::system::error_code& ec)
            {
                if(ec || !m_ioHandler)
                    return;
                queryNextContact();
            }));
}

void Kernel::queryNextContact()
{
    if(!m_ioHandler)
        return;

    const unsigned int total = m_config.s88amount * 16;
    if(m_s88NextContact > total)
    {
        startS88Cycle();
        return;
    }

    m_s88WaitingReply = true;
    sendCmd("C " + std::to_string(m_s88NextContact));
}

void Kernel::onS88Response(const std::string& line)
{
    m_s88WaitingReply = false;

    bool state = false;
    try { state = (std::stoi(line) != 0); }
    catch(...) {}

    const uint32_t contact = m_s88NextContact++;

    if(s88Callback)
    {
        EventLoop::call(
            [this, contact, state]()
            {
                if(s88Callback)
                    s88Callback(contact, state);
            });
    }

    queryNextContact();
}

} // namespace Marklin6023
