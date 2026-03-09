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

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

#include <thread>
#include <chrono>
#include <vector>

#if defined(_WIN32)
static void dbg(const char* msg)
{
    PWSTR path = nullptr;
    if(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path) != S_OK)
        return;

    char utf8Path[MAX_PATH * 4];
    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
    CoTaskMemFree(path);

    if(len <= 0)
        return;

    std::string full = std::string(utf8Path) + "\\kernel_debug.txt";
    FILE* f = fopen(full.c_str(), "a");
    if(!f)
        return;

    fprintf(f, "%s\n", msg);
    fclose(f);
}
#else
static void dbg(const char*) {}
#endif

using namespace Marklin6050;

// --- Construction / Destruction ---

Kernel::Kernel(const std::string& port, unsigned int baudrate)
    : m_port(port)
    , m_baudrate(baudrate)
{
#if defined(_WIN32)
    m_handle = INVALID_HANDLE_VALUE;
#else
    m_fd = -1;
#endif
}

Kernel::~Kernel()
{
    stop();
}

// --- Lifecycle ---

bool Kernel::start()
{
#if defined(_WIN32)
    dbg("Kernel::start - opening port");

    std::string devicePath = "\\\\.\\" + m_port;
    m_handle = CreateFileA(
        devicePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if(m_handle == INVALID_HANDLE_VALUE)
    {
        dbg("Kernel::start - INVALID_HANDLE");
        return false;
    }

    dbg("Kernel::start - handle OK");

    DCB dcb{};
    if(!GetCommState(m_handle, &dcb))
        return false;

    dcb.BaudRate = m_baudrate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if(!SetCommState(m_handle, &dcb))
        return false;

    m_isOpen = true;
    return true;

#else
    m_fd = open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if(m_fd < 0)
        return false;

    termios options{};
    tcgetattr(m_fd, &options);

    speed_t speed;
    switch(m_baudrate)
    {
        case 1200:   speed = B1200;   break;
        case 2400:   speed = B2400;   break;
        case 4800:   speed = B4800;   break;
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:     speed = B2400;   break;
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    tcsetattr(m_fd, TCSANOW, &options);

    m_isOpen = true;
    return true;
#endif
}

void Kernel::stop()
{
    if(!m_isOpen)
        return;

#if defined(_WIN32)
    if(m_handle != INVALID_HANDLE_VALUE)
        CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
#else
    if(m_fd >= 0)
        close(m_fd);
    m_fd = -1;
#endif

    m_isOpen = false;
}

// --- Serial I/O ---

bool Kernel::sendByte(unsigned char byte)
{
    if(!m_isOpen)
    {
        dbg("sendByte - port not open!");
        return false;
    }

#if defined(_WIN32)
    char buf[64];
    sprintf(buf, "sendByte: %u", byte);
    dbg(buf);
    DWORD written = 0;
    WriteFile(m_handle, &byte, 1, &written, nullptr);
    return written == 1;
#else
    return write(m_fd, &byte, 1) == 1;
#endif
}

int Kernel::readByte()
{
#if defined(_WIN32)
    unsigned char b;
    DWORD read = 0;
    if(ReadFile(m_handle, &b, 1, &read, nullptr) && read == 1)
    {
        char buf[64];
        sprintf(buf, "readByte: %u", b);
        dbg(buf);
        return b;
    }
    dbg("readByte: FAILED");
    return -1;
#else
    unsigned char b;
    int r = ::read(m_fd, &b, 1);
    return (r == 1) ? b : -1;
#endif
}

// --- Internal helpers ---

void Kernel::sendCommand(uint8_t byte1, uint8_t byte2)
{
    sendByte(byte1);
    sendByte(byte2);
}

void Kernel::setRedundancy(unsigned int count)
{
    m_redundancy = count;
}

// --- Loco commands ---

void Kernel::setLocoSpeed(uint8_t address, uint8_t speed, bool f0)
{
    if(!m_isOpen || address < 1)
        return;

    uint8_t cmd = speed & LocoSpeedMask;
    if(f0)
        cmd |= LocoF0Bit;

    sendCommand(cmd, address);

    if(m_redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_isOpen) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

void Kernel::setLocoDirection(uint8_t address, bool f0)
{
    if(!m_isOpen || address < 1)
        return;

    uint8_t cmd = LocoDirToggle;
    if(f0)
        cmd |= LocoF0Bit;

    // no redundancy — toggling twice cancels out
    sendCommand(cmd, address);
}

void Kernel::setLocoEmergencyStop(uint8_t address, bool f0)
{
    if(!m_isOpen || address < 1)
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
        if(!m_isOpen) return;
        sendCommand(cmd, address);
    }).detach();
}

void Kernel::setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0)
{
    if(!m_isOpen || address < 1)
        return;

    uint8_t cmd = currentSpeed & LocoSpeedMask;
    if(f0)
        cmd |= LocoF0Bit;

    sendCommand(cmd, address);

    if(m_redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_isOpen) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

void Kernel::setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4)
{
    if(!m_isOpen || address < 1)
        return;

    uint8_t cmd = FunctionBase;
    if(f1) cmd |= FunctionF1;
    if(f2) cmd |= FunctionF2;
    if(f3) cmd |= FunctionF3;
    if(f4) cmd |= FunctionF4;

    sendCommand(cmd, address);

    if(m_redundancy > 0)
    {
        std::thread([this, cmd, address, count = m_redundancy]()
        {
            for(unsigned int i = 0; i < count; i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                if(!m_isOpen) return;
                sendCommand(cmd, address);
            }
        }).detach();
    }
}

// --- Accessory commands ---

bool Kernel::setAccessory(uint32_t address, OutputValue value, unsigned int timeMs)
{
    if(!m_isOpen || address < 1 || address > 256)
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
    std::thread([this, cmd, addr, timeMs, count = m_redundancy]()
    {
        // first cycle: deactivate
        std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
        if(!m_isOpen) return;
        sendCommand(AccessoryOff, addr);

        // redundant cycles: full activate → wait → deactivate
        for(unsigned int i = 0; i < count; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if(!m_isOpen) return;
            sendCommand(cmd, addr);

            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            if(!m_isOpen) return;
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
    if(!m_running || !m_isOpen || modules == 0)
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
