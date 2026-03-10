/**
 * server/src/hardware/protocol/Marklin6050Interface/kernel.cpp
 *
 * Kernel supporting both binary (6050) and ASCII (6023/6223) protocols.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "kernel.hpp"
#include "../../../utils/serialport.hpp"
#include "../../../core/eventloop.hpp"
#include "../../../log/log.hpp"
#include "../../../log/logmessageexception.hpp"
#include "../../../core/eventloop.hpp"

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <thread>
#include <chrono>
#include <vector>

using namespace Marklin6050;

// --- Construction / Destruction ---

Kernel::Kernel(std::string logId_, const Config& config)
    : logId{std::move(logId_)}
    , m_config{config}
    , m_serialPort{m_ioContext}
{
}

Kernel::~Kernel()
{
    stop();
}

// --- Lifecycle ---

void Kernel::start(const std::string& device, uint32_t baudrate)
{
    SerialPort::open(m_serialPort, device, baudrate, 8,
        SerialParity::None, SerialStopBits::One, SerialFlowControl::None);
}

void Kernel::stop()
{
    stopInputThread();

    if(m_serialPort.is_open())
    {
        boost::system::error_code ec;
        m_serialPort.close(ec);
    }
}

// === Low-level I/O ===

bool Kernel::sendByte(uint8_t byte)
{
    if(!m_serialPort.is_open())
        return false;

    boost::system::error_code ec;
    boost::asio::write(m_serialPort, boost::asio::buffer(&byte, 1), ec);

    if(ec)
    {
        EventLoop::call(
            [this, ec]()
            {
                Log::log(logId, LogMessage::E2001_SERIAL_WRITE_FAILED_X, ec);
            });
        return false;
    }
    return true;
}

bool Kernel::sendString(const std::string& str)
{
    if(!m_serialPort.is_open())
        return false;

    boost::system::error_code ec;
    boost::asio::write(m_serialPort, boost::asio::buffer(str), ec);

    if(ec)
    {
        EventLoop::call(
            [this, ec]()
            {
                Log::log(logId, LogMessage::E2001_SERIAL_WRITE_FAILED_X, ec);
            });
        return false;
    }
    return true;
}

int Kernel::readByte()
{
    if(!m_serialPort.is_open())
        return -1;

    uint8_t byte;
    boost::system::error_code ec;
    boost::asio::read(m_serialPort, boost::asio::buffer(&byte, 1), ec);

    if(ec)
    {
        EventLoop::call(
            [this, ec]()
            {
                Log::log(logId, LogMessage::E2002_SERIAL_READ_FAILED_X, ec);
            });
        return -1;
    }
    return byte;
}

std::string Kernel::readLine()
{
    std::string line;
    while(m_serialPort.is_open())
    {
        int b = readByte();
        if(b < 0)
            return {};
        if(b == AsciiCR || b == '\n')
        {
            if(!line.empty())
                return line;
            continue; // skip leading CR/LF
        }
        line += static_cast<char>(b);
    }
    return {};
}

// === Binary protocol helpers (6050) ===

void Kernel::sendBinaryCommand(uint8_t byte1, uint8_t byte2)
{
    sendByte(byte1);
    sendByte(byte2);
}

// === ASCII protocol helpers (6023/6223) ===

void Kernel::sendAsciiCommand(const std::string& cmd)
{
    sendString(cmd + AsciiCR);
}

// === Global commands ===

bool Kernel::sendGlobalGo()
{
    if(m_config.protocolMode == ProtocolMode::ASCII)
        return sendString(std::string("G") + AsciiCR);
    else
        return sendByte(Marklin6050::GlobalGo);
}

bool Kernel::sendGlobalStop()
{
    if(m_config.protocolMode == ProtocolMode::ASCII)
        return sendString(std::string("S") + AsciiCR);
    else
        return sendByte(Marklin6050::GlobalStop);
}

// === Loco commands ===

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    if(m_config.protocolMode == ProtocolMode::ASCII)
    {
        // L x S y F z
        std::string cmd = "L " + std::to_string(address)
            + " S " + std::to_string(speed & LocoSpeedMask)
            + " F " + (f0 ? "1" : "0");
        sendAsciiCommand(cmd);

        if(m_config.redundancy > 0)
        {
            std::thread([this, cmd, count = m_config.redundancy]()
            {
                for(unsigned int i = 0; i < count; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if(!m_serialPort.is_open()) return;
                    sendAsciiCommand(cmd);
                }
            }).detach();
        }
    }
    else
    {
        uint8_t cmd = speed & LocoSpeedMask;
        if(f0)
            cmd |= LocoF0Bit;

        sendBinaryCommand(cmd, address);

        if(m_config.redundancy > 0)
        {
            std::thread([this, cmd, address, count = m_config.redundancy]()
            {
                for(unsigned int i = 0; i < count; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if(!m_serialPort.is_open()) return;
                    sendBinaryCommand(cmd, address);
                }
            }).detach();
        }
    }
}

void Kernel::setLocoDirection(uint8_t address, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    if(m_config.protocolMode == ProtocolMode::ASCII)
    {
        // L x D — no redundancy (toggle)
        sendAsciiCommand("L " + std::to_string(address) + " D");
    }
    else
    {
        uint8_t cmd = LocoDirToggle;
        if(f0)
            cmd |= LocoF0Bit;

        // no redundancy — toggling twice cancels out
        sendBinaryCommand(cmd, address);
    }
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    if(m_config.protocolMode == ProtocolMode::ASCII)
    {
        // double direction toggle
        std::string cmd = "L " + std::to_string(address) + " D";
        sendAsciiCommand(cmd);

        std::thread([this, cmd]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if(!m_serialPort.is_open()) return;
            sendAsciiCommand(cmd);
        }).detach();
    }
    else
    {
        uint8_t cmd = LocoDirToggle;
        if(f0)
            cmd |= LocoF0Bit;

        sendBinaryCommand(cmd, address);

        std::thread([this, cmd, address]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if(!m_serialPort.is_open()) return;
            sendBinaryCommand(cmd, address);
        }).detach();
    }
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    if(m_config.protocolMode == ProtocolMode::ASCII)
    {
        // resend speed with updated F0
        std::string cmd = "L " + std::to_string(address)
            + " S " + std::to_string(currentSpeed & LocoSpeedMask)
            + " F " + (f0 ? "1" : "0");
        sendAsciiCommand(cmd);

        if(m_config.redundancy > 0)
        {
            std::thread([this, cmd, count = m_config.redundancy]()
            {
                for(unsigned int i = 0; i < count; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if(!m_serialPort.is_open()) return;
                    sendAsciiCommand(cmd);
                }
            }).detach();
        }
    }
    else
    {
        uint8_t cmd = currentSpeed & LocoSpeedMask;
        if(f0)
            cmd |= LocoF0Bit;

        sendBinaryCommand(cmd, address);

        if(m_config.redundancy > 0)
        {
            std::thread([this, cmd, address, count = m_config.redundancy]()
            {
                for(unsigned int i = 0; i < count; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if(!m_serialPort.is_open()) return;
                    sendBinaryCommand(cmd, address);
                }
            }).detach();
        }
    }
}

void Kernel::setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4)
{
    // F1-F4 only supported in binary mode (6050)
    if(m_config.protocolMode == ProtocolMode::ASCII)
        return;

    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = FunctionBase;
    if(f1) cmd |= FunctionF1;
    if(f2) cmd |= FunctionF2;
    if(f3) cmd |= FunctionF3;
    if(f4) cmd |= FunctionF4;

    sendBinaryCommand(cmd, address);

    if(m_config.redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_config.redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_serialPort.is_open()) return;
                sendBinaryCommand(cmd, address);
            }
        }).detach();
    }
}

// === Accessory commands ===

bool Kernel::setAccessory(uint32_t address, OutputValue value, unsigned int timeMs)
{
    if(!m_serialPort.is_open() || address < 1 || address > 256)
        return false;

    if(m_config.protocolMode == ProtocolMode::ASCII)
    {
        // determine direction
        char dir = 'G'; // green/straight

        std::visit([&](auto&& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, OutputPairValue>)
                dir = (v == OutputPairValue::First) ? 'R' : 'G';
            else if constexpr (std::is_same_v<T, TriState>)
                dir = (v == TriState::True) ? 'R' : 'G';
        }, value);

        // ASCII mode: CU handles timing internally
        std::string cmd = "M " + std::to_string(address) + " " + dir;
        sendAsciiCommand(cmd);

        if(m_config.redundancy > 0)
        {
            std::thread([this, cmd, count = m_config.redundancy]()
            {
                for(unsigned int i = 0; i < count; i++)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if(!m_serialPort.is_open()) return;
                    sendAsciiCommand(cmd);
                }
            }).detach();
        }

        return true;
    }
    else
    {
        // binary mode: need activate + deactivate cycle
        unsigned char cmd = 0;

        std::visit([&](auto&& v)
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, OutputPairValue>)
                cmd = (v == OutputPairValue::First) ? AccessoryRed : AccessoryGreen;
            else if constexpr (std::is_same_v<T, TriState>)
                cmd = (v == TriState::True) ? AccessoryRed : AccessoryGreen;
            else
                cmd = static_cast<unsigned char>(v);
        }, value);

        uint8_t addr = static_cast<uint8_t>(address);

        sendBinaryCommand(cmd, addr);

        std::thread([this, cmd, addr, timeMs, count = m_config.redundancy]()
        {
            // first cycle: deactivate
            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            if(!m_serialPort.is_open()) return;
            sendBinaryCommand(AccessoryOff, addr);

            // redundant cycles: full activate → wait → deactivate
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_serialPort.is_open()) return;
                sendBinaryCommand(cmd, addr);

                std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
                if(!m_serialPort.is_open()) return;
                sendBinaryCommand(AccessoryOff, addr);
            }
        }).detach();

        return true;
    }
}

// === S88 input polling ===

void Kernel::startInputThread(unsigned int moduleCount, unsigned int intervalMs)
{
    if(m_running)
        return;

    m_running = true;

    m_inputThread = std::thread([this, moduleCount, intervalMs]()
    {
        while(m_running)
        {
            if(m_config.protocolMode == ProtocolMode::ASCII)
                asciiInputLoop(moduleCount);
            else
                binaryInputLoop(moduleCount);

            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    });
}

void Kernel::stopInputThread()
{
    if(!m_running)
        return;

    m_running = false;
    if(m_inputThread.joinable())
        m_inputThread.join();
}

void Kernel::binaryInputLoop(unsigned int modules)
{
    if(!m_running || !m_serialPort.is_open() || modules == 0)
        return;

    uint8_t cmd = S88Base + static_cast<uint8_t>(modules);

    if(!sendByte(cmd))
        return;

    const unsigned int totalBytes = modules * 2;
    std::vector<uint8_t> buffer(totalBytes);

    for(unsigned int i = 0; i < totalBytes; i++)
    {
        int b = readByte();
        if(b < 0)
            return;
        buffer[i] = static_cast<uint8_t>(b);
    }

    for(unsigned int m = 0; m < modules; m++)
    {
        uint16_t bits =
            (static_cast<uint16_t>(buffer[m * 2]) << 8) |
             static_cast<uint16_t>(buffer[m * 2 + 1]);

        for(int bit = 0; bit < 16; bit++)
        {
            bool state = bits & (1 << bit);
            uint32_t address = m * 16 + (bit + 1);

            if(s88Callback)
            {
                EventLoop::call(
                    [this, address, state]()
                    {
                        if(s88Callback)
                            s88Callback(address, state);
                    });
            }
        }
    }
}

void Kernel::asciiInputLoop(unsigned int modules)
{
    if(!m_running || !m_serialPort.is_open() || modules == 0)
        return;

    // poll each contact individually using C x command
    const unsigned int totalContacts = modules * 16;

    for(unsigned int contact = 1; contact <= totalContacts; contact++)
    {
        if(!m_running || !m_serialPort.is_open())
            return;

        sendAsciiCommand("C " + std::to_string(contact));

        std::string response = readLine();
        if(response.empty())
            return;

        bool state = false;
        try
        {
            state = (std::stoi(response) != 0);
        }
        catch(...)
        {
            continue; // skip unparseable responses
        }

        if(s88Callback)
        {
            EventLoop::call(
                [this, contact, state]()
                {
                    if(s88Callback)
                        s88Callback(contact, state);
                });
        }
    }
}
