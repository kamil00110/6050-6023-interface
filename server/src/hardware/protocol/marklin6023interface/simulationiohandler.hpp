/**
 * server/src/hardware/protocol/Marklin6023Interface/simulationiohandler.hpp
 *
 * Simulation IOHandler for the Märklin 6023/6223 kernel.
 * Responds to S88 contact queries with "0" (clear) so the kernel's
 * S88 polling cycle runs normally without real hardware.
 * All loco and accessory commands are silently consumed.
 *
 * Copyright (C) 2025
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_SIMULATIONIOHANDLER_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6023_SIMULATIONIOHANDLER_HPP

#include "iohandler.hpp"

#include <boost/asio/strand.hpp>

namespace Marklin6023 {

class SimulationIOHandler final : public IOHandler
{
public:
  SimulationIOHandler(Kernel& kernel,
                      boost::asio::io_context::strand& strand);

  void start() final;
  void stop() final;
  void sendString(std::string str) final;

private:
  boost::asio::io_context::strand& m_strand;
};

} // namespace Marklin6023

#endif
