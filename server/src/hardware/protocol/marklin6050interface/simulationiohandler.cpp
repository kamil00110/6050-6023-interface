/**
 * server/src/hardware/protocol/Marklin6050Interface/simulationiohandler.cpp
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
#include "protocol.hpp"

namespace Marklin6050 {

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

void SimulationIOHandler::send(std::initializer_list<uint8_t> bytes)
{
  if(bytes.size() != 1)
    return; // only single-byte commands trigger a response

  const uint8_t b = *bytes.begin();

  if(b >= S88Base)
  {
    // S88 poll: respond with (moduleCount * 2) zero bytes — all contacts clear
    const uint8_t moduleCount = b - S88Base;
    m_strand.post(
      [this, moduleCount]()
      {
        for(uint8_t i = 0; i < moduleCount * 2u; ++i)
          m_kernel.receive(0x00);
      });
  }
  // All other single-byte commands (GlobalGo, GlobalStop, Extension poll)
  // are silently consumed — no response needed.
}

} // namespace Marklin6050
