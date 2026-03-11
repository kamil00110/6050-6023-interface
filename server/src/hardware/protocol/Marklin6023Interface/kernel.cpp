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

#include <boost/asio/post.hpp>
#include <chrono>

using namespace std::chrono_literals;

namespace Marklin6023 {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Kernel::Kernel(std::string logId, const Config& config,
               std::string device, uint32_t baudrate)
    : KernelBase{std::move(logId)}
    , m_config{config}
    , m_device{std::move(device)}
    , m_baudrate{baudrate}
    , m_s88Timer{m_ioContext}
{
}

Kernel::~Kernel() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Kernel::start()
{
    m_ioHandler = std::make_unique<IOHandler>(*this, m_ioContext, m_device, m_baudrate);

    if(m_config.s88amount > 0)
        startS88Cycle();

    KernelBase::start();
}

void Kernel::stop()
{
    boost::asio::post(m_strand,
        [this]()
        {
            m_s88Timer.cancel();
            m_redundancyTimers.clear();
            m_ioHandler.reset();
        });

    KernelBase::stop();
}

// ---------------------------------------------------------------------------
// Public command API  (posts onto the strand)
// ---------------------------------------------------------------------------

void Kernel::sendGlobalGo()
{
    boost::asio::post(m_strand, [this](){ sendCmdWithRedundancy("G"); });
}

void Kernel::sendGlobalStop()
{
    boost::asio::post(m_strand, [this](){ sendCmdWithRedundancy("S"); });
}

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    boost::asio::post(m_strand,
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
    // The 6023/6223 direction command does not carry an F0 state byte.
    boost::asio::post(m_strand,
        [this, address]()
        {
            sendCmd("L " + std::to_string(address) + " D");
        });
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool /*f0*/)
{
    // Double direction toggle: first stops, second restores direction.
    boost::asio::post(m_strand,
        [this, address]()
        {
            const std::string cmd = "L " + std::to_string(address) + " D";
            sendCmd(cmd);

            auto& t = m_redundancyTimers.emplace_back(m_ioContext);
            t.expires_after(50ms);
            t.async_wait(boost::asio::bind_executor(m_strand,
                [this, cmd](const boost::system::error_code& ec)
                {
                    if(!ec && m_ioHandler)
                        sendCmd(cmd);
                }));
        });
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    // Resend the speed command with the updated F0 bit.
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

    boost::asio::post(m_strand,
        [this, address, dir]()
        {
            sendCmdWithRedundancy(
                "M " + std::to_string(address) + " " + dir);
        });

    return true;
}

// ---------------------------------------------------------------------------
// IOHandler callbacks  (called on the strand)
// ---------------------------------------------------------------------------

void Kernel::receiveLine(std::string line)
{
    // The only responses we currently expect are S88 contact query replies ("0" or "1").
    if(m_s88WaitingReply)
    {
        onS88Response(line);
    }
    // Future: handle additional response types here.
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
// Internal send helpers  (must be on the strand)
// ---------------------------------------------------------------------------

void Kernel::sendCmd(std::string cmd)
{
    if(m_ioHandler)
        m_ioHandler->sendString(std::move(cmd) + CR);
}

void Kernel::sendCmdWithRedundancy(std::string cmd)
{
    sendCmd(cmd);

    for(unsigned int i = 0; i < m_config.redundancy; ++i)
    {
        auto& t = m_redundancyTimers.emplace_back(m_ioContext);
        t.expires_after(std::chrono::milliseconds(50u * (i + 1)));
        t.async_wait(boost::asio::bind_executor(m_strand,
            [this, cmd](const boost::system::error_code& ec)
            {
                if(!ec && m_ioHandler)
                    sendCmd(cmd);
            }));
    }
}

// ---------------------------------------------------------------------------
// S88 polling  (contact-by-contact, request/response style)
// ---------------------------------------------------------------------------

void Kernel::startS88Cycle()
{
    // Wait for the configured interval before starting the first (and each
    // subsequent) cycle so we do not hammer the CU immediately on connect.
    m_s88NextContact   = 1;
    m_s88WaitingReply  = false;

    m_s88Timer.expires_after(std::chrono::milliseconds(m_config.s88interval));
    m_s88Timer.async_wait(boost::asio::bind_executor(m_strand,
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

    const unsigned int totalContacts = m_config.s88amount * 16;
    if(m_s88NextContact > totalContacts)
    {
        // Cycle complete – restart after the poll interval
        startS88Cycle();
        return;
    }

    m_s88WaitingReply = true;
    sendCmd("C " + std::to_string(m_s88NextContact));
    // The response will arrive asynchronously via receiveLine() → onS88Response()
}

void Kernel::onS88Response(const std::string& line)
{
    m_s88WaitingReply = false;

    bool state = false;
    try
    {
        state = (std::stoi(line) != 0);
    }
    catch(...)
    {
        // Malformed response – skip this contact but continue the cycle
    }

    const uint32_t contact = m_s88NextContact;
    ++m_s88NextContact;

    if(s88Callback)
    {
        EventLoop::call(
            [this, contact, state]()
            {
                if(s88Callback)
                    s88Callback(contact, state);
            });
    }

    // Query the next contact immediately (no inter-contact delay needed;
    // the round-trip time acts as natural throttle).
    queryNextContact();
}

} // namespace Marklin6023
