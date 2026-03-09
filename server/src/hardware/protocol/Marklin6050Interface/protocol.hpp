/**
 * server/src/hardware/protocol/Marklin6050Interface/protocol.hpp
 *
 * Protocol constants for the Märklin 6050/6023 serial interface.
 *
 * Binary protocol (6050): All commands are 2 bytes: command byte + address byte.
 * ASCII protocol (6023/6223): Commands are ASCII strings terminated with CR.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_PROTOCOL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_PROTOCOL_HPP

#include <cstdint>

namespace Marklin6050 {

enum class ProtocolMode : uint8_t
{
    Binary,  // 6050 binary protocol
    ASCII    // 6023/6223 ASCII protocol
};

// ============================================================
// Binary protocol (6050)
// ============================================================

// --- Loco speed + F0 ---
// Command byte: speed (bits 0-3) | F0 (bit 4)
// Address byte: loco address
constexpr uint8_t LocoSpeedMask    = 0x0F;  // bits 0-3: speed 0-14
constexpr uint8_t LocoF0Bit        = 0x10;  // bit 4: F0 (light) on/off
constexpr uint8_t LocoStop         = 0;     // speed step 0 = stop
constexpr uint8_t LocoSpeedMin     = 1;     // speed step 1
constexpr uint8_t LocoSpeedMax     = 14;    // speed step 14
constexpr uint8_t LocoDirToggle    = 15;    // direction toggle (not a speed)

// --- Accessory (turnout/output) ---
// Command byte: action, Address byte: accessory address 1-256
constexpr uint8_t AccessoryOff     = 32;    // deactivate solenoid
constexpr uint8_t AccessoryGreen   = 33;    // activate green/straight
constexpr uint8_t AccessoryRed     = 34;    // activate red/diverging

// --- Loco functions F1-F4 ---
// Command byte: FunctionBase | bitmask, Address byte: loco address
// bit 0 = F1, bit 1 = F2, bit 2 = F3, bit 3 = F4
constexpr uint8_t FunctionBase     = 64;    // 0x40
constexpr uint8_t FunctionF1       = 0x01;
constexpr uint8_t FunctionF2       = 0x02;
constexpr uint8_t FunctionF3       = 0x04;
constexpr uint8_t FunctionF4       = 0x08;

// --- Global commands (single byte, no address) ---
constexpr uint8_t GlobalGo         = 96;    // resume track power
constexpr uint8_t GlobalStop       = 97;    // cut track power (preserves state)

// --- S88 feedback polling ---
// Command byte: S88Base + module count (1-31)
// Response: 2 bytes per module (16 bits contact state)
constexpr uint8_t S88Base          = 128;   // 0x80

// ============================================================
// ASCII protocol (6023/6223)
// ============================================================

constexpr char AsciiCR = '\r';

// ASCII commands are built as strings:
//   "G\r"           - start/go
//   "S\r"           - shutdown/stop
//   "Q\r"           - switch to binary compatibility mode
//   "L x D\r"       - loco direction toggle
//   "L x S y\r"     - loco speed
//   "L x S y F z\r" - loco speed + F0
//   "M x G\r"       - turnout green/straight
//   "M x R\r"       - turnout red/diverging
//   "A x\r"         - read s88 module (16 contacts)
//   "C x\r"         - read single s88 contact

} // namespace Marklin6050

#endif
