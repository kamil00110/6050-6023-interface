/**
 * server/src/hardware/protocol/Marklin6050Interface/simulationiohandler.hpp
 *
 * Simulation IOHandler for the Märklin 6050/6051 kernel.
 * Responds to S88 poll bytes with all-zero (clear) module data.
 * All loco/accessory commands are silently accepted.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_SIMULATIONIOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_SIMULATIONIOHANDLER_HPP

#include "iohandler.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

namespace Marklin6050 {

class SimulationIOHandler final : public IOHandler
{
public:
  SimulationIOHandler(Kernel& kernel,
                      boost::asio::io_context::strand& strand);

  void start() final;
  void stop() final;
  void send(std::initializer_list<uint8_t> bytes) final;

private:
  boost::asio::io_context::strand& m_strand;
};

} // namespace Marklin6050

#endif
