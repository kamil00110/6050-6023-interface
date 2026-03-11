/**
 * server/src/hardware/protocol/Marklin6050Interface/iohandler.hpp
 *
 * Abstract IOHandler base for the Märklin 6050/6051 kernel.
 * Concrete subclasses: SerialIOHandler (real hardware) and
 * SimulationIOHandler (no serial port required).
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_IOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_IOHANDLER_HPP

#include <initializer_list>
#include <cstdint>

namespace Marklin6050 {

class Kernel;

class IOHandler
{
public:
  explicit IOHandler(Kernel& kernel) : m_kernel{kernel} {}
  virtual ~IOHandler() = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void send(std::initializer_list<uint8_t> bytes) = 0;

protected:
  Kernel& m_kernel;
};

} // namespace Marklin6050

#endif
