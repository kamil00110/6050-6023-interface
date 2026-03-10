/**
 * server/src/hardware/protocol/Marklin6050/kernel.cpp
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
#include <cassert>

using namespace std::chrono_literals;

namespace Marklin6050 {

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
    , m_extensionTimer{m_ioContext}
{
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Kernel::start()
{
    // IOHandler constructor opens the port; throws LogMessageException on error.
    m_ioHandler = std::make_unique<IOHandler>(*this, m_ioContext, m_device, m_baudrate);

    // Start S88 polling if modules are configured
    if(m_config.s88amount > 0)
        scheduleS88Poll();

    // Start extension polling if requested
    if(m_config.extensions)
        scheduleExtensionPoll();

    // Run the io_context on the KernelBase background thread
    KernelBase::start();
}

void Kernel::stop()
{
    // Cancel timers and destroy the handler on the strand, then stop the thread.
    boost::asio::post(m_strand,
        [this]()
        {
            m_s88Timer.cancel();
            m_extensionTimer.cancel();
            m_redundancyTimers.clear();
            m_ioHandler.reset();
        });

    KernelBase::stop();  // joins the io_context thread
}

// ---------------------------------------------------------------------------
// Public command API  (posts onto the strand)
// ---------------------------------------------------------------------------

void Kernel::sendGlobalGo()
{
    boost::asio::post(m_strand, [this](){ sendWithRedundancy(GlobalGo); });
}

void Kernel::sendGlobalStop()
{
    boost::asio::post(m_strand, [this](){ sendWithRedundancy(GlobalStop); });
}

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    boost::asio::post(m_strand,
        [this, address, speed, f0]()
        {
            uint8_t cmd = speed & LocoSpeedMask;
            if(f0) cmd |= LocoF0Bit;
            sendWithRedundancy(cmd, address);
        });
}

void Kernel::setLocoDirection(uint8_t address, bool f0)
{
    // No redundancy – toggling twice cancels out
    boost::asio::post(m_strand,
        [this, address, f0]()
        {
            uint8_t cmd = LocoDirToggle;
            if(f0) cmd |= LocoF0Bit;
            sendRaw(cmd, address);
        });
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool f0)
{
    // Double direction-toggle: first stops, second restores direction.
    boost::asio::post(m_strand,
        [this, address, f0]()
        {
            uint8_t cmd = LocoDirToggle;
            if(f0) cmd |= LocoF0Bit;

            sendRaw(cmd, address);

            // Schedule the second toggle on a timer (50 ms later)
            auto& t = m_redundancyTimers.emplace_back(m_ioContext);
            t.expires_after(50ms);
            t.async_wait(boost::asio::bind_executor(m_strand,
                [this, cmd, address](const boost::system::error_code& ec)
                {
                    if(!ec && m_ioHandler)
                        sendRaw(cmd, address);
                }));
        });
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    boost::asio::post(m_strand,
        [this, address, currentSpeed, f0]()
        {
            uint8_t cmd = currentSpeed & LocoSpeedMask;
            if(f0) cmd |= LocoF0Bit;
            sendWithRedundancy(cmd, address);
        });
}

void Kernel::setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4)
{
    boost::asio::post(m_strand,
        [this, address, f1, f2, f3, f4]()
        {
            uint8_t cmd = FunctionBase;
            if(f1) cmd |= FunctionF1;
            if(f2) cmd |= FunctionF2;
            if(f3) cmd |= FunctionF3;
            if(f4) cmd |= FunctionF4;
            sendWithRedundancy(cmd, address);
        });
}

bool Kernel::setAccessory(uint32_t address, OutputValue value, unsigned int timeMs)
{
    if(address < 1 || address > 256)
        return false;

    uint8_t cmd = 0;
    std::visit(
        [&](auto&& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr(std::is_same_v<T, OutputPairValue>)
                cmd = (v == OutputPairValue::First) ? AccessoryRed : AccessoryGreen;
            else if constexpr(std::is_same_v<T, TriState>)
                cmd = (v == TriState::True) ? AccessoryRed : AccessoryGreen;
            else
                cmd = static_cast<uint8_t>(v);
        }, value);

    const uint8_t addr = static_cast<uint8_t>(address);
    const unsigned int redundancy = m_config.redundancy;

    boost::asio::post(m_strand,
        [this, cmd, addr, timeMs, redundancy]()
        {
            if(!m_ioHandler)
                return;

            // First activation
            sendRaw(cmd, addr);

            // Schedule redundant activations, then deactivation cycle
            // We chain timers: [50ms * i  activate] ... [timeMs  deactivate] [50ms * i  deactivate]
            auto scheduleRetransmits = [this, cmd, addr, redundancy, timeMs]()
            {
                unsigned int offset = 0;

                // --- redundant activations ---
                for(unsigned int i = 0; i < redundancy; ++i)
                {
                    offset += 50;
                    auto& t = m_redundancyTimers.emplace_back(m_ioContext);
                    t.expires_after(std::chrono::milliseconds(offset));
                    t.async_wait(boost::asio::bind_executor(m_strand,
                        [this, cmd, addr](const boost::system::error_code& ec)
                        {
                            if(!ec && m_ioHandler)
                                sendRaw(cmd, addr);
                        }));
                }

                // --- solenoid off after timeMs ---
                offset += timeMs;
                {
                    auto& t = m_redundancyTimers.emplace_back(m_ioContext);
                    t.expires_after(std::chrono::milliseconds(offset));
                    t.async_wait(boost::asio::bind_executor(m_strand,
                        [this, addr](const boost::system::error_code& ec)
                        {
                            if(!ec && m_ioHandler)
                                sendRaw(AccessoryOff, addr);
                        }));
                }

                // --- redundant deactivations ---
                for(unsigned int i = 0; i < redundancy; ++i)
                {
                    offset += 50;
                    auto& t = m_redundancyTimers.emplace_back(m_ioContext);
                    t.expires_after(std::chrono::milliseconds(offset));
                    t.async_wait(boost::asio::bind_executor(m_strand,
                        [this, addr](const boost::system::error_code& ec)
                        {
                            if(!ec && m_ioHandler)
                                sendRaw(AccessoryOff, addr);
                        }));
                }
            };

            scheduleRetransmits();
        });

    return true;
}

// ---------------------------------------------------------------------------
// IOHandler callbacks
// ---------------------------------------------------------------------------

void Kernel::receive(uint8_t byte)
{
    // Called on the strand by IOHandler.
    // Route the byte to whichever state machine is active.

    // --- S88 receive state machine ---
    if(m_s88State == S88State::ReceivingData)
    {
        // Bytes arrive as pairs (high, low) per S88 module word.
        const unsigned int byteIndex = (m_config.s88amount * 2) - m_s88Expect;
        --m_s88Expect;

        const bool isHighByte = (byteIndex % 2 == 0);
        if(isHighByte)
        {
            m_s88High = byte;
        }
        else
        {
            // Full 16-bit word received; fire callbacks
            const uint16_t bits =
                (static_cast<uint16_t>(m_s88High) << 8) |
                 static_cast<uint16_t>(byte);

            const unsigned int moduleIdx = m_s88Module++;

            if(s88Callback)
            {
                for(int bit = 0; bit < 16; ++bit)
                {
                    const bool state = (bits >> bit) & 1u;
                    const uint32_t address = moduleIdx * 16 + (bit + 1);

                    EventLoop::call(
                        [this, address, state]()
                        {
                            if(s88Callback)
                                s88Callback(address, state);
                        });
                }
            }
        }

        if(m_s88Expect == 0)
        {
            m_s88State  = S88State::Idle;
            m_s88Module = 0;
        }
        return;
    }

    // --- Extension receive state machine ---
    if(m_config.extensions && m_extState != ExtState::Idle)
    {
        processExtensionByte(byte);
        return;
    }
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

void Kernel::sendRaw(uint8_t b1, uint8_t b2)
{
    if(m_ioHandler)
        m_ioHandler->send({b1, b2});
}

void Kernel::sendRaw(uint8_t b)
{
    if(m_ioHandler)
        m_ioHandler->send({b});
}

void Kernel::sendWithRedundancy(uint8_t b)
{
    sendRaw(b);
    for(unsigned int i = 0; i < m_config.redundancy; ++i)
    {
        auto& t = m_redundancyTimers.emplace_back(m_ioContext);
        t.expires_after(std::chrono::milliseconds(50u * (i + 1)));
        t.async_wait(boost::asio::bind_executor(m_strand,
            [this, b](const boost::system::error_code& ec)
            {
                if(!ec && m_ioHandler)
                    sendRaw(b);
            }));
    }
}

void Kernel::sendWithRedundancy(uint8_t b1, uint8_t b2)
{
    sendRaw(b1, b2);
    for(unsigned int i = 0; i < m_config.redundancy; ++i)
    {
        auto& t = m_redundancyTimers.emplace_back(m_ioContext);
        t.expires_after(std::chrono::milliseconds(50u * (i + 1)));
        t.async_wait(boost::asio::bind_executor(m_strand,
            [this, b1, b2](const boost::system::error_code& ec)
            {
                if(!ec && m_ioHandler)
                    sendRaw(b1, b2);
            }));
    }
}

// ---------------------------------------------------------------------------
// S88 polling  (timer-based, strand-safe)
// ---------------------------------------------------------------------------

void Kernel::scheduleS88Poll()
{
    m_s88Timer.expires_after(std::chrono::milliseconds(m_config.s88interval));
    m_s88Timer.async_wait(boost::asio::bind_executor(m_strand,
        [this](const boost::system::error_code& ec)
        {
            if(ec || !m_ioHandler)
                return;
            doS88Poll();
            scheduleS88Poll();
        }));
}

void Kernel::doS88Poll()
{
    // Send the poll command: S88Base + module count
    const uint8_t cmd = S88Base + static_cast<uint8_t>(m_config.s88amount);
    sendRaw(cmd);

    // Arm the receive state machine
    m_s88State  = S88State::ReceivingData;
    m_s88Expect = m_config.s88amount * 2;
    m_s88Module = 0;
}

// ---------------------------------------------------------------------------
// Extension polling  (timer-based, strand-safe)
// ---------------------------------------------------------------------------

void Kernel::scheduleExtensionPoll()
{
    m_extensionTimer.expires_after(1s);
    m_extensionTimer.async_wait(boost::asio::bind_executor(m_strand,
        [this](const boost::system::error_code& ec)
        {
            if(ec || !m_ioHandler)
                return;
            doExtensionPoll();
            scheduleExtensionPoll();
        }));
}

void Kernel::doExtensionPoll()
{
    sendRaw(Extension::PollByte);
    sendRaw(Extension::PollByte);
    m_extState      = ExtState::WaitCount;
    m_extEventsLeft = 0;
}

// ---------------------------------------------------------------------------
// Extension byte processing  (inline helper; called from receive())
// ---------------------------------------------------------------------------

void Kernel::processExtensionByte(uint8_t byte)
{
    // This is intentionally a flat state machine rather than recursive calls,
    // so the stack stays shallow and all logic stays on the strand.
    switch(m_extState)
    {
        case ExtState::WaitCount:
            m_extEventsLeft = byte;
            m_extState = (byte > 0) ? ExtState::WaitType : ExtState::Idle;
            break;

        case ExtState::WaitType:
            switch(byte)
            {
                case Extension::EventGlobal:    m_extState = ExtState::GlobalData;    break;
                case Extension::EventTurnout:   m_extState = ExtState::TurnoutAddr;   break;
                case Extension::EventLocoState: m_extState = ExtState::LocoStateAddr; break;
                case Extension::EventLocoFunc:  m_extState = ExtState::LocoFuncAddr;  break;
                default:
                    // Unknown event type – cannot safely skip unknown length; abort polling.
                    m_extState = ExtState::Idle;
                    break;
            }
            break;

        case ExtState::GlobalData:
        {
            const bool power = byte & Extension::GlobalPowerBit;
            const bool run   = byte & Extension::GlobalRunBit;
            if(extensionGlobalCallback)
            {
                EventLoop::call(
                    [this, power, run]()
                    {
                        if(extensionGlobalCallback)
                            extensionGlobalCallback(power, run);
                    });
            }
            advanceExtensionEvent();
            break;
        }

        case ExtState::TurnoutAddr:
            m_extTmpAddr = byte;
            m_extState   = ExtState::TurnoutState;
            break;

        case ExtState::TurnoutState:
        {
            const uint32_t address = (m_extTmpAddr == 0) ? 256u
                                                           : static_cast<uint32_t>(m_extTmpAddr);
            const bool green = (byte != 0);
            if(extensionTurnoutCallback)
            {
                EventLoop::call(
                    [this, address, green]()
                    {
                        if(extensionTurnoutCallback)
                            extensionTurnoutCallback(address, green);
                    });
            }
            advanceExtensionEvent();
            break;
        }

        case ExtState::LocoStateAddr:
            m_extTmpAddr = byte;
            m_extState   = ExtState::LocoStateData;
            break;

        case ExtState::LocoStateData:
        {
            const uint8_t address = m_extTmpAddr;
            const uint8_t speed   = byte & Extension::LocoSpeedBits;
            const bool    f0      = byte & Extension::LocoF0Bit_Ext;
            const bool    forward = !(byte & Extension::LocoDirBit);
            if(extensionLocoCallback)
            {
                EventLoop::call(
                    [this, address, speed, f0, forward]()
                    {
                        if(extensionLocoCallback)
                            extensionLocoCallback(address, speed, f0, forward);
                    });
            }
            advanceExtensionEvent();
            break;
        }

        case ExtState::LocoFuncAddr:
            m_extTmpAddr = byte;
            m_extState   = ExtState::LocoFuncData;
            break;

        case ExtState::LocoFuncData:
        {
            const uint8_t address = m_extTmpAddr;
            const bool f1 = byte & Extension::LocoF1Bit;
            const bool f2 = byte & Extension::LocoF2Bit;
            const bool f3 = byte & Extension::LocoF3Bit;
            const bool f4 = byte & Extension::LocoF4Bit;
            if(extensionFuncCallback)
            {
                EventLoop::call(
                    [this, address, f1, f2, f3, f4]()
                    {
                        if(extensionFuncCallback)
                            extensionFuncCallback(address, f1, f2, f3, f4);
                    });
            }
            advanceExtensionEvent();
            break;
        }

        default:
            m_extState = ExtState::Idle;
            break;
    }
}

void Kernel::advanceExtensionEvent()
{
    if(--m_extEventsLeft > 0)
        m_extState = ExtState::WaitType;
    else
        m_extState = ExtState::Idle;
}

} // namespace Marklin6050
