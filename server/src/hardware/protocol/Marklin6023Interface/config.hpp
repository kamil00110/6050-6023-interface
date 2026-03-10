/**
 * server/src/hardware/protocol/Marklin6023/config.hpp
 *
 * Runtime configuration snapshot for the Märklin 6023/6223 ASCII kernel.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_CONFIG_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_CONFIG_HPP

#include <cstdint>

namespace Marklin6023 {

struct Config
{
    unsigned int s88amount   = 1;   ///< number of S88 modules (max 4 for 6023/6223)
    unsigned int s88interval = 400; ///< milliseconds between S88 poll cycles
    unsigned int redundancy  = 0;   ///< extra retransmit count (0 = send once)
};

} // namespace Marklin6023

#endif
