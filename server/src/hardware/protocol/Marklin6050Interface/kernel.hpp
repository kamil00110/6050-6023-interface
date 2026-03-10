/**
 * server/src/hardware/protocol/Marklin6050/kernel.hpp
 *
 * Kernel for the Märklin 6050/6051 binary serial protocol.
 *
 * Design
 * ------
 *  - Derives from KernelBase (io_context ownership, strand, lifecycle).
 *  - Serial I/O is fully delegated to IOHandler (async, non-blocking).
 *  - Command redundancy is implemented via Boost.Asio steady_timer
 *    (no detached threads).
 *  - S88 polling and Extension polling each use a recurring steady_timer.
 *  - All state is accessed exclusively on the kernel strand → thread-safe.
 *  - Public command methods (setLocoSpeed, etc.) are safe to call from
 *    any thread; they post work onto the strand internally.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_KERNEL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_KERNEL_HPP

#include "../kernelbase.hpp"
#include "config.hpp"
#include "protocol.hpp"
#include "../../output/outputvalue.hpp"

#include <functional>
#include <memory>
#include <vector>
#include <cstdint>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace Marklin6050 {

class IOHandler;

class Kernel : public KernelBase
{
public:
    // -----------------------------------------------------------------------
    // Callbacks (set before start(); called on the EventLoop thread)
    // -----------------------------------------------------------------------
    std::function<void(uint32_t address, bool state)>                           s88Callback;
    std::function<void(bool power, bool run)>                                   extensionGlobalCallback;
    std::function<void(uint32_t address, bool green)>                           extensionTurnoutCallback;
    std::function<void(uint8_t address, uint8_t speed, bool f0, bool forward)>  extensionLocoCallback;
    std::function<void(uint8_t address, bool f1, bool f2, bool f3, bool f4)>    extensionFuncCallback;

    // -----------------------------------------------------------------------
    // Construction / lifecycle
    // -----------------------------------------------------------------------

    /**
     * @param logId   Identifier used in log messages.
     * @param config  Immutable configuration snapshot.
     */
    explicit Kernel(std::string logId, const Config& config);

    /**
     * Open the serial port and begin operation.
     * Throws LogMessageException on serial-port errors.
     */
    void start(const std::string& device, uint32_t baudrate);

    /** Gracefully stop all timers and close the port. */
    void stop();

    // -----------------------------------------------------------------------
    // Command API  (thread-safe – may be called from any thread)
    // -----------------------------------------------------------------------

    void sendGlobalGo();
    void sendGlobalStop();

    void setLocoSpeed(uint8_t address, uint8_t speed, bool f0);
    void setLocoDirection(uint8_t address, bool f0);
    void setLocoEmergencyStop(uint8_t address, bool f0);
    void setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0);
    void setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4);

    bool setAccessory(uint32_t address, OutputValue value, unsigned int timeMs);

    // -----------------------------------------------------------------------
    // IOHandler entry points  (called on the strand by IOHandler)
    // -----------------------------------------------------------------------
    void receive(uint8_t byte);
    void onReadError(const boost::system::error_code& ec);
    void onWriteError(const boost::system::error_code& ec);

private:
    // -----------------------------------------------------------------------
    // Internal helpers  (must be called on the strand)
    // -----------------------------------------------------------------------

    // Low-level send – goes straight to IOHandler
    void sendRaw(uint8_t b1, uint8_t b2);
    void sendRaw(uint8_t b);

    // Redundant send:  sends immediately, schedules (config.redundancy) retransmits
    void sendWithRedundancy(uint8_t b);
    void sendWithRedundancy(uint8_t b1, uint8_t b2);

    // -----------------------------------------------------------------------
    // S88 polling
    // -----------------------------------------------------------------------
    void scheduleS88Poll();
    void doS88Poll();

    // S88 binary-protocol receive state machine
    enum class S88State { Idle, ReceivingData };
    S88State     m_s88State   = S88State::Idle;
    unsigned int m_s88Expect  = 0;   ///< bytes still expected for current poll
    unsigned int m_s88Module  = 0;   ///< module index being received
    uint8_t      m_s88High    = 0;   ///< high byte of current 16-bit word

    // -----------------------------------------------------------------------
    // Extension polling
    // -----------------------------------------------------------------------
    void scheduleExtensionPoll();
    void doExtensionPoll();

    // Extension receive state machine
    enum class ExtState
    {
        Idle,
        WaitCount,
        WaitType,
        GlobalData,
        TurnoutAddr, TurnoutState,
        LocoStateAddr, LocoStateData,
        LocoFuncAddr,  LocoFuncData
    };
    ExtState    m_extState        = ExtState::Idle;
    uint8_t     m_extEventsLeft   = 0;
    uint8_t     m_extTmpAddr      = 0;

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    const Config                          m_config;
    std::unique_ptr<IOHandler>            m_ioHandler;

    boost::asio::steady_timer             m_s88Timer;
    boost::asio::steady_timer             m_extensionTimer;

    // For redundancy retransmit scheduling
    std::vector<boost::asio::steady_timer> m_redundancyTimers;
};

} // namespace Marklin6050

#endif
