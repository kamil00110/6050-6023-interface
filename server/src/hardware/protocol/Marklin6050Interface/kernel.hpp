/**
 * server/src/hardware/protocol/Marklin6050Interface/kernel.hpp
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <string>
#include <cstdint>
#include <thread>
#include <functional>
#include <atomic>

#include "protocol.hpp"
#include "../../output/outputvalue.hpp"

namespace Marklin6050 {

class Kernel
{
public:
    Kernel(const std::string& port, unsigned int baudrate = 2400);
    ~Kernel();

    // --- Serial I/O ---
    bool sendByte(unsigned char byte);
    int readByte();

    // --- Lifecycle ---
    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }

    // --- Configuration ---
    void setRedundancy(unsigned int count);

    // --- Loco commands ---
    void setLocoSpeed(uint8_t address, uint8_t speed, bool f0);
    void setLocoDirection(uint8_t address, bool f0);
    void setLocoEmergencyStop(uint8_t address, bool f0);
    void setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0);
    void setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4);

    // --- Accessory commands ---
    bool setAccessory(uint32_t address, OutputValue value, unsigned int timeMs);

    // --- S88 input polling ---
    void startInputThread(unsigned int moduleCount, unsigned int intervalMs);
    void stopInputThread();

    // --- Callbacks ---
    std::function<void(uint32_t, bool)> s88Callback;

private:
    std::string m_port;
    unsigned int m_baudrate;
    unsigned int m_redundancy{0};
    std::thread m_inputThread;
    std::atomic<bool> m_running{false};
    bool m_isOpen{false};

    void sendCommand(uint8_t byte1, uint8_t byte2);
    void inputLoop(unsigned int modules);

#if defined(_WIN32)
    void* m_handle;
#else
    int m_fd;
#endif
};

} // namespace Marklin6050
