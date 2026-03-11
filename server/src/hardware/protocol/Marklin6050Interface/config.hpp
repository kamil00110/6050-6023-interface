/**
 * server/src/hardware/protocol/Marklin6050/config.hpp
 *
 * Runtime configuration snapshot for the Märklin 6050/6051 binary kernel.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_CONFIG_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_CONFIG_HPP

#include <cstdint>

namespace Marklin6050 {

struct Config
{
    uint16_t    centralUnitVersion = 6020;
    bool        analog             = false;
    unsigned int s88amount         = 1;
    unsigned int s88interval       = 400;   ///< milliseconds between S88 polls
    unsigned int turnouttime       = 200;   ///< milliseconds solenoid on-time
    unsigned int redundancy        = 0;     ///< extra retransmit count (0 = send once)
    bool        extensions         = false; ///< enable extension event polling
    bool        debugLogRXTX       = false; ///< log every TX/RX byte to the debug log
};

} // namespace Marklin6050

#endif
