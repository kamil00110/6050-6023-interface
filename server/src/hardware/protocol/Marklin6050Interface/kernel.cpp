/**
 * server/src/hardware/protocol/Marklin6050Interface/kernel.cpp
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
        // ignore close errors
    }
}

// --- Serial I/O ---

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

// --- Internal helpers ---

void Kernel::sendCommand(uint8_t byte1, uint8_t byte2)
{
    sendByte(byte1);
    sendByte(byte2);
}

// --- Loco commands ---

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = speed & LocoSpeedMask;
    if(f0)
        cmd |= LocoF0Bit;

    sendCommand(cmd, address);

    if(m_config.redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_config.redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_serialPort.is_open()) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

void Kernel::setLocoDirection(uint8_t address, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = LocoDirToggle;
    if(f0)
        cmd |= LocoF0Bit;

    // no redundancy — toggling twice cancels out
    sendCommand(cmd, address);
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = LocoDirToggle;
    if(f0)
        cmd |= LocoF0Bit;

    // first toggle: triggers emergency stop
    sendCommand(cmd, address);

    // second toggle: restores original direction
    std::thread([this, cmd, address]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(!m_serialPort.is_open()) return;
        sendCommand(cmd, address);
    }).detach();
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = currentSpeed & LocoSpeedMask;
    if(f0)
        cmd |= LocoF0Bit;

    sendCommand(cmd, address);

    if(m_config.redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_config.redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_serialPort.is_open()) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

void Kernel::setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4)
{
    if(!m_serialPort.is_open() || address < 1)
        return;

    uint8_t cmd = FunctionBase;
    if(f1) cmd |= FunctionF1;
    if(f2) cmd |= FunctionF2;
    if(f3) cmd |= FunctionF3;
    if(f4) cmd |= FunctionF4;

    sendCommand(cmd, address);

    if(m_config.redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_config.redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_serialPort.is_open()) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

// --- Accessory commands ---

bool Kernel::setAccessory(uint32_t address, OutputValue value, unsigned int timeMs)
{
    if(!m_serialPort.is_open() || address < 1 || address > 256)
        return false;

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

    sendCommand(cmd, addr);

    // deactivate after delay, then repeat full cycle if redundancy
    std::thread([this, cmd, addr, timeMs, count = m_config.redundancy]()
    {
        // first cycle: deactivate
        std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
        if(!m_serialPort.is_open()) return;
        sendCommand(AccessoryOff, addr);

        // redundant cycles: full activate → wait → deactivate
        for(unsigned int i = 0; i < count; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if(!m_serialPort.is_open()) return;
            sendCommand(cmd, addr);

            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            if(!m_serialPort.is_open()) return;
            sendCommand(AccessoryOff, addr);
        }
    }).detach();

    return true;
}

// --- S88 input polling ---

void Kernel::startInputThread(unsigned int moduleCount, unsigned int intervalMs)
{
    if(m_running)
        return;

    m_running = true;

    m_inputThread = std::thread([this, moduleCount, intervalMs]()
    {
        while(m_running)
        {
            inputLoop(moduleCount);
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

void Kernel::inputLoop(unsigned int modules)
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
                s88Callback(address, state);
        }
    }
}
