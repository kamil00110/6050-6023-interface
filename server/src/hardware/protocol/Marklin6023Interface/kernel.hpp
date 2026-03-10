/**
 * server/src/hardware/protocol/Marklin6023/kernel.hpp
 *
 * Kernel for the Märklin 6023/6223 ASCII serial protocol.
 *
 * Design
 * ------
 *  - Derives from KernelBase (io_context ownership, strand, lifecycle).
 *  - Serial I/O is fully delegated to IOHandler (async, non-blocking).
 *  - S88 polling is contact-by-contact: a query command is sent, then
 *    the kernel waits for the response line before querying the next
 *    contact. The poll cycle restarts via steady_timer once all contacts
 *    have been queried.
 *  - Command redundancy is implemented via steady_timer (no threads).
 *  - All mutable state is accessed exclusively on the kernel strand.
 *  - Public command methods post work onto the strand → thread-safe.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_KERNEL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_KERNEL_HPP

#include "../kernelbase.hpp"
#include "config.hpp"
#include "../../output/outputvalue.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6023 {

class IOHandler;

class Kernel : public KernelBase
{
public:
    // -----------------------------------------------------------------------
    // Callbacks (set before start(); called on the EventLoop thread)
    // -----------------------------------------------------------------------
    std::function<void(uint32_t address, bool state)> s88Callback;

    // -----------------------------------------------------------------------
    // Construction / lifecycle
    // -----------------------------------------------------------------------

    /**
     * @param logId    Identifier used in log messages.
     * @param config   Immutable configuration snapshot.
     * @param device   Serial device path (e.g. "/dev/ttyUSB0").
     * @param baudrate Baud rate (typically 9600 for 6023/6223).
     *
     * The device and baudrate are stored here so that start() is parameterless,
     * consistent with the KernelBase contract.
     */
    Kernel(std::string logId, const Config& config,
           std::string device, uint32_t baudrate);

    /**
     * Create the IOHandler (opens the serial port) and begin operation.
     * Throws LogMessageException on serial-port errors.
     */
    void start();

    /** Gracefully stop all timers and close the port. */
    void stop();

    // -----------------------------------------------------------------------
    // Command API  (thread-safe)
    // -----------------------------------------------------------------------

    void sendGlobalGo();
    void sendGlobalStop();

    void setLocoSpeed(uint8_t address, uint8_t speed, bool f0);
    void setLocoDirection(uint8_t address, bool f0);
    void setLocoEmergencyStop(uint8_t address, bool f0);
    void setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0);

    bool setAccessory(uint32_t address, OutputValue value);

    // -----------------------------------------------------------------------
    // IOHandler entry points  (called on the strand)
    // -----------------------------------------------------------------------
    void receiveLine(std::string line);
    void onReadError(const boost::system::error_code& ec);
    void onWriteError(const boost::system::error_code& ec);

private:
    // -----------------------------------------------------------------------
    // Internal helpers  (must be on the strand)
    // -----------------------------------------------------------------------

    void sendCmd(std::string cmd);                ///< sends cmd + CR
    void sendCmdWithRedundancy(std::string cmd);  ///< sends + schedules retransmits

    // -----------------------------------------------------------------------
    // S88 polling state machine
    // -----------------------------------------------------------------------
    void startS88Cycle();                         ///< kick off a new poll cycle
    void queryNextContact();                      ///< send "C <n>\r"
    void onS88Response(const std::string& line);  ///< handle "0" or "1" reply

    unsigned int m_s88NextContact = 1;            ///< 1-based contact being queried
    bool         m_s88WaitingReply = false;       ///< true while awaiting response

    boost::asio::steady_timer m_s88Timer;

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    const Config                 m_config;
    const std::string            m_device;
    const uint32_t               m_baudrate;
    std::unique_ptr<IOHandler>   m_ioHandler;

    std::vector<boost::asio::steady_timer> m_redundancyTimers;
};

} // namespace Marklin6023

#endif
