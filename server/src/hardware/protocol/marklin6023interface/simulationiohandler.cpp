/**
 * server/src/hardware/protocol/Marklin6023Interface/simulationiohandler.cpp
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "simulationiohandler.hpp"
#include "kernel.hpp"

namespace Marklin6023 {

SimulationIOHandler::SimulationIOHandler(Kernel& kernel,
                                         boost::asio::io_context::strand& strand)
  : IOHandler{kernel}
  , m_strand{strand}
{
}

void SimulationIOHandler::start()
{
  m_kernel.started(); // immediately ready — no hardware to open
}

void SimulationIOHandler::stop()
{
  // nothing to close
}

void SimulationIOHandler::sendString(std::string str)
{
  // Parse just enough to respond to S88 contact queries.
  // Format: "C <n>\r"
  if(str.size() >= 3 && str[0] == 'C' && str[1] == ' ')
  {
    // Reply "0\r" (contact clear) on the strand so receiveLine() is called
    // with correct thread affinity.
    m_strand.post(
      [this]()
      {
        m_kernel.receiveLine("0");
      });
  }
  // All other commands (G, S, L ..., M ...) are silently consumed.
}

} // namespace Marklin6023
