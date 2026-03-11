/**
 * server/src/hardware/protocol/Marklin6023Interface/iohandler.hpp
 *
 * Abstract IOHandler base for the Märklin 6023/6223 kernel.
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_IOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_IOHANDLER_HPP

#include <string>

namespace Marklin6023 {

class Kernel;

class IOHandler
{
public:
  explicit IOHandler(Kernel& kernel) : m_kernel{kernel} {}
  virtual ~IOHandler() = default;

  virtual void start() = 0;
  virtual void stop() = 0;

  /** Send a fully-formed command string including the CR terminator. */
  virtual void sendString(std::string str) = 0;

protected:
  Kernel& m_kernel;
};

} // namespace Marklin6023

#endif
